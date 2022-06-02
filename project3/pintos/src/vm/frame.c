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

uint32_t
*falloc(enum palloc_flags flag){
    lock_acquire(&swap_lock);
    uint8_t *kpage = palloc_get_page(flag);
    while(kpage == NULL){
        reclame();
        kpage = palloc_get_page(flag);
    }
    lock_release(&swap_lock);
    return kpage;
}

void
ffree(uint32_t *upage){   // After this, no kpage, not present upage
    lock_acquire(&swap_lock);
    uint32_t *pd = thread_current() -> pagedir;
    struct spage_entry *spte = get_spte(pd, upage);
    if(spte != NULL){
        list_remove(&spte->elem);
        pagedir_clear_page(pd, spte->upage);
        palloc_free_page(spte->kpage);
        free(spte);
        lock_release(&swap_lock);
        return;
    }
    struct mmap_entry *me = get_me(pd, upage);
    if(me != NULL){
        list_remove(&me->elem);
        pagedir_clear_page(pd, me->upage);
        palloc_free_page(me->kpage);
        free(me);
        lock_release(&swap_lock);
        return;
    }
    struct stack_entry *stke = get_stke(pd, upage);
    if(stke != NULL){
        list_remove(&stke->elem);
        pagedir_clear_page(pd, stke->upage);
        palloc_free_page(stke->kpage);
        free(stke);
        lock_release(&swap_lock);
        return;
    }
    PANIC("NOT_REACHED, ffree");
}
