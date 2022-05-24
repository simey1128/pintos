#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

#include "debug.h"


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

        // list_push_back(&thread_current()->spage_table, &spte->elem);
        spte_insert(spte);
        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        ofs += page_read_bytes;
        upage += PGSIZE;
    }
}
void spte_insert(struct spage_entry* spte){
    struct spage_entry** spage_table = thread_current()->spt;
    int i=0;
    while(i<SPT_MAX){
        if(spage_table[i] == NULL){
            spage_table[i] = spte;
            break;
        }
        i++;
    }
}

void spt_free(struct spage_entry** spt){
    int i=0;
    while(i<SPT_MAX){
        if(spt[i] != NULL){
            pagedir_clear_page(thread_current()->pagedir, spt[i]->upage);
            free(spt[i]);
            spt[i] = NULL;
        }
        i++;
    }
}