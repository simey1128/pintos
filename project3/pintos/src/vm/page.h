#include <list.h>
#include "filesys/off_t.h"


struct spage_entry{
    uint8_t *upage;

    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    struct list_elem elem;
};

struct list spage_table;

void spte_create(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void spt_free(struct list *);