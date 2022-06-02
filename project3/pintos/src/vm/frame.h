#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"

#define NUM_FRAME_ENTRY 1024
#define FRBITS 12
#define FRSIZE (1 << FRBITS)


typedef uint32_t fid_t;
typedef bool bit;



void ffree(uint32_t *);
uint32_t *falloc(enum palloc_flags);


#endif