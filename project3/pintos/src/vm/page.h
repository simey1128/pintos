#include <list.h>
#include "filesys/off_t.h"


struct spage_entry{
    uint32_t *upage;

    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    struct list_elem elem;
};

struct list spage_table;

void spte_create(struct file *file, off_t ofs, uint32_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void spt_free(struct list *);
void update_pte(uint32_t* pd, uint32_t* upage);