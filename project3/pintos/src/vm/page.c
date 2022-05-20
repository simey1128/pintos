#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

#include "debug.h"


void
spte_create(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable){
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
        upage += PGSIZE;
    }
}

void spt_free(struct list *spage_table){
    struct list_elem *e;
    for(e = list_begin(&spage_table); e != list_end(&spage_table); e = list_next(e)){
        struct spage_entry *spte = list_entry(e, struct spage_entry, elem);
        free(spte);
    }
}