#include "filesys/inode.h"
#include "list.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#define BUFFER_CACHE_SIZE 64

struct bc_entry{
    struct inode* inode; // in-memory inode
    block_sector_t sector; //disk block for FCB

    uint8_t* cache;

    int dirty; //0-not dirty, 1-dirty -> need write behind
    int new;
    uint32_t last_clock;
    struct lock lock;
};

void buffer_cache_init(void);
void buffer_cache_free(void);
struct bc_entry* replace_bc_entry(block_sector_t sector);

//bc_table_lock 사용함
struct bc_entry* get_bc_entry(block_sector_t idx);
struct bc_entry* get_oldest_bc_entry(void);


//개별 entry lock 사용됨
void buffer_read(block_sector_t idx, void* buffer_, off_t bytes_read, off_t size, off_t offset);
void buffer_write(block_sector_t idx, void* buffer_, off_t bytes_written, off_t size, off_t offset);
void flush_bc_entry(struct bc_entry* bc);
void set_bc_entry(struct bc_entry* bc, block_sector_t sector);

