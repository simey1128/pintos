#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#include "debug.h"

#include "threads/palloc.h"


#define NOT_REACHED() PANIC ("frame, frame overflow");

static fid_t allocate_fid(void);

fid_t
falloc(uint32_t *kpage, uint32_t *upage){
    fid_t fid = (uint32_t)(kpage - (uint32_t *)PHYS_BASE) >> 12;

    if(fid == -1)
        NOT_REACHED();

    struct frame_entry *e = malloc(sizeof *e);
    e -> fid = fid;
    e -> pd = thread_current() -> pagedir;
    e -> upage = upage;
    e -> kpage = kpage;

    list_insert_ordered(&frame_table, &e->elem, cmp_fid, NULL);
    return fid;
}

void
ffree(uint32_t *pte){
    fid_t fid = (*pte & 0xfffff000) >> 12;
    struct list_elem *e = list_begin(&frame_table);
    while(e != list_end(&frame_table)){
        struct frame_entry *f = list_entry(e, struct frame_entry, elem);
        if(f -> fid == fid){
            list_remove(&f->elem);
            // printf("before\n");
            // palloc_free_page(f->kpage);
            // printf("after\n");
            free(f);
            return;
        }
        e = e->next;
    }
}

bool cmp_fid(const struct list_elem *_l, const struct list_elem *_s, void *aux){
  const struct frame_entry *l = list_entry(_l, struct frame_entry, elem);
  const struct frame_entry *s = list_entry(_s, struct frame_entry, elem);
  return (l -> fid) < (s -> fid);
}
