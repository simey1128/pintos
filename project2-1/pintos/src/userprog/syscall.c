#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <stdbool.h>
#include <string.h>

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
      check_addr(f -> esp + 28);
      write(*(uint32_t *) (f -> esp + 20), *(uint32_t *)(f -> esp + 24), *(uint32_t *)(f -> esp + 28));
      break;
    
    case SYS_OPEN:
      check_addr(*(__CHAR32_TYPE__ *)(f -> esp + 4));
      int fd = open(*(__CHAR32_TYPE__ *)(f -> esp + 4));
      f->eax = fd;
      break;

    case SYS_CLOSE:
      close(*(uint32_t *) (f -> esp + 4));
    break;

    case SYS_CREATE:
      check_addr(*(__CHAR32_TYPE__ *)(f -> esp + 16));
      int success = create(*(__CHAR32_TYPE__ *)(f -> esp + 16), *(unsigned *)(f -> esp + 20));
      f->eax = success;
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
  for(i=2; i<128; i++){
    if(fd_list[i] != NULL) close(i);
  }
  
  thread_exit();
}

int write(int fd, const void *buffer, unsigned size){
  if (fd == 1){
    putbuf(buffer, size);
    return size;
  }
  return -1;
}

int open(const char* file){
  struct file* opened = filesys_open(file);
 
  if(opened == NULL) {
    return -1;
  }

  return add_file(file);
}

void close(int fd){
  if(fd > 128) exit(-1);

  struct thread *cur_thread = thread_current();
  struct file *file = (cur_thread->fd_list)[fd];
  
  if(file == NULL) exit(-1);

  cur_thread->fd_list[fd] = NULL;
  return file_close(file);
}

int create(const char* file, unsigned initial_size){
  if(strlen(file)>128) return 0;
  if(strlen(file) == 0) exit(-1);

  return filesys_create(file, initial_size);

}
