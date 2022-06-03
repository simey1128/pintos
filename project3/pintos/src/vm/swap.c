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

void swap_out(uint32_t *pd, struct vpage_entry *vte){
    //swap table entry 만들기
    size_t idx = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    struct swap_entry* se = malloc(sizeof *se);
    se -> pd = pd;
    se -> upage = vte->upage;
    se -> writable = vte -> writable;
    se->idx = idx;
    se -> file = vte -> file;
    se -> ofs = vte -> ofs;
    se -> read_bytes = vte -> read_bytes;
    se -> zero_bytes = vte -> zero_bytes;

    //disk 쓰기
    size_t i;
    for(i=0; i<SECTORS; i++){
        block_write(swap_disk, se->idx*SECTORS + i, pagedir_get_page(pd, vte->upage) + (i * BLOCK_SECTOR_SIZE/4));
    }
    // bitmap_set(swap_bitmap, se->sector, true);
    
    list_push_back(&swap_table, &se->elem);
    pagedir_clear_page(pd, vte->upage);   // fte->upage: reclaim 대상 (박힌돌)
    palloc_free_page(pagedir_get_page(pd, vte->upage));
    // free(fte);
}

void swap_in(uint32_t* kpage, uint32_t *pd, struct swap_entry *vte){
    struct swap_entry *se = get_swap_entry(pd, vte->upage);
    int i;
    for(i=0; i<SECTORS; i++){
        block_read(swap_disk, se->idx * SECTORS + i, kpage+(i*BLOCK_SECTOR_SIZE/4));
    }
    bitmap_flip(swap_bitmap, se->idx);
    list_remove(&se->elem);
    // free(se);
}

struct swap_entry* get_swap_entry(uint32_t *pd, uint32_t *upage){
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
    struct thread *t = thread_current();
    struct list_elem *e = list_begin(&t -> vpage_table);
    while(1){
        struct vpage_entry *vte = list_entry(e, struct vpage_entry, elem);
        bool dirty = pagedir_is_dirty(t->pagedir, vte->upage);
        bool accessed = pagedir_is_accessed(t->pagedir, vte->upage);

        if(vte->type == SUP_PAGE && !vte->swap_disable){
            if(dirty){
                swap_out(t->pagedir, vte);
                vte->type = REM_PAGE;
            }
        }else if(vte->type == MAP_PAGE && !vte->swap_disable){
            if(dirty){
                write_back(t->pagedir, vte);
            }
        }else if(vte->type == REM_PAGE && !vte->swap_disable){
            swap_out(t->pagedir, vte);
        }
        vte -> on_frame = false;
        kfree(vte -> upage);

        e = list_next(e);
        if(e == list_end(&frame_table))
            e = list_begin(&frame_table);
    }
}