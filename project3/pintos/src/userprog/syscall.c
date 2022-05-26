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

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"


#define NOT_REACHED() PANIC ("frame overflow");

struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
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
  // printf("syscall num: %d, tid: %d\n", *(uint8_t *)(f -> esp), thread_current()->tid);
  check_addr((f->esp));
  switch(*(uint8_t *)(f -> esp)){
    case SYS_HALT:
      halt();
      break;

    case SYS_EXIT:
      check_addr(f -> esp + 4);
      exit(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_EXEC:
      check_addr(f -> esp + 4);
      f->eax = exec(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_WAIT:
      check_addr(f -> esp + 4);
      f -> eax = wait(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_CREATE:
      check_addr(f -> esp + 16);
      int success = create(*(uint32_t *)(f -> esp + 16), *(unsigned *)(f -> esp + 20));
      f->eax = success;
      break;

    case SYS_REMOVE:
      check_addr(f -> esp + 4);
      f -> eax = remove(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_OPEN:
      check_addr(f -> esp + 4);
      int fd = open(*(uint32_t *)(f -> esp + 4));
      f -> eax = fd;
      break;

    case SYS_FILESIZE:
      check_addr(f -> esp + 4);
      f -> eax = filesize(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_READ:
      check_addr(f -> esp + 20);
      check_addr(f -> esp + 24);
      check_addr(f -> esp + 28);
      f -> eax = read(*(uint32_t *) (f -> esp + 20), *(uint32_t *)(f -> esp + 24), *(uint32_t *)(f -> esp + 28));

      break;

    case SYS_WRITE:
      check_addr(f -> esp + 20);
      check_addr(f -> esp + 24);
      check_addr(f -> esp + 28);
      f -> eax = write(*(uint32_t *) (f -> esp + 20), *(uint32_t *)(f -> esp + 24), *(uint32_t *)(f -> esp + 28));
      break;

    case SYS_SEEK:
      check_addr(f -> esp + 16);
      check_addr(f -> esp + 20);
      seek(*(uint32_t *)(f -> esp + 16), *(unsigned *)(f -> esp + 20));
      break;

    case SYS_TELL:
      check_addr(f -> esp + 4);
      f -> eax = tell(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_CLOSE:
      check_addr(f -> esp + 4);
      close(*(uint32_t *) (f -> esp + 4));
      break;

    default: 
      exit(-1);
  }
}

void check_addr(void *uaddr){
  // 1. user addr인지 check
  if(uaddr == NULL || !is_user_vaddr(uaddr)) exit(-1);

  // 2. stack growth
  if(uaddr < thread_current() -> stack){
    uint8_t *kpage = palloc_get_page(PAL_USER);
    if(kpage == NULL)
      exit(-1);

    fid_t fid = falloc(kpage, PGSIZE);
    if(fid == -1)
      NOT_REACHED();

    thread_current() -> stack = kpage;
  }
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
    if(fd_list[i] != NULL) close(i);
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
