#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

typedef enum {
    OP_RETURN
} opcode_t;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
} chunk_t;

void init_chunk(chunk_t* chunk);

#endif
