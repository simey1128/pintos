#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

#define NUM_FRAME_ENTRY 1024
#define FRBITS 12
#define FRSIZE (1 << FRBITS)


typedef uint32_t fid_t;
typedef bool bit;

struct frame_entry{
    fid_t fid;
    uint32_t *pd;
    uint32_t *upage;
    uint32_t *kpage;

    bit accessed;
    bit dirty;

    struct list_elem elem;
};

struct list frame_table;

fid_t falloc(uint32_t *, uint32_t *);
void ffree(uint32_t *);

bool cmp_fid(const struct list_elem *, const struct list_elem *, void *);

#endif