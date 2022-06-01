#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <stdbool.h>
#include <string.h>
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "debug.h"
#include "userprog/pagedir.h"
#include "filesys/inode.h"
#include "userprog/exception.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"


#define NOT_REACHED() PANIC ("syscall, frame overflow");
static bool
install_page (void *upage, void *kpage, bool writable);
static int
get_user (const uint8_t *uaddr);
static bool check_addr(uint8_t*);
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[125];               /* Not used. */
  };
struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };

struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf("f->esp: %p\n", f->esp);
  if((f -> esp)>=PHYS_BASE-4) exit(-1);
  switch(*(uint8_t *)(f -> esp)){
    case SYS_HALT:
      halt();
      break;

    case SYS_EXIT:
      exit(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_EXEC:
      f->eax = exec(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_WAIT:
      f -> eax = wait(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_CREATE:
      f->eax = create(*(uint32_t *)(f -> esp + 16), *(unsigned *)(f -> esp + 20));
      break;

    case SYS_REMOVE:
      f -> eax = remove(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_OPEN:
      f -> eax = open(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_FILESIZE:
      f -> eax = filesize(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_READ:
      f -> eax = read(*(uint32_t *) (f -> esp + 20), *(uint32_t *)(f -> esp + 24), *(uint32_t *)(f -> esp + 28));
      break;

    case SYS_WRITE:
      f -> eax = write(*(uint32_t *) (f -> esp + 20), *(uint32_t *)(f -> esp + 24), *(uint32_t *)(f -> esp + 28));
      break;

    case SYS_SEEK:
      seek(*(uint32_t *)(f -> esp + 16), *(unsigned *)(f -> esp + 20));
      break;

    case SYS_TELL:
      f -> eax = tell(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_CLOSE:
      close(*(uint32_t *) (f -> esp + 4));
      break;
    
    case SYS_MMAP:
      f -> eax = mmap(*(uint32_t *)(f -> esp + 16), *(uint32_t *)(f -> esp + 20));
      break;
      
    case SYS_MUNMAP:
      munmap(*(uint32_t *)(f -> esp + 4));
      break;

    default: 
      exit(-1);
  }
}

static int
get_user (const uint8_t *uaddr)
{
  if(!is_user_vaddr(uaddr))
    return -1;
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

static bool check_addr(uint8_t* uaddr){
  if(get_user(uaddr) == -1) exit(-1);
}

static bool
install_page (void *upage, void *kpage, bool writable)
{
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (thread_current()->pagedir, upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, upage, kpage, writable));
}

void halt(){
  shutdown_power_off();
}

void exit(int status){
  // printf("tid: %d\n", thread_current() -> tid);
  thread_current()->exit_status = status;
  printf("%s: exit(%d)\n", thread_name(), status);

  struct file **fd_list = thread_current()->fd_list;
  int i;
  for(i=3; i<128; i++){
    if(fd_list[i] != NULL){
      munmap(i);
      close(i);
    }

  }
  
  thread_exit();
}

pid_t exec(const char *cmd_line){
  check_addr(cmd_line);
  return process_execute(cmd_line);
}

int wait(pid_t pid){
  return process_wait(pid);
}

int create(const char* file, unsigned initial_size){
  check_addr(file);
  if(strlen(file)>128) return 0; // userprog/create-long
  if(strlen(file) == 0) exit(-1); // userprog/create-empty

  return filesys_create(file, initial_size);
}

int remove(const char *file){
  check_addr(file);
  if(file == NULL){
    exit(-1);
  }
  return filesys_remove(file);
}

int open(const char* file){
  check_addr(file);
  lock_acquire(&filesys_lock);
  struct file* file_opened = filesys_open(file);
 
  if(file_opened == NULL) {
    lock_release(&filesys_lock);
    return -1;
  }
  
  int i, fd;
  for (i = 3; i < 128; i++) {
      if (thread_current()->fd_list[i] == NULL) {
        thread_current()->fd_list[i] = file_opened; 
        fd = i;
        break;
      }   
    }   
  lock_release(&filesys_lock);
  return fd;
}

int filesize(int fd){
  struct file *open_file = thread_current() -> fd_list[fd];
  if(open_file == NULL)
    return -1;
  return file_length(open_file);
}

int read(int fd, void *buffer, unsigned size){
  check_addr(buffer);
  check_addr(buffer+size-1);
  if(fd >= 128) return -1;
  int read_value;
  if(fd == 0){
    *(char *)buffer = input_getc();
    read_value = size;
  }else if(fd > 2){
    lock_acquire(&filesys_lock);
    read_value = file_read(thread_current() -> fd_list[fd], buffer, size);
    lock_release(&filesys_lock);
  }else
    read_value = -1;
  return read_value;
}

int write(int fd, const void *buffer, unsigned size){
  check_addr(buffer);
  check_addr(buffer+size-1);
  if(fd >= 128) return -1;
  int write_value;
  lock_acquire(&filesys_lock);
  if(fd == 1){
    putbuf(buffer, size);
    write_value = size;
  }else if(fd > 2){
    struct file* target = thread_current()->fd_list[fd];
    if(target==NULL) write_value = -1;

    write_value = file_write(target, buffer, size);
   
  }else
    write_value = -1;
  lock_release(&filesys_lock);
  return write_value;
}

void seek(int fd, unsigned position){
  file_seek(thread_current() -> fd_list[fd], position);
}

unsigned tell(int fd){
  file_tell(thread_current() -> fd_list[fd]);
}

void close(int fd){
  if(fd > 128) exit(-1); // userprog/close-bad-fd

  struct thread *cur_thread = thread_current();
  struct file *file = (cur_thread->fd_list)[fd];
  
  if(file == NULL) exit(-1);

  cur_thread->fd_list[fd] = NULL;
  return file_close(file);
}

mapid_t mmap(int fd, void *addr){
  if(addr == 0) return -1;
  if(addr > PHYS_BASE - 0x800000) return -1;
  if((uint32_t)addr % PGSIZE != 0) return -1;
  if(addr < (uint32_t*)0x08048000 + thread_current()->read_bytes) return -1;
  if(get_me(addr) != NULL) return -1;


  struct thread *t = thread_current();
  struct mmap_entry *me = malloc(sizeof(*me));
  me -> mapid = fd;
  me -> file = file_reopen(t -> fd_list[fd]);
  me -> file_size = me -> file -> inode -> data.length;
  me -> start_addr = addr;

  if(me -> file_size == 0) exit(-1);

  list_push_back(&t->mmap_table, &me->elem);

  return me->mapid;
}
void munmap(mapid_t mapid){
  struct list_elem* e = list_begin(&thread_current()->mmap_table);
  while(e!=list_end(&thread_current()->mmap_table)){
    struct mmap_entry* me = list_entry(e, struct mmap_entry, elem);
    if(me->mapid == mapid){
      lock_acquire(&filesys_lock);
      write_back(thread_current()->pagedir, me);
      lock_release(&filesys_lock);
      list_remove(&me->elem);
      // free(me);
    }
    e = e->next;
  }
}
