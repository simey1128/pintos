#include <list.h>
#include "filesys/off_t.h"
#include "threads/thread.h""

typedef int mapid_t;
typedef enum entry_type{
    spte,
    me,
    stke,
};

struct spage_entry{
    uint32_t *pd;
    uint32_t *upage;
    uint32_t *kpage;
    bool on_frame;
    bool swap_disable;

    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    struct list_elem elem;
};

struct mmap_entry{
    uint32_t *pd;
    uint32_t *upage;
    uint32_t *kpage;

    mapid_t mapid;
    bool on_frame;
    bool swap_disable;
    bool writable;

    struct file *file;
    int file_size;
    uint32_t *start_addr;

    struct list_elem elem;
};

struct stack_entry{
    uint32_t *pd;
    uint32_t *upage;
    uint32_t *kpage;
    tid_t* tid;

    bool on_frame;
    bool swap_disable;
    bool writable;

    struct list_elem elem;
};

void spte_create(struct file *file, off_t ofs, void *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void spt_free(void);
void all_pt_free(uint32_t* pd);
void update_pte(uint32_t* pd, uint32_t* upage);
int write_back(uint32_t *pd, struct mmap_entry *me);

struct spage_entry* get_spte(uint32_t *, uint32_t*);
struct mmap_entry *get_me(uint32_t *, uint32_t*);
struct stack_entry *get_stke(uint32_t *, uint32_t *);
void me_create(mapid_t mapid, struct file* file, int file_size, uint32_t* start_addr);

void* test(void);