#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} interpret_result_t;

const char* INTERPRET_RESULT_STRING[];

typedef struct {
    chunk_t* chunk;
    uint8_t* ip;

    value_t stack[STACK_MAX];
    value_t* stack_top;

    obj_t* objects;
} vm_t;

extern vm_t vm;

void init_vm(void);
void free_vm(void);

interpret_result_t interpret(const char* source);

void push(value_t value);
value_t pop(void);

#endif
