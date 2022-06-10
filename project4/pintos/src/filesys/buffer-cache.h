#include "filesys/inode.h"
#include "list.h"
#define BUFFER_LIST_SIZE 64

struct buffer_entry{

    struct inode* inode; // in-memory inode
    block_sector_t sector; //disk block for FCB

    void* file_data;

    int dirty; //0-not dirty, 1-dirty -> need write behind
    uint32_t last_clock;
    //# TODO - lock?
};

struct buffer_entry* buffer_list[BUFFER_LIST_SIZE];
uint8_t* buffer_cache;
void init_buffer_cache();
struct buffer_entry* get_buffer_entry(block_sector_t idx);
int buffer_read(block_sector_t idx, void* buffer_, off_t bytes_read, off_t size, off_t offset);
