#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_RETURN
} opcode_t;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    value_array_t constants;
} chunk_t;

void init_chunk(chunk_t* chunk);
void free_chunk(chunk_t* chunk);
void write_chunk(chunk_t* chunk, uint8_t byte);
int add_constant(chunk_t* chunk, value_t value);

#endif
