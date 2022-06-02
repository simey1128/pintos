#include <list.h>
#include "filesys/off_t.h"

typedef int mapid_t;

struct vpage{
    uint32_t *pd;
    uint32_t *upage;
    uint32_t *kpage;

    bool on_frame;
    bool swap_disable;
    bool writable;

    struct list_elem elem;
};

struct spage_entry{
    struct vpage vpage;
    
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
};

struct mmap_entry{
    struct vpage vpage;

    mapid_t mapid;
    struct file *file;
    int file_size;
    uint32_t *start_addr;

};

struct stack_entry{
    struct vpage vpage;
};

void spte_create(struct file *file, off_t ofs, void *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void spt_free(void);
void stack_free(void);
void update_pte(uint32_t* pd, uint32_t* upage);

struct spage_entry* get_spte(uint32_t *, uint32_t*);
struct mmap_entry *get_me(uint32_t *, uint32_t*);
struct stack_entry *get_stke(uint32_t *, uint32_t *);
struct vpage* get_vpage(uint32_t *pd, uint32_t *kpage);
