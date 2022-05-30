#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

typedef int pid_t;
typedef int mapid_t;
struct lock filesys_lock;

void syscall_init (void);

void halt();
void exit(int);
pid_t exec(const char *);
int wait(pid_t pid);
int create(const char*, unsigned);
int remove(const char *);
int open(const char*);
int filesize(int fd);
int read(int, void *, unsigned);
int write(int, const void *, unsigned);
void seek(int, unsigned);
unsigned tell(int);
void close(int);
mapid_t mmap(int, void *);
void munmap(mapid_t);
#endif /* userprog/syscall.h */
