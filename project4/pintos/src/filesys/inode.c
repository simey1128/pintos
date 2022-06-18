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



enum access_type {
  DIRECT,
  SINGLE_INDIRECT,
  DOUBLE_INDIRECT
};

struct block_table_ofs{
  enum access_type access_type;
  off_t inner_table_ofs;
  off_t outer_table_ofs;
};

struct indirect_table{
  block_sector_t entries[INDIRECT_BLOCK_SIZE];
};

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;
    bool is_dir;
    block_sector_t direct_table[DIRECT_BLOCK_SIZE];
    block_sector_t single_indirect_table_sec; // inner table(data 바로 가리키는)이 들어있는 sector number
    block_sector_t double_indirect_table_sec; // outer table이 들어있는 sector number
  };

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
    struct lock io_lock;
  };

static bool set_inode_disk(struct inode* inode, struct inode_disk*inode_disk);
static void set_block_table_ofs(off_t pos, struct block_table_ofs* block_table_ofs);
static bool update_inode_disk(struct inode_disk* inode_disk, block_sector_t new_block, struct block_table_ofs* block_table_ofs);
static void free_inode_disk_blocks(struct inode_disk* inode_disk);
static bool file_growth(struct inode_disk* inode_disk, off_t start_loc, off_t end_loc);
/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode_disk *inode_disk, off_t pos) 
{
  block_sector_t result;
  if(pos < inode_disk->length){
    struct indirect_table *indirect_table;
    struct block_table_ofs* block_table_ofs = malloc(sizeof(*block_table_ofs));
    set_block_table_ofs(pos, block_table_ofs);

    switch(block_table_ofs->access_type){
      case DIRECT: 
        result = inode_disk->direct_table[block_table_ofs->inner_table_ofs];
        break;
      
      case SINGLE_INDIRECT:
        indirect_table = (struct indirect_table*)malloc(BLOCK_SECTOR_SIZE);
        if(indirect_table){
          read_buffer_cache(inode_disk->single_indirect_table_sec, indirect_table->entries, 0, BLOCK_SECTOR_SIZE, 0);
          result = indirect_table->entries[block_table_ofs->inner_table_ofs];
        }else {
          result = 0;
        }

        free(indirect_table);
        break;
      
      case DOUBLE_INDIRECT:
        indirect_table = (struct indirect_table*)malloc(BLOCK_SECTOR_SIZE);
        if(indirect_table){
          read_buffer_cache(inode_disk->double_indirect_table_sec, indirect_table, 0, BLOCK_SECTOR_SIZE, 0);
          read_buffer_cache(indirect_table->entries[block_table_ofs->outer_table_ofs], indirect_table, 0, BLOCK_SECTOR_SIZE, 0);
          result = indirect_table->entries[block_table_ofs->inner_table_ofs];
        }else result = 0;

        free(indirect_table);
        break;
      
      default:
        result = 0;
    }

    return result;
  }
  return 0;
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
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if(file_growth(disk_inode, 0, length)){
        write_buffer_cache(sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
        free (disk_inode);
        success = true;
      }
      
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
          struct inode_disk*inode_disk = (struct inode_disk*)malloc(BLOCK_SECTOR_SIZE);
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

  struct inode_disk*inode_disk = (struct inode_disk*)malloc(BLOCK_SECTOR_SIZE);
  set_inode_disk(inode, inode_disk);

  while (size > 0) 
    {
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
        break;


      read_buffer_cache(sector_idx, buffer, bytes_read, chunk_size, sector_ofs);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  free(inode_disk);
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

  struct inode_disk* inode_disk = (struct inode_disk*)malloc(BLOCK_SECTOR_SIZE);

  if(inode_disk == NULL) return 0;
  set_inode_disk(inode, inode_disk);

  lock_acquire(&inode->io_lock);
  int prev_length = inode_disk->length;
  int write_end = offset + size -1;
  if(write_end>prev_length -1){
    inode_disk->length = offset + size;
    file_growth(inode_disk, prev_length, offset+size);
    write_buffer_cache(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
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

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  // write_buffer_cache(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);

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
  struct inode_disk*inode_disk = (struct inode_disk*)malloc(BLOCK_SECTOR_SIZE);
  set_inode_disk(inode, inode_disk);

  off_t len = inode_disk->length;
  free(inode_disk);
  return len;
}


static bool set_inode_disk(struct inode* inode, struct inode_disk*inode_disk){
  read_buffer_cache(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);

  return true;
}

static void set_block_table_ofs(off_t pos, struct block_table_ofs* block_table_ofs){
  off_t sector = pos / BLOCK_SECTOR_SIZE;
  
  if(sector < DIRECT_BLOCK_SIZE){
    block_table_ofs->access_type = DIRECT;
    block_table_ofs->inner_table_ofs = sector % DIRECT_BLOCK_SIZE;
  }else if(sector < (off_t)(DIRECT_BLOCK_SIZE + INDIRECT_BLOCK_SIZE)){
    block_table_ofs->access_type = SINGLE_INDIRECT;
    block_table_ofs->inner_table_ofs = (sector - DIRECT_BLOCK_SIZE) % INDIRECT_BLOCK_SIZE;
  }else if(sector < (off_t)(DIRECT_BLOCK_SIZE + INDIRECT_BLOCK_SIZE*(INDIRECT_BLOCK_SIZE+1))){
    block_table_ofs->access_type = DOUBLE_INDIRECT;
    block_table_ofs->inner_table_ofs = (sector - DIRECT_BLOCK_SIZE - INDIRECT_BLOCK_SIZE) % INDIRECT_BLOCK_SIZE;
    block_table_ofs->outer_table_ofs = (sector - DIRECT_BLOCK_SIZE - INDIRECT_BLOCK_SIZE) / INDIRECT_BLOCK_SIZE;
  }else {
    PANIC("set_block_table_ofs error\n");
  }
}

static bool update_inode_disk(struct inode_disk* inode_disk, block_sector_t new_block, struct block_table_ofs* block_table_ofs){
  struct indirect_table* indirect_table;
  
  switch(block_table_ofs->access_type){
    case DIRECT:
      inode_disk->direct_table[block_table_ofs->inner_table_ofs] = new_block;
      break;

    case SINGLE_INDIRECT:
      indirect_table = (struct indirect_table*)malloc(BLOCK_SECTOR_SIZE);
      if(indirect_table == NULL) return false;
      read_buffer_cache(inode_disk->single_indirect_table_sec, indirect_table->entries, 0, BLOCK_SECTOR_SIZE, 0);
      indirect_table->entries[block_table_ofs->inner_table_ofs] = new_block;
      write_buffer_cache(inode_disk->single_indirect_table_sec, indirect_table->entries, 0, BLOCK_SECTOR_SIZE, 0);
      break;

    case DOUBLE_INDIRECT:
      indirect_table = (struct indirect_table*)malloc(BLOCK_SECTOR_SIZE);
      if(indirect_table == NULL) return false;

      read_buffer_cache(inode_disk->double_indirect_table_sec, indirect_table, 0, BLOCK_SECTOR_SIZE, 0);
      block_sector_t inner_table_sector = indirect_table->entries[block_table_ofs->outer_table_ofs];
      read_buffer_cache(inner_table_sector, indirect_table, 0, BLOCK_SECTOR_SIZE, 0);
      indirect_table->entries[block_table_ofs->inner_table_ofs] = new_block;
      write_buffer_cache(inner_table_sector, indirect_table, 0, BLOCK_SECTOR_SIZE, 0);
      break;

    default:
      return false;
  }
  free(indirect_table);
  return true;
}



static void free_inode_disk_blocks(struct inode_disk* inode_disk){
  int i, j;
  if(inode_disk->double_indirect_table_sec > 0){
    
    struct indirect_table* outer_table = (struct indirect_table*)malloc(BLOCK_SECTOR_SIZE);
    struct indirect_table* inner_table = (struct indirect_table*)malloc(BLOCK_SECTOR_SIZE);
    read_buffer_cache(inode_disk->double_indirect_table_sec, outer_table, 0, BLOCK_SECTOR_SIZE, 0);
    i=0;
    while(outer_table->entries[i]>0){
      j=0;
      read_buffer_cache(outer_table->entries[i], inner_table, 0, BLOCK_SECTOR_SIZE, 0);
      while(inner_table->entries[j]>0){
        free_map_release(inner_table->entries[j], 1);
        j++;
      }
      free(inner_table);
      i++;
    }
    free(outer_table);
  }

  if(inode_disk->single_indirect_table_sec > 0){
    struct indirect_table* indirect_table = (struct indirect_table*)malloc(BLOCK_SECTOR_SIZE);
    read_buffer_cache(inode_disk->single_indirect_table_sec, indirect_table, 0, BLOCK_SECTOR_SIZE, 0);
    i=0;
    while(indirect_table->entries[i]>0){
      free_map_release(indirect_table->entries[i], 1);
      i++;
    }
    free(indirect_table);
  }

  i=0;
  while(inode_disk->direct_table[i]>0){
    free_map_release(inode_disk->direct_table[i], 1);
    i++;
  }
}
static bool file_growth(struct inode_disk* inode_disk, off_t start_loc, off_t end_loc){
  off_t size = end_loc - start_loc;
  off_t location = start_loc;
  static char zeros[BLOCK_SECTOR_SIZE];

  while(size > 0){
    
    int sector_loc = location % BLOCK_SECTOR_SIZE;
    size_t chunk_size = size < BLOCK_SECTOR_SIZE ? size : BLOCK_SECTOR_SIZE;

    if(sector_loc > 0){
      block_sector_t existing_block = byte_to_sector(inode_disk, location);
      write_buffer_cache(existing_block, zeros, 0, BLOCK_SECTOR_SIZE - sector_loc, sector_loc);
      size -= BLOCK_SECTOR_SIZE - sector_loc;
      location += BLOCK_SECTOR_SIZE - sector_loc;
    }else {
      block_sector_t new_block;
      if(free_map_allocate(1, &new_block)){
        struct block_table_ofs *block_table_ofs = (struct block_table_ofs*)malloc(BLOCK_SECTOR_SIZE);
        set_block_table_ofs(location, block_table_ofs);

        if(block_table_ofs->access_type == SINGLE_INDIRECT && block_table_ofs->inner_table_ofs == 0){
          free_map_allocate(1, &inode_disk->single_indirect_table_sec);
        }
        update_inode_disk(inode_disk, new_block, block_table_ofs);
      }else{
        return false;
      }
      write_buffer_cache(new_block, zeros, 0, BLOCK_SECTOR_SIZE, 0);
      size -= chunk_size;
      location += chunk_size;
    }
    
  }
  // free(zeros);
  return true;
}