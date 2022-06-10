#include "filesys/inode.h"
#include "list.h"
#include "threads/synch.h"
#define BUFFER_LIST_SIZE 64

struct buffer_entry{

    struct inode* inode; // in-memory inode
    block_sector_t sector; //disk block for FCB

    uint8_t* cache;

    int dirty; //0-not dirty, 1-dirty -> need write behind
    uint32_t last_clock;
    struct lock lock;
};

struct buffer_entry* buffer_list[BUFFER_LIST_SIZE];



void init_buffer_cache();
void set_buffer_entry(struct buffer_entry* be, block_sector_t sector, int idx);
void flush_buffer_entry(struct buffer_entry* be);
struct buffer_entry* get_buffer_entry(block_sector_t idx);


//lock 사용됨
void buffer_read(block_sector_t idx, void* buffer_, off_t bytes_read, off_t size, off_t offset);
void buffer_write(block_sector_t idx, void* buffer_, off_t bytes_written, off_t size, off_t offset);
void free_buffer(void);
struct buffer_entry* replace_buffer_entry(block_sector_t sector);

