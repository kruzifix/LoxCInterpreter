#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"

typedef struct {
    chunk_t* chunk;
} vm_t;

void init_vm();
void free_vm();

#endif
