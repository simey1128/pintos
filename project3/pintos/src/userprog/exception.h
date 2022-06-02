#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

void exception_init (void);
void exception_print_stats (void);


int lazy_load_segment (struct spage_entry*, uint32_t *);
int load_mapped_file(struct mmap_entry *, uint32_t *);
void load_stack_segment(uint32_t * , void*);

#endif /* userprog/exception.h */
