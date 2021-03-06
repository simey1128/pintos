#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"

#include <stdio.h>
#include "kernel/bitmap.h"
#include "lib/string.h"
#include "debug.h"
#include "lib/round.h"

#define SECTORS (PGSIZE/BLOCK_SECTOR_SIZE)

void swap_init(){
    swap_disk = block_get_role(BLOCK_SWAP);
    swap_bitmap = bitmap_create(block_size(swap_disk)/SECTORS);
    bitmap_set_all(swap_bitmap, false);

    list_init(&swap_table);
    lock_init(&swap_lock);
}

void swap_out(struct frame_entry *fte){
    //swap table entry 만들기
    size_t idx = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    struct swap_entry* se = malloc(sizeof *se);
    se->pd = fte->pd;
    se->upage = fte->upage;
    se->sector = idx;

    //disk 쓰기
    size_t i;
    for(i=0; i<SECTORS; i++){
        block_write(swap_disk, se->sector*SECTORS + i, fte->kpage + (i * BLOCK_SECTOR_SIZE/4));
    }
    // bitmap_set(swap_bitmap, se->sector, true);
    
    list_push_back(&swap_table, &se->elem);
    list_remove(&fte->elem);
    pagedir_clear_page(fte->pd, fte->upage);   // fte->upage: reclaim 대상 (박힌돌)
    palloc_free_page(fte->kpage);
    // free(fte);
}

void swap_in(uint32_t* kpage, struct swap_entry *se){

    int i;
    for(i=0; i<SECTORS; i++){
        block_read(swap_disk, se->sector * SECTORS + i, kpage+(i*BLOCK_SECTOR_SIZE/4));
    }

    bitmap_flip(swap_bitmap, se->sector);
    list_remove(&se->elem);
    // free(se);
}

struct swap_entry* get_swap_entry(uint32_t*pd, uint32_t*upage){
    struct list_elem* e = list_begin(&swap_table);
    while(e != list_end(&swap_table)){
        struct swap_entry* se = list_entry(e, struct swap_entry, elem);

        if(se->pd == pd && se->upage == upage){
            return se;
        }
        e = e->next;
    }
    return NULL;
}

void reclaim(){
    struct list_elem *e = list_begin(&frame_table);
    while(1){
        struct frame_entry *fte = list_entry(e, struct frame_entry, elem);
        bool dirty = pagedir_is_dirty(fte->pd, fte->upage);
        bool accessed = pagedir_is_accessed(fte->pd, fte->upage);

        if(!dirty && !accessed){   // need to reclaim
            swap_out(fte);
            return;
        }

        if(dirty){   // need to write back
            struct mmap_entry *me = get_me(fte->upage);
            if(me != NULL){
                // write back
                int write_value = file_write_at(me->file, fte->upage, PGSIZE, fte->upage - me->start_addr);
            }
            pagedir_set_dirty(fte->pd, fte->upage, false);
        }

        if(accessed){   // need to clear accessed bit
            pagedir_set_accessed(fte->pd,fte->upage, false);
        }
        e = list_next(e);

        if(e == list_end(&frame_table))
            e = list_begin(&frame_table);
    }
}