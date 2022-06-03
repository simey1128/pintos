#include <list.h>
#include "filesys/off_t.h"

typedef int mapid_t;

enum page_type{
    SUP_PAGE,
    MAP_PAGE,
    REM_PAGE,
};

struct vpage_entry{
    size_t type;
    uint32_t *vaddr;
    uint32_t *upage;
    uint32_t *kpage;
    bool writable;
    bool on_frame;
    bool swap_disable;

    int fd;
    struct file *file;
    int file_size;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;

    struct list_elem elem;
};


void vt_free(void);
bool
vte_create(struct file *file, off_t ofs, void* upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
int write_back(uint32_t *pd, struct vpage_entry *vte);
