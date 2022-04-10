#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf("syscall num: %d\n", *(uint8_t *)(f -> esp));
  // hex_dump(f -> esp, f -> esp, 100, true);
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
  }
  // thread_exit ();
}

void check_addr(void *uaddr){
  if(!is_user_vaddr(uaddr))
    exit(-1);
}

void halt(){
  shutdown_power_off();
}

void exit(int status){
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
}

int write(int fd, const void *buffer, unsigned size){
  if (fd == 1){
    putbuf(buffer, size);
    return size;
  }
  return -1;
}

int close(int fd){

}