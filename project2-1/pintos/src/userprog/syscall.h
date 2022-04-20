#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


void syscall_init (void);

void check_addr(void *);
void halt();
void exit(int);
int write(int, const void *, unsigned);
int open(const char*);
void close(int);
int create(const char*, unsigned);
#endif /* userprog/syscall.h */
