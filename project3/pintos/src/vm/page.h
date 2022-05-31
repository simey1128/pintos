#include <list.h>
#include "filesys/off_t.h"

typedef int mapid_t;

struct spage_entry{
    uint32_t *upage;

    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    struct list_elem elem;
};

struct mmap_entry{
    mapid_t mapid;
    struct file *file;
    int file_size;
    uint32_t *start_addr;

    struct list_elem elem;
};

void spte_create(struct file *file, off_t ofs, void *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void spt_free(void);
void update_pte(uint32_t* pd, uint32_t* upage);
int write_back(uint32_t *pd, struct mmap_entry *me);

struct spage_entry* get_spte(uint32_t* uaddr);
struct mmap_entry *get_me(uint32_t* uaddr);
