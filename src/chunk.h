#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_POPN,
    OP_GET_LOCAL,
    OP_GET_GLOBAL,
    OP_GET_GLOBAL_LONG,
    OP_DEFINE_GLOBAL,
    OP_DEFINE_GLOBAL_LONG,
    OP_SET_LOCAL,
    OP_SET_GLOBAL,
    OP_SET_GLOBAL_LONG,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_CONSTANT_LONG,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN
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

#endif
