#include "filesys/buffer-cache.h"
#include "filesys/filesys.h"

void init_buffer_cache(){
    int i;
    for(i=0; i<BUFFER_LIST_SIZE; i++){
        buffer_list[i] = NULL;
        buffer_list[i]->cache = (uint8_t*)malloc(sizeof(uint8_t*)*BLOCK_SECTOR_SIZE);
        lock_init(&buffer_list[i]->lock);
    }
}
/*
buffer_list에서 sector위치에 있는 buffer_entry 반환
* invalid sector는 sector<0 or buffer_list[sector]가 비어있을 때
* sector가 BUFFER_LIST_SIZE 이상이면, select victim, replace entry
* 그렇지 않으면 buffer_list[sector] 반환
*/
struct buffer_entry* get_buffer_entry(block_sector_t sector){
    
    // buffer_list에 존재하는지 확인
    int i;
    for(i=0; i<BUFFER_LIST_SIZE; i++){
        struct buffer_entry* be = buffer_list[i];
        if(be->sector == sector) return be;
    }

    // buffer_list에 없으며, entry를 새로 만들어야함
    for(i=0; i<BUFFER_LIST_SIZE; i++){
        if(buffer_list[i] == NULL){
            struct buffer_entry* be = buffer_list[i];
            set_buffer_entry(be, sector, i);

            return be;
        }
    }

    // buffer_list가 꽉차서 entry를 replace해야함
    return replace_buffer_entry(sector);    
}

void buffer_read(block_sector_t idx, void* _buffer, off_t bytes_read, off_t size, off_t offset){
    struct buffer_entry* be = get_buffer_entry(idx);
    lock_acquire(&be->lock);

    block_read(fs_device, idx, be->cache);
    memcpy(_buffer+bytes_read, be->cache + offset, size);

    be->last_clock = timer_ticks();
    lock_release(&be->lock);
}

void buffer_write(block_sector_t idx, void* buffer_, off_t bytes_written, off_t size, off_t offset){
    struct buffer_entry* be = get_buffer_entry(idx);
    lock_acquire(&be->lock);

    block_write(fs_device, idx, be->cache);
    memcpy(be->cache+offset , buffer_ + bytes_written, size);

    be->last_clock = timer_ticks();
    be->dirty = 1;
    lock_release(&be->lock);
}

struct buffer_entry* replace_buffer_entry(block_sector_t sector){
    int i, victim_idx;
    struct buffer_entry* victim_be = NULL;

    for(i=0; i<BUFFER_LIST_SIZE; i++){
        struct buffer_entry* target_be = buffer_list[i];

        if(victim_be == NULL || target_be->last_clock < victim_be->last_clock){
            victim_be = target_be;
            victim_idx = i;
        }
    }

    lock_acquire(&victim_be->lock);
    if(victim_be->dirty) flush_buffer_entry(victim_be);
    set_buffer_entry(victim_be, sector, victim_idx);

    lock_release(&victim_be->lock);
    return victim_be;
}

void flush_buffer_entry(struct buffer_entry* be){
    block_write(fs_device, be->sector, be->cache);
    be->dirty = 0;
    be->last_clock = timer_ticks();
}

void set_buffer_entry(struct buffer_entry* be, block_sector_t sector, int idx){
    be->inode = inode_open(sector);
    be->sector = sector;
    be->dirty = 0;
    be->last_clock = timer_ticks();
}

void free_buffer(){
    int i;
    for(i=0; i<BUFFER_LIST_SIZE; i++){
        struct buffer_entry* be = buffer_list[i];
        lock_acquire(&be->lock);
        if(be->dirty){
            flush_buffer_entry(be);
        }
        free(be->cache);
        lock_release(&be->lock);
        buffer_list[i] = NULL;
    }
}