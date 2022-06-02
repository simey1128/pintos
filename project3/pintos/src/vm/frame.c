#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#include "debug.h"

#include "threads/palloc.h"


#define NOT_REACHED() PANIC ("frame, frame overflow");


uint32_t*
falloc(enum palloc_flags flag){
    uint32_t *kpage;
    lock_acquire(&swap_lock);
    // printf("falloc, kpage: %p\n", kpage);
    kpage = palloc_get_page(flag);
    printf("falloc, kpage: %p\n", kpage);
    while(kpage == NULL){
        reclaim();
        kpage = palloc_get_page(flag);
        printf("falloc, kpage: %p\n", kpage);
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
        list_remove(&spte->vpage.elem);
        pagedir_clear_page(pd, spte->vpage.upage);
        palloc_free_page(spte->vpage.kpage);
        free(spte);
        lock_release(&swap_lock);
        return;
    }
    struct mmap_entry *me = get_me(pd, upage);
    if(me != NULL){
        list_remove(&me->vpage.elem);
        pagedir_clear_page(pd, me->vpage.upage);
        palloc_free_page(me->vpage.kpage);
        free(me);
        lock_release(&swap_lock);
        return;
    }
    struct stack_entry *stke = get_stke(pd, upage);
    if(stke != NULL){
        list_remove(&stke->vpage.elem);
        pagedir_clear_page(pd, stke->vpage.upage);
        palloc_free_page(stke->vpage.kpage);
        free(stke);
        lock_release(&swap_lock);
        return;
    }
    PANIC("NOT_REACHED, ffree");
}
