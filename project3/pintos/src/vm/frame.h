#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

#define NUM_FRAME_ENTRY 1024
#define FRBITS 12
#define FRSIZE (1 << FRBITS)


typedef uint32_t fid_t;
typedef bool bit;

fid_t fid_next;

struct frame_entry{
    fid_t fid;
    uint8_t *kpage;

    bit accessed;
    bit dirty;

    struct list_elem elem;
};

struct list frame_table;

fid_t falloc(uint8_t *, int);
void ffree(uint8_t *);

bool cmp_fid(const struct list_elem *, const struct list_elem *, void *);

#endif