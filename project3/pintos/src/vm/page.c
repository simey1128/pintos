#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

#include "debug.h"

#define NOT_REACHED() PANIC ("OVER SPT_MAX");


void
spte_create(struct file *file, off_t ofs, void* upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable, struct spage_entry **spt){
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

        spte_insert(spte, spt);
        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        ofs += page_read_bytes;
        upage += PGSIZE;
    }
}
void spte_insert(struct spage_entry* spte, struct spage_entry **spt){
    int i=0;
    while(i<SPT_MAX){
        if(spt[i] == NULL){
            spt[i] = spte;
            return;
        }
        i++;
    }
    // NOT_REACHED ();
}

void spt_free(struct spage_entry** spt, uint32_t *pd){
    int i=0;
    while(i<SPT_MAX){
        if(spt[i] != NULL){
            pagedir_clear_page(pd, spt[i]->upage);
            free(spt[i]);
            spt[i] = NULL;
        }
        i++;
    }
}

int write_back(uint32_t *pd, struct mmap_entry *me){
   int write_value = 0;
   uint32_t *mpage;
   uint32_t *start_pg = (uint32_t)me->start_addr & 0xfffff000;
   for(mpage = start_pg; mpage < start_pg + me->file_size + PGSIZE; mpage += PGSIZE){
      if(pagedir_is_dirty(pd, mpage)){
         write_value += file_write_at(me->file, mpage, PGSIZE, mpage - start_pg);
         pagedir_set_dirty(pd, mpage, false);
      }
   }
}

struct spage_entry* get_spte(uint32_t* upage){
   int i=0;
   while(i<SPT_MAX){
      struct spage_entry* spte = thread_current()->spt[i];
      if(spte->upage == upage) return spte;
      i++;
   }
}

struct mmap_entry* get_me(uint32_t *uaddr){
   struct list_elem *e = list_begin(&thread_current()->mmap_table);
   while(e != list_end(&thread_current()->mmap_table)){
      struct mmap_entry *me = list_entry(e, struct mmap_entry, elem);
      // int32_t size = me -> file -> inode -> data.length;
      if (uaddr >= me -> start_addr && uaddr < me->start_addr+me->file_size){
         return me;
      }
      e = list_next(e);
   }
   return NULL;
}
