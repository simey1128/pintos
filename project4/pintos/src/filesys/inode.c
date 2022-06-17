#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/buffer-cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    block_sector_t direct_blocks[DIRECT_BLOCKS_SIZE];
    block_sector_t single_indirect_block;
    block_sector_t double_indirect_block;
  };

static void set_inode_disk(struct inode *inode, struct inode_disk *inode_disk);
static void calc_ofs(off_t file_size, struct block_location *block_loc);
static void free_inode_disk_blocks(struct inode_disk* inode_disk);
static bool file_growth(struct inode_disk* inode_disk, off_t start_loc, off_t end_loc);
/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    // struct inode_disk data;             /* Inode content. */
    struct lock io_lock;
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (struct inode_disk *inode_disk, off_t pos) 
{
  if(pos >= inode_disk->length){
    return -1;
  }

  block_sector_t *tmp_block;
  tmp_block = malloc(BLOCK_SECTOR_SIZE);
  block_sector_t return_sector;
  struct block_location *block_loc = malloc(sizeof *block_loc);
  calc_ofs(pos, block_loc);
  switch(block_loc->direct_status){
    case DIRECT:
      return_sector = inode_disk->direct_blocks[block_loc->direct_ofs];
      break;
    case SINGLE_INDIRECT:
      read_buffer_cache(inode_disk->single_indirect_block, tmp_block, 0, BLOCK_SECTOR_SIZE, 0);
      return_sector = *(tmp_block + block_loc->single_indirect_ofs);
      break;
    case DOUBLE_INDIRECT:
      read_buffer_cache(inode_disk->double_indirect_block, tmp_block, 0, BLOCK_SECTOR_SIZE, 0);
      block_sector_t tmp_idx = *(tmp_block + block_loc->double_indirect_ofs);
      read_buffer_cache(tmp_idx, tmp_block, 0, BLOCK_SECTOR_SIZE, 0);
      return_sector = *(tmp_block + block_loc->single_indirect_ofs);
      break;
    default:
      return_sector = -1;
  }
  free(tmp_block);
  free(block_loc);
  return return_sector;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *inode_disk = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *inode_disk == BLOCK_SECTOR_SIZE);

  inode_disk = calloc (1, sizeof *inode_disk);
  if (inode_disk != NULL)
    {
      
      inode_disk->magic = INODE_MAGIC;
      // inode_disk->length = length;
      if(file_growth(inode_disk, 0, length)){
        write_buffer_cache(sector, inode_disk, 0, BLOCK_SECTOR_SIZE ,0);
        success = true;
      }
      free (inode_disk);
      
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  // block_read (fs_device, inode->sector, &inode->data);
  lock_init(&inode->io_lock);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          struct inode_disk* inode_disk = malloc(sizeof(*inode_disk));
          set_inode_disk(inode, inode_disk);
          free_inode_disk_blocks(inode_disk);
          free_map_release(inode->sector, 1);
          free(inode_disk);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
  
  struct inode_disk* inode_disk = malloc(sizeof(*inode_disk));
  set_inode_disk(inode, inode_disk);

  while (size > 0) 
    {
      // printf("buffer: %p\n", buffer);
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode_disk, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_disk->length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      
      if (chunk_size <= 0)
        {
          break;
        }
      read_buffer_cache(sector_idx, buffer, bytes_read, chunk_size, sector_ofs);
      // printf("read!!!!!!\n");
      // printf("sector_idx: %d, buffer: %p, bytes_read: %d, sector_ofs: %d\n", sector_idx, buffer, bytes_read, sector_ofs);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  struct inode_disk* inode_disk = malloc(sizeof(*inode_disk));
  set_inode_disk(inode, inode_disk);


  lock_acquire(&inode->io_lock);
  int prev_length = inode_disk->length;
  int end_loc = offset + size;
  if(end_loc -1 > prev_length -1){
    file_growth(inode_disk, prev_length, offset+size);
  }
  lock_release(&inode->io_lock);

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode_disk, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_disk->length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      write_buffer_cache(sector_idx, buffer, bytes_written, chunk_size, sector_ofs);
      // printf("write!!!\n");
      // printf("sector_idx: %d, buffer: %p, bytes_written: %d, sector_ofs: %d\n", sector_idx, buffer, bytes_written, sector_ofs);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  write_buffer_cache(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk* inode_disk = malloc(sizeof(*inode_disk));
  read_buffer_cache(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
  
  off_t length = inode_disk->length;
  free(inode_disk);
  return length;
}

static void set_inode_disk(struct inode *inode, struct inode_disk *inode_disk){
  read_buffer_cache(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
}

/*
파일의 크기에 맞게 index block안에서 ofs을 계산하여 update함
*/
static void calc_ofs(off_t file_size, struct block_location *block_loc){
  block_sector_t file_sec = ((file_size) / BLOCK_SECTOR_SIZE);
  if(file_sec < DIRECT_BLOCKS_SIZE){
    block_loc -> direct_status = DIRECT;
    block_loc -> direct_ofs = file_sec;

  }else if(file_sec < (DIRECT_BLOCKS_SIZE + INDIRECT_BLOCKS_SIZE)){
    block_loc -> direct_status = SINGLE_INDIRECT;
    block_loc -> single_indirect_ofs = file_sec - DIRECT_BLOCKS_SIZE;
  }else{
    block_loc -> direct_status = DOUBLE_INDIRECT;
    // single_indirect_ofs: block을 가리킴
    block_loc -> single_indirect_ofs = file_sec % INDIRECT_BLOCKS_SIZE;
    // double_indirect_ofs: block table을 가리킴
    block_loc -> double_indirect_ofs = (file_sec - block_loc->single_indirect_ofs) / INDIRECT_BLOCKS_SIZE;
  }
}

/*
새로 할당받은 block을 inode_disk에 추가하기
*/
static bool inode_disk_growth(struct inode_disk *inode_disk, block_sector_t new_block, struct block_location *block_loc){
  block_sector_t *tmp_block;
  tmp_block = malloc(BLOCK_SECTOR_SIZE);
  
  switch(block_loc->direct_status){
    case DIRECT:
      inode_disk -> direct_blocks[block_loc->direct_ofs] = new_block;
      break;
    case SINGLE_INDIRECT:
      read_buffer_cache(inode_disk->single_indirect_block, tmp_block, 0, BLOCK_SECTOR_SIZE, 0);
      *(tmp_block + block_loc->single_indirect_ofs) = new_block;
      write_buffer_cache(inode_disk->single_indirect_block, tmp_block, 0, BLOCK_SECTOR_SIZE, 0);
      break;
    case DOUBLE_INDIRECT:
      read_buffer_cache(inode_disk->double_indirect_block, tmp_block, 0, BLOCK_SECTOR_SIZE, 0);
      block_sector_t tmp_idx = *(tmp_block + block_loc->double_indirect_ofs);
      read_buffer_cache(tmp_idx, tmp_block, 0, BLOCK_SECTOR_SIZE, 0);
      *(tmp_block + block_loc->single_indirect_ofs) = new_block;
      write_buffer_cache(tmp_idx, tmp_block, 0, BLOCK_SECTOR_SIZE, 0);
      break;
    default:{
      return false;
    }
  }
  free(tmp_block);
  return true;
}


/*
파일이 growth한 것을 inode에 반영하기
*/
static bool file_growth(struct inode_disk* inode_disk, off_t start_loc, off_t end_loc){
  block_sector_t existing_sector = byte_to_sector(inode_disk, start_loc);
  int size = end_loc - start_loc;
  inode_disk->length += size;
  int offset = start_loc;

  static char zeros[BLOCK_SECTOR_SIZE];
  // memset(zeros, 0, BLOCK_SECTOR_SIZE);

  if(existing_sector != -1){
    
    // 기존에 존재하는 block에서 남은부분 채우고
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;
    int temp_size = BLOCK_SECTOR_SIZE - sector_ofs;

    write_buffer_cache(existing_sector, zeros, 0, temp_size, sector_ofs);
    size -= temp_size;
    offset += temp_size;
  }
  while(size > 0){
    
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;
    int temp_size = size < BLOCK_SECTOR_SIZE ? size : BLOCK_SECTOR_SIZE;

    block_sector_t new_block;
    if(free_map_allocate(1, &new_block)){
      struct block_location *block_loc = malloc(sizeof *block_loc);
      calc_ofs(offset + temp_size - BLOCK_SECTOR_SIZE, block_loc);
      inode_disk_growth(inode_disk, new_block, block_loc);
      free(block_loc);
    }else{
      return false;
    }
    write_buffer_cache(new_block, zeros, 0, temp_size, 0);

    size -= temp_size;
    offset += temp_size;
  }

  return true;
}

static void free_inode_disk_blocks(struct inode_disk* inode_disk){
  int i=0;

  //free direct blocks
  while(i<DIRECT_BLOCKS_SIZE){
    block_sector_t sector = inode_disk->direct_blocks[i];
    if(sector <= 0) break;

    free_map_release(sector, 1);
    i++;
  }

  //free single indirect blocks
  if(inode_disk->single_indirect_block > 0){
    block_sector_t* block_table;
    block_table = malloc(BLOCK_SECTOR_SIZE);

    read_buffer_cache(inode_disk->single_indirect_block, block_table, 0, BLOCK_SECTOR_SIZE, 0);
    i=0;
    while(i<INDIRECT_BLOCKS_SIZE){
      block_sector_t sector = *(block_table + i);
      if(sector <= 0) break;

      free_map_release(sector, 1);
      i++;
    }

    free(block_table);    
  }

  // free double indirect blocks
  if(inode_disk->double_indirect_block > 0){
    block_sector_t* outer_table;
    block_sector_t* inner_table;
    outer_table = malloc(BLOCK_SECTOR_SIZE);
    inner_table = malloc(BLOCK_SECTOR_SIZE);

    read_buffer_cache(inode_disk->double_indirect_block, outer_table, 0, BLOCK_SECTOR_SIZE, 0);

    i=0;
    while(i<INDIRECT_BLOCKS_SIZE){
      block_sector_t table_sector = *(outer_table + i);
      if(table_sector <= 0) break;

      read_buffer_cache(table_sector, inner_table, 0, BLOCK_SECTOR_SIZE, 0);
      int j=0;
      while(j<INDIRECT_BLOCKS_SIZE){
        block_sector_t sector = *(inner_table+j);
        if(sector<= 0) break;

        free_map_release(sector, 1);
        j++;
      }

      free_map_release(table_sector, 1);
      i++;
    }

    free(outer_table);
    free(inner_table);
  }
}

