#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "devices/block.h"
#include "threads/synch.h"
#include <list.h>

typedef bool bit;
struct lock frame_lock;
struct lock swap_lock;
struct lock disk_lock;

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

void swap_init(void);
void swap_out(struct frame_entry *);
void reclaim();
void swap_disk_free(uint32_t *);
#endif