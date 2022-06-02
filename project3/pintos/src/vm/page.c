#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

#include "debug.h"

#define NOT_REACHED() PANIC ("OVER SPT_MAX");


void
spte_create(struct file *file, off_t ofs, void* upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable){
    ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT (pg_ofs (upage) == 0);
    ASSERT (ofs % PGSIZE == 0);

    while (read_bytes > 0 || zero_bytes > 0) {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        struct spage_entry *spte = malloc(sizeof(*spte));
        spte -> upage = upage;
        spte -> file = file;
        spte -> ofs = ofs;
        spte -> read_bytes = page_read_bytes;
        spte -> zero_bytes = page_zero_bytes;
        spte -> writable = writable;

        list_push_back(&thread_current()->spage_table, &spte->elem);
        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        ofs += page_read_bytes;
        upage += PGSIZE;
    }
}

void spt_free(){
    uint32_t *pd = thread_current()->pagedir;
    struct list_elem *e = list_begin(&thread_current()->spage_table);
    while(e != list_end(&thread_current()->spage_table)){
        lock_acquire(&swap_lock);
        struct spage_entry *spte = list_entry(e, struct spage_entry, elem);
        list_remove(&spte->elem);
        pagedir_clear_page(pd, spte->upage);
        palloc_free_page(pagedir_get_page(pd, spte->upage));
        // free(spte);
        lock_release(&swap_lock);

        e = list_next(e);
    }
}

int write_back(uint32_t *pd, struct mmap_entry *me){
   int write_value = 0;
   uint32_t *mpage;
   uint32_t *start_pg = (uint32_t)me->start_addr & 0xfffff000;
   for(mpage = start_pg; mpage < start_pg + me->file_size + PGSIZE; mpage += PGSIZE){
      if(pagedir_is_dirty(pd, mpage)){
         write_value += file_write_at(me->file, mpage, PGSIZE, mpage - start_pg);
         pagedir_clear_page(pd, mpage);
        //  pagedir_set_dirty(pd, mpage, false);
      }
   }
}

struct spage_entry* get_spte(uint32_t *pd, uint32_t* upage){
   struct list_elem *e = list_begin(&thread_current()->spage_table);
    while(e != list_end(&thread_current()->spage_table)){
        struct spage_entry *spte = list_entry(e, struct spage_entry, elem);
        if(spte->upage == upage)
            return spte;
        e = list_next(e);
    }
    return NULL;
}

struct mmap_entry* get_me(uint32_t *pd, uint32_t *upage){
   struct list_elem *e = list_begin(&thread_current()->mmap_table);
   while(e != list_end(&thread_current()->mmap_table)){
      struct mmap_entry *me = list_entry(e, struct mmap_entry, elem);
      if (me->upage == upage){
         return me;
      }
      e = list_next(e);
   }
   return NULL;
}

struct stack_entry* get_stke(uint32_t *pd, uint32_t *upage){
   struct list_elem *e = list_begin(&thread_current()->stack_table);
   while(e != list_end(&thread_current()->stack_table)){
      struct stack_entry *stke = list_entry(e, struct stack_entry, elem);
      if (stke->upage == upage){
         return stke;
      }
      e = list_next(e);
   }
   return NULL;
}