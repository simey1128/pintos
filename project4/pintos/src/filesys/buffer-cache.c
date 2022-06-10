#include "filesys/buffer-cache.h"
#include "filesys/filesys.h"

void init_buffer_cache(){
    buffer_cache = malloc(BLOCK_SECTOR_SIZE * BUFFER_LIST_SIZE);

    int i;
    for(i=0; i<BUFFER_LIST_SIZE; i++){
        buffer_list[i] = NULL;
    }
}
/*
buffer_list에서 idx위치에 있는 buffer_entry 반환
* invalid idx는 idx<0 or buffer_list[idx]가 비어있을 때
* idx가 BUFFER_LIST_SIZE 이상이면, select victim, replace entry
* 그렇지 않으면 buffer_list[idx] 반환
*/
struct buffer_entry* get_buffer_entry(block_sector_t idx){
    if(idx<0 || buffer_list[idx]==NULL) return NULL;
    
    // buffer cache is full #TODO
    if(idx>=BUFFER_LIST_SIZE){

        return buffer_list[idx];
    }
    return buffer_list[idx];
}

int buffer_read(block_sector_t idx, void* _buffer, off_t bytes_read, off_t size, off_t offset){
    struct buffer_entry* be = get_buffer_entry(idx);

    // block_sector_t sector_idx = byte_to_sector (be->inode, offset);
    // int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    // block_read(fs_device, sector_idx, buffer_cache+(BLOCK_SECTOR_SIZE*idx));
    // memcpy(_buffer+bytes_read, buffer_cache+(BLOCK_SECTOR_SIZE*idx)+sector_ofs, size);

    //#TODO clock bit 설정하기

}