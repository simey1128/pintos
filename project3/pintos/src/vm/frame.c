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

void
falloc(uint32_t *kpage, uint32_t *upage){
    lock_acquire(&swap_lock);
    struct frame_entry *e = malloc(sizeof *e);
    e -> pd = thread_current() -> pagedir;
    e -> upage = upage;
    e -> kpage = kpage;
    e -> swap_disable = false;

    list_push_back(&frame_table, &e->elem);
    lock_release(&swap_lock);
}

void
ffree(uint32_t *kpage){
    lock_acquire(&swap_lock);
    struct list_elem *e = list_begin(&frame_table);
    while(e != list_end(&frame_table)){
        struct frame_entry *f = list_entry(e, struct frame_entry, elem);
        if(f -> kpage == kpage){
            list_remove(&f->elem);
            pagedir_clear_page(f->pd, f->upage);
            // palloc_free_page(f->kpage);
            // free(f);
            break;
        }
        e = e->next;
    }
    lock_release(&swap_lock);
}
