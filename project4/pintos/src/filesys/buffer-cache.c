#include "filesys/buffer-cache.h"
#include "filesys/filesys.h"
#include <string.h>

static uint8_t *buffer_cache;

static struct bc_entry* bc_table[BUFFER_CACHE_SIZE];
static struct lock bc_table_lock;

void buffer_cache_init(){
    buffer_cache = (uint8_t*)malloc(sizeof(uint8_t) * 64 * BLOCK_SECTOR_SIZE);

    lock_init(&bc_table_lock);
    int i;
    for(i=0; i<BUFFER_CACHE_SIZE; i++){
        struct bc_entry* bce = malloc(sizeof(*bce));

        bce->cache = buffer_cache + i * BLOCK_SECTOR_SIZE;
        lock_init(&bce->lock);
        bc_table[i] = bce;
    }
}

struct bc_entry* get_bc_entry(block_sector_t sector){
    lock_acquire(&bc_table_lock);
    // buffer_cache_table에 존재하는지 확인
    int i;
    for(i=0; i<BUFFER_CACHE_SIZE; i++){
        struct bc_entry* bce = bc_table[i];
        if(bce->sector == sector){
            lock_release(&bc_table_lock);
            return bce;
        }
    }

    // buffer_cache_table에 없으며, entry를 새로 만들어야함
    for(i=0; i<BUFFER_CACHE_SIZE; i++){
        if(bc_table[i] == NULL){
            struct bc_entry* bce = bc_table[i];
            set_bc_entry(bce, sector);
            lock_release(&bc_table_lock);
            return bce;
        }
    }

    // buffer_cache_table가 꽉차서 entry를 replace해야함
    lock_release(&bc_table_lock);
    return replace_bc_entry(sector);    
}

void read_buffer_cache(block_sector_t idx, void* _buffer, off_t bytes_read, off_t size, off_t offset){
    struct bc_entry* bce = get_bc_entry(idx);
    if(bce->new){
        lock_acquire(&bce->lock);

        block_read(fs_device, idx, bce->cache);
        bce->new=0;
        lock_release(&bce->lock);
    }
    
    lock_acquire(&bce->lock);

    memcpy(_buffer+bytes_read, bce->cache + offset, size);

    bce->last_clock = timer_ticks();
    lock_release(&bce->lock);
}

void write_buffer_cache(block_sector_t idx, void* buffer_, off_t bytes_written, off_t size, off_t offset){
    struct bc_entry* bce = get_bc_entry(idx);

    lock_acquire(&bce->lock);
    if(bce->new){
        block_write(fs_device, idx, bce->cache);
        bce->new=0;
    }
    
    memcpy(bce->cache+offset , buffer_ + bytes_written, size);

    bce->last_clock = timer_ticks();
    bce->dirty = 1;
    lock_release(&bce->lock);
}

struct bc_entry* replace_bc_entry(block_sector_t sector){
    struct bc_entry* victim_be = get_oldest_bc_entry();

    if(victim_be->dirty) flush_bc_entry(victim_be);
    set_bc_entry(victim_be, sector);

    return victim_be;
}

struct bc_entry* get_oldest_bc_entry(){
    lock_acquire(&bc_table_lock);
    int i;
    struct bc_entry* victim_be = NULL;

    for(i=0; i<BUFFER_CACHE_SIZE; i++){
        struct bc_entry* target_be = bc_table[i];

        if(victim_be == NULL || target_be->last_clock < victim_be->last_clock){
            victim_be = target_be;
        }
    }
    lock_release(&bc_table_lock);
    return victim_be;
}

void flush_bc_entry(struct bc_entry* bce){
    lock_acquire(&bce->lock);
    block_write(fs_device, bce->sector, bce->cache);
    bce->dirty = 0;
    bce->last_clock = timer_ticks();
    lock_release(&bce->lock);
}

void set_bc_entry(struct bc_entry* bce, block_sector_t sector){
    lock_acquire(&bce->lock);
    bce->inode = inode_open(sector);
    bce->sector = sector;
    bce->dirty = 0;
    bce->last_clock = timer_ticks();
    bce->new = 1;
    lock_release(&bce->lock);
}

void buffer_cache_free(){
    int i;
    for(i=0; i<BUFFER_CACHE_SIZE; i++){
        struct bc_entry* bce = bc_table[i];
        if(bce->dirty){
            flush_bc_entry(bce);
        }
        free(bce);
        bc_table[i] = NULL;
    }
}