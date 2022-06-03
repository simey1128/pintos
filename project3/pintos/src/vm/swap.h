#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "devices/block.h"
#include "threads/synch.h"
#include <list.h>

typedef bool bit;
struct lock swap_lock;

struct swap_entry{
    uint32_t *pd;
    uint32_t *upage;
    bool writable;

    block_sector_t idx;
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    struct list_elem elem;
};

struct list swap_table;
struct block *swap_disk;

struct bitmap *swap_bitmap;
struct swap_entry* get_swap_entry(uint32_t*pd, uint32_t*upage);


#endif
void swap_init(void);
void swap_out(uint32_t *, struct vpage_entry *);
void swap_in(uint32_t *, uint32_t *, struct swap_entry *);
void reclaim();