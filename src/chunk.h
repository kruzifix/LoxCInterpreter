#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    // 1 byte
    OP_ADD,
    OP_DIVIDE,
    OP_FALSE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_NEGATE,
    OP_NIL,
    OP_NOT,
    OP_POP,
    OP_PRINT,
    OP_RETURN,
    OP_TRUE,
    // 2 byte
    OP_POPN,
    OP_CONSTANT,
    OP_GET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_GLOBAL,
    OP_SET_LOCAL,
    OP_DEFINE_GLOBAL,
    // 4 byte
    OP_CONSTANT_LONG,
    OP_GET_GLOBAL_LONG,
    OP_DEFINE_GLOBAL_LONG,
    OP_SET_GLOBAL_LONG,
    OP_JUMP,
    OP_JUMP_FALSE,
} opcode_t;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    value_array_t constants;
} chunk_t;

void init_chunk(chunk_t* chunk);
void free_chunk(chunk_t* chunk);
void write_chunk(chunk_t* chunk, uint8_t byte, int line);
void write_constant(chunk_t* chunk, value_t value, int line);

int add_constant(chunk_t* chunk, value_t value);

// appends 'other' to 'chunk'
void append_chunk(chunk_t* chunk, chunk_t* other);

#endif
