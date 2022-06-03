#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

#include "debug.h"

#define NOT_REACHED() PANIC ("OVER SPT_MAX");


bool
vte_create(struct file *file, off_t ofs, void* upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable){
    ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT (pg_ofs (upage) == 0);
    ASSERT (ofs % PGSIZE == 0);

    while (read_bytes > 0 || zero_bytes > 0) {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        struct vpage_entry *vte = malloc(sizeof(*vte));
        vte -> type = SUP_PAGE;
        vte -> vaddr = upage;
        vte -> upage = upage;
        vte -> writable = writable;
        vte -> on_frame = false;
        vte -> swap_disable = false;

        vte -> file = file;
        vte -> ofs = ofs;
        vte -> read_bytes = page_read_bytes;
        vte -> zero_bytes = page_zero_bytes;

        list_push_back(&thread_current()->vpage_table, &vte->elem);
        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        ofs += page_read_bytes;
        upage += PGSIZE;
    }
    return true;
}

void vt_free(){
    struct thread *t = thread_current();
    uint32_t *pd = t->pagedir;
    struct list_elem *e = list_begin(&t->vpage_table);
    while(e != list_end(&t->vpage_table)){
        lock_acquire(&swap_lock);
        struct vpage_entry *vte = list_entry(e, struct vpage_entry, elem);
        list_remove(&vte->elem);
        pagedir_clear_page(pd, vte->upage);
        palloc_free_page(pagedir_get_page(pd, vte->upage));
        // free(vte);
        lock_release(&swap_lock);

        e = list_next(e);
    }
}

int write_back(uint32_t *pd, struct vpage_entry *vte){
   int write_value = 0;
    if(pagedir_is_dirty(pd, vte->upage)){
        file_write_at(vte->file, vte->upage, vte->read_bytes, vte->ofs);
        pagedir_clear_page(pd, vte->upage);
    }
}

struct vpage_entry *get_vte(uint32_t *vaddr){
    struct thread *t = thread_current();
    struct list_elem *e = list_begin(&t->vpage_table);
    while(e != list_end(&t->vpage_table)){
        struct vpage_entry *vte = list_entry(e, struct vpage_entry, elem);
        if(vte -> vaddr == vaddr){
            return vte;
        }
        e = list_next(e);
    }
    return NULL;
}



