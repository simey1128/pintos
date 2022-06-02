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
        spte -> vpage.upage = upage;
        spte -> file = file;
        spte -> ofs = ofs;
        spte -> read_bytes = page_read_bytes;
        spte -> zero_bytes = page_zero_bytes;
        spte -> vpage.writable = writable;

        list_push_back(&thread_current()->spage_table, &spte->vpage.elem);
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
        struct spage_entry *spte = list_entry(e, struct spage_entry, vpage.elem);
        list_remove(&spte->vpage.elem);
        pagedir_clear_page(pd, spte->vpage.upage);
        palloc_free_page(pagedir_get_page(pd, spte->vpage.upage));
        // free(spte);
        lock_release(&swap_lock);

        e = list_next(e);
    }
}

void stack_free(){
   uint32_t *pd = thread_current()->pagedir;
    struct list_elem *e = list_begin(&thread_current()->stack_table);
    while(e != list_end(&thread_current()->stack_table)){
        lock_acquire(&swap_lock);
        struct stack_entry *stke = list_entry(e, struct stack_entry, vpage.elem);
        list_remove(&stke->vpage.elem);
        pagedir_clear_page(pd, stke->vpage.upage);
        palloc_free_page(stke->vpage.kpage);
        // free(spte);
        lock_release(&swap_lock);

        e = list_next(e);
    }
}

void me_create(mapid_t mapid, struct file* file, int file_size, uint32_t* start_addr){
    int read_bytes = file_size;
    int zero_bytes = 1;
    uint32_t* upage = start_addr;

    while(read_bytes > 0 || zero_bytes > 0){
        int tmp_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        int tmp_zero_bytes = PGSIZE - tmp_read_bytes;
        
        struct mmap_entry* me = malloc(sizeof(*me));
        me->vpage.upage = upage;
        me->mapid = mapid;
        me->file = file;
        me->file_size = file_size;
        me->start_addr = start_addr;

        list_push_back(&thread_current()->mmap_table, &me->vpage.elem);


        upage += PGSIZE;
        read_bytes -= tmp_read_bytes;
        zero_bytes -= tmp_zero_bytes;
    }
}

struct spage_entry* get_spte(uint32_t *pd, uint32_t* kpage){
   struct list_elem *e = list_begin(&thread_current()->spage_table);
    while(e != list_end(&thread_current()->spage_table)){
        struct spage_entry *spte = list_entry(e, struct spage_entry, vpage.elem);
        if(spte->vpage.kpage == kpage)
            return spte;
        e = list_next(e);
    }
    return NULL;
}

struct mmap_entry* get_me(uint32_t *pd, uint32_t *upage){
   struct list_elem *e = list_begin(&thread_current()->mmap_table);
   while(e != list_end(&thread_current()->mmap_table)){
      struct mmap_entry *me = list_entry(e, struct mmap_entry, vpage.elem);
      if (me->vpage.upage == upage){
         return me;
      }
      e = list_next(e);
   }
   return NULL;
}

struct stack_entry* get_stke(uint32_t *pd, uint32_t *upage){
   struct list_elem *e = list_begin(&thread_current()->stack_table);
   while(e != list_end(&thread_current()->stack_table)){
      struct stack_entry *stke = list_entry(e, struct stack_entry, vpage.elem);
      if (stke->vpage.upage == upage){
         return stke;
      }
      e = list_next(e);
   }
   return NULL;
}

struct vpage* get_vpage(uint32_t *pd, uint32_t *kpage){
   struct list_elem* e;
   
   e = list_begin(&thread_current()->spage_table);
   while(e != list_end(&thread_current()->spage_table)){
      struct spage_entry* spte = list_entry(e, struct spage_entry, vpage.elem);
      if(spte->vpage.pd == pd && spte->vpage.kpage == kpage){
         return &(spte->vpage);
      }
      e = e->next;
   }

   e = list_begin(&thread_current()->mmap_table);
   while(e != list_end(&thread_current()->mmap_table)){
      struct mmap_entry* me = list_entry(e, struct mmap_entry, vpage.elem);
      if(me->vpage.pd == pd && me->vpage.kpage == kpage){
         return &me->vpage;
      }
      e = e->next;
   }

   e = list_begin(&thread_current()->stack_table);
   while(e != list_end(&thread_current()->stack_table)){
      struct stack_entry* stke = list_entry(e, struct stack_entry, vpage.elem);
      if(stke->vpage.pd == pd && stke->vpage.kpage == kpage){
         return &stke->vpage;
      }
      e = e->next;
   }

   PANIC("NOT_REACHED, get_vpage");

}