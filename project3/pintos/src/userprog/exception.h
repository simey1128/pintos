#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

void exception_init (void);
void exception_print_stats (void);

struct spage_entry* get_spte(uint32_t* uaddr);
struct mmap_entry *get_me(uint32_t* uaddr);
int lazy_load_segment (struct spage_entry*);
int load_mapped_file(struct mmap_entry *me, uint32_t *uaddr);
int write_back(uint32_t *pd, struct mmap_entry *me);
#endif /* userprog/exception.h */
