#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "devices/block.h"
#include <list.h>

typedef bool bit;

struct swap_entry{
    uint32_t *pd;
    uint32_t *upage;

    block_sector_t sector;
    struct list_elem elem;
};

struct list swap_table;
struct block *swap_disk;

struct bitmap *swap_bitmap;
struct swap_entry* get_swap_entry(uint32_t*pd, uint32_t*upage);


#endif
void swap_init(void);
void swap_out(struct frame_entry *);
void reclaim();