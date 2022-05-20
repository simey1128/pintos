#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

#include "debug.h"


#define NOT_REACHED() PANIC ("frame overflow");

static fid_t allocate_fid(void);

fid_t
falloc(uint8_t *kpage, int size){
    fid_t fid = allocate_fid();

    if(fid == -1)
        NOT_REACHED();

    struct frame_entry *e = malloc(sizeof *e);
    e -> fid = fid;
    e -> kpage = kpage;
    e -> accessed = 0;
    e -> dirty = 0;

    list_insert_ordered(&frame_table, &e->elem, cmp_fid, NULL);

    return fid;
}

void
ffree(uint8_t *pte){
    fid_t fid = (*pte & 0xfffff000) >> 12;
    struct list_elem *e = list_begin(&frame_table);
    while(e != list_end(&frame_table)){
        struct frame_entry *f = list_entry(e, struct frame_entry, elem);
        if(f -> fid == fid){
            free(e);
            list_remove(e);
            return;
        }
    }
}

static fid_t
allocate_fid(void){
    fid_t fid = fid_next;
    if(fid > 0x100000)
        return -1;

    if(list_empty(&frame_table)){
        fid_next++;
        return fid;
    }
    struct list_elem *e = list_begin(&frame_table);
    while(e != list_end(&frame_table)){
        struct frame_entry *cur = list_entry(e, struct frame_entry, elem);
        struct frame_entry *next = list_entry(e->next, struct frame_entry, elem);
        if(next->fid - cur->fid > 1){
            fid_next = cur->fid + 1;
            return fid;
        }
        e= e->next;
    }
    fid_next = list_size(&frame_table) + 1;
    return fid;
}

bool cmp_fid(const struct list_elem *_l, const struct list_elem *_s, void *aux){
  const struct frame_entry *l = list_entry(_l, struct frame_entry, elem);
  const struct frame_entry *s = list_entry(_s, struct frame_entry, elem);
  return (l -> fid) < (s -> fid);
}