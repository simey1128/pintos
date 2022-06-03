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

uint32_t *kalloc(int flag){
    lock_acquire(&swap_lock);
    uint32_t *kpage = palloc_get_page(flag);
    while(kpage == NULL){
        reclaim();
        kpage = palloc_get_page(flag);
    }
    lock_release(&swap_lock);
    return kpage;
}

void kfree(uint32_t *upage){
    lock_acquire(&swap_lock);
    struct thread *t = thread_current();
    struct list_elem *e = list_begin(&t -> vpage_table);
    struct vpage_entry *vte = get_vte(upage);
    list_remove(&vte->elem);
    pagedir_clear_page(t -> pagedir, upage);
    palloc_free_page(pagedir_get_page(t -> pagedir, upage));
    // free(vte);
    lock_release(&swap_lock);
}
