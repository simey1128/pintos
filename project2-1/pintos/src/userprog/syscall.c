#include "userprog/syscall.h"
#include <stdio.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"

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
  // hex_dump(f -> esp, f -> esp, PHYS_BASE - f ->esp, true);
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
    case SYS_CREATE:
      create(*(__CHAR32_TYPE__ *) (f -> esp + 16), *(uint32_t *) (f -> esp + 20));
      break;
    // case SYS_OPEN:
    //   open();
    //   break;
    // case SYS_CLOSE:
    //   close();
    //   break;
  }
  // thread_exit ();
}

void check_addr(void *uaddr){
  if(uaddr == NULL || !is_user_vaddr(uaddr))
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

bool create(const char* file, unsigned initial_size){
  return filesys_create(file, initial_size);
}
