#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"

#include "kernel/bitmap.h"
#include "lib/string.h"
#include "debug.h"
#include "lib/round.h"

#define SECTORS (PGSIZE/BLOCK_SECTOR_SIZE)

void swap_init(){
    swap_disk = block_get_role(BLOCK_SWAP);

    // size_t bm_pages = DIV_ROUND_UP (bitmap_buf_size (1024), PGSIZE);
    swap_bitmap = bitmap_create(block_size(swap_disk)/SECTORS);

    list_init(&swap_table);
}

void swap_out(struct frame_entry *fte){
    if(bitmap_all(swap_bitmap, 0, 1024)){
        PANIC("bitmap out");
    }
    //swap table entry 만들기
    struct swap_entry* se = malloc(sizeof *se);
    se->pd = fte->pd;
    se->upage = fte->upage;
    se->sector = bitmap_scan_and_flip(swap_bitmap, 0, 1, false) * SECTORS;

    list_push_back(&swap_table, &se);

    //disk 쓰기
    int i;
    for(i=0; i<SECTORS; i++){
        block_write(swap_disk, se->sector+i, fte->kpage + i * BLOCK_SECTOR_SIZE);
    }

}

void swap_in(uint32_t* kpage, struct swap_entry *se){

    int i;
    for(i=0; i<SECTORS; i++){
        block_read(swap_disk, se->sector+i, kpage+i*BLOCK_SECTOR_SIZE);
    }

    bitmap_reset(swap_bitmap, se->sector/SECTORS);
    list_remove(&se->elem);
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