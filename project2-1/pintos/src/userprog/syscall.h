#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void check_addr(void *);

void halt(void);
void exit(int);
int write(int, const void *, unsigned);

#endif /* userprog/syscall.h */
