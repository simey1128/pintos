#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#define bool _Bool

void syscall_init (void);

void check_addr(void *);

void halt(void);
void exit(int);
int write(int, const void *, unsigned);
bool create(const char*, unsigned);
#endif /* userprog/syscall.h */
