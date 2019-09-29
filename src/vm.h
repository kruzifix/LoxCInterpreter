#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    obj_closure_t* closure;
    uint8_t* ip;
    value_t* slots;
} call_frame_t;

typedef struct {
    const char* file_path;
    bool trace_execution;
    bool print_disassembly;
} interpreter_params_t;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} interpret_result_t;

const char* INTERPRET_RESULT_STRING[];

typedef struct {
    call_frame_t frames[FRAMES_MAX];
    int frame_count;

    value_t stack[STACK_MAX];
    value_t* stack_top;

    table_t globals;
    table_t strings;
    obj_upvalue_t* openUpvalues;

    obj_t* objects;
} vm_t;

extern vm_t vm;

void init_vm(void);
void free_vm(void);

interpret_result_t interpret(const char* source, interpreter_params_t* params);

void push(value_t value);
value_t pop(void);

#endif
