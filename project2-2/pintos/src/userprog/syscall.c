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
#include "devices/input.h"
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_addr((f->esp));
  switch(*(uint8_t *)(f -> esp)){
    case SYS_HALT:
      halt();
      break;

    case SYS_EXIT:
      check_addr(f -> esp + 4);
      exit(*(uint32_t *)(f -> esp + 4));
      break;

    case SYS_WRITE:
      check_addr(f -> esp + 24);
      write(*(uint32_t *) (f -> esp + 20), *(uint32_t *)(f -> esp + 24), *(uint32_t *)(f -> esp + 28));
      break;
    
    case SYS_OPEN:
      check_addr(*(uint32_t *)(f -> esp + 4));
      int fd = open(*(uint32_t *)(f -> esp + 4));
      printf("fd is %d\n", fd);
      f->eax = fd; 
      break;

    case SYS_CLOSE:
      close(*(uint32_t *) (f -> esp + 4));
      break;

    case SYS_CREATE:
      check_addr(*(uint32_t *)(f -> esp + 16));
      int success = create(*(uint32_t *)(f -> esp + 16), *(unsigned *)(f -> esp + 20));
      f->eax = success;
      break;

    case SYS_READ:
      check_addr(*(uint32_t *)(f -> esp + 24)); //TODO
      off_t bytes_read = read(*(uint32_t *) (f -> esp + 20), *(uint32_t *)(f -> esp + 24), *(uint32_t *)(f -> esp + 28));
      f->eax = bytes_read;
      break;
    





    default: 
      break;
  }
}

void check_addr(void *uaddr){
  if(uaddr == NULL || !is_user_vaddr(uaddr)) exit(-1);
}


void halt(){
  shutdown_power_off();
}

void exit(int status){
  printf("%s: exit(%d)\n", thread_name(), status);

  struct file **fd_list = thread_current()->fd_list;
  int i;
  for(i=3; i<128; i++){
    if(fd_list[i] != NULL) close(i);
  }
  
  thread_exit();
}

int write(int fd, const void *buffer, unsigned size){
  if (fd == 1){
    putbuf(buffer, size);
    return size;
  }
  struct file *file = (thread_current()->fd_list)[fd];
  if(file == NULL) return -1;

  off_t bytes_written = file_write(file, buffer, size);
  return bytes_written;
  return -1;
}

int read(int fd, const void *buffer, unsigned size){
  printf("read\n");
  if(fd == 0){
    uint8_t key = input_getc();
    return key;
  }
  struct file *file = (thread_current()->fd_list)[fd];
  if(file == NULL) return -1;


  off_t bytes_read = file_read(file, buffer, size);
  return bytes_read;
}

int open(const char* file){
  struct file* file_opened = filesys_open(file);
 
  if(file_opened == NULL) {
    return -1;
  }
  
  return add_fd(file_opened);
}

void close(int fd){
  if(fd > 128) exit(-1); // userprog/close-bad-fd

  struct thread *cur_thread = thread_current();
  struct file *file = (cur_thread->fd_list)[fd];
  
  if(file == NULL) exit(-1);

  cur_thread->fd_list[fd] = NULL;
  return file_close(file);


}

int create(const char* file, unsigned initial_size){
  if(strlen(file)>128) return 0; // userprog/create-long
  if(strlen(file) == 0) exit(-1); // userprog/create-empty

  return filesys_create(file, initial_size);

}
