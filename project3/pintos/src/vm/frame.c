#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"


fid_t
falloc(uint8_t *kpage, int size){
    fid_t fid = allocate_fid();
    if(fid == -1)
        return fid;

    struct frame_entry *e;
    e -> fid = fid;
    e -> kpage = kpage;

    e -> accessed = 0;
    e -> dirty = 0;

    list_insert_ordered(&frame_entry);

    return fid;
}

static fid_t
allocate_fid(void){
    int i;
    fid_t fid = fid_next;
    if(fid > 1024)
        return -1;
    struct list_elem *e = list_begin(&frame_table);
    while(e != list_end(&frame_table)){
        struct frame_entry *cur = list_entry(e, struct frame_entry, elem);
        struct frame_entry *next = list_entry(e->next, struct frame_entry, elem);
        if(next->fid - cur->fid > 1){
            fid_next = cur->fid + 1;
            return fid;
        }
    }
    fid_next = list_size(&frame_table) + 1;
    return fid;
}