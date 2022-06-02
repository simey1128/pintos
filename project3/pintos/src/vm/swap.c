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
static bool _reclaim(struct vpage* vpage);

void swap_init(){
    swap_disk = block_get_role(BLOCK_SWAP);
    swap_bitmap = bitmap_create(block_size(swap_disk)/SECTORS);
    bitmap_set_all(swap_bitmap, false);

    list_init(&swap_table);
    lock_init(&swap_lock);
}

void swap_out(struct vpage* vpage){ //#TODO
    //swap table entry 만들기
    size_t idx = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    struct swap_entry* se = malloc(sizeof *se);
    se->pd = vpage->pd;
    se->upage = vpage->upage;
    se->sector = idx;

    //disk 쓰기
    size_t i;
    for(i=0; i<SECTORS; i++){
        block_write(swap_disk, se->sector*SECTORS + i, vpage->kpage + (i * BLOCK_SECTOR_SIZE/4));
    }
    // bitmap_set(swap_bitmap, se->sector, true);
    
    list_push_back(&swap_table, &se->elem);
    list_remove(&vpage->elem);
    vpage->on_frame = false;
    pagedir_clear_page(vpage->pd, vpage->upage);   // vpage->upage: reclaim 대상 (박힌돌)
    palloc_free_page(vpage->kpage);
}

void swap_in(uint32_t* kpage, struct swap_entry *se){
    struct vpage* vpage = get_vpage(se->pd, kpage);

    int i;
    for(i=0; i<SECTORS; i++){
        block_read(swap_disk, se->sector * SECTORS + i, kpage+(i*BLOCK_SECTOR_SIZE/4));
    }
    
    bitmap_flip(swap_bitmap, se->sector);
    vpage->on_frame = true;
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
    struct list_elem* e;

    e = list_begin(&thread_current()->mmap_table);
    while(e != list_end(&thread_current()->spage_table)){
        struct spage_entry* spte = list_entry(e, struct spage_entry, vpage.elem);
        if(_reclaim(&spte->vpage)) return;

        e = e->next;
    }   

    e = list_begin(&thread_current()->mmap_table);
    while(e != list_end(&thread_current()->mmap_table)){
        struct mmap_entry* me = list_entry(e, struct mmap_entry, vpage.elem);
        if(_reclaim(&me->vpage)) return;

        e = e->next;
    } 

    e = list_begin(&thread_current()->stack_table);
    while(e != list_end(&thread_current()->stack_table)){
        struct stack_entry* stke = list_entry(e, struct stack_entry, vpage.elem);
        if(_reclaim(&stke->vpage)) return;
        
        e = e->next;
    } 

    PANIC("NOT_REACHED, reclaim");


}


static bool _reclaim(struct vpage* vpage){
        bool dirty = pagedir_is_dirty(vpage->pd, vpage->upage);
        bool accessed = pagedir_is_accessed(vpage->pd, vpage->upage);

        if(!dirty && !accessed && !vpage->swap_disable){   // need to reclaim
            swap_out(vpage);
            return true;
        }

        if(dirty){   // need to write back
            struct mmap_entry *me = get_me(vpage->pd, vpage->upage);
            if(me != NULL){
                // write back
                int write_value = file_write_at(me->file, vpage->upage, PGSIZE, vpage->upage - me->start_addr);
            }
            pagedir_set_dirty(vpage->pd, vpage->upage, false);
        }

        if(accessed){   // need to clear accessed bit
            pagedir_set_accessed(vpage->pd,vpage->upage, false);
        }

        return false;

    
}