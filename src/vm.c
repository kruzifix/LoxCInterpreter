#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

const char* INTERPRET_RESULT_STRING[] = {
    "OK",
    "COMPILE_ERROR",
    "RUNTIME_ERROR"
};

vm_t vm;

static void reset_stack()
{
    vm.stack_top = vm.stack;
}

static void runtime_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code;
    fprintf(stderr, "[line %d] in script\n", vm.chunk->lines[instruction]);

    reset_stack();
}

void init_vm()
{
    reset_stack();
}

void free_vm()
{

}

static value_t peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

static interpret_result_t run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (vm.chunk->constants.values[(READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE()])

#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (false)

    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (value_t* slot = vm.stack; slot < vm.stack_top; slot++)
        {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
        case OP_CONSTANT: {
            value_t constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_NIL: push(NIL_VAL); break;
        case OP_TRUE: push(BOOL_VAL(true)); break;
        case OP_FALSE: push(BOOL_VAL(false)); break;
        case OP_CONSTANT_LONG: {
            value_t constant = READ_CONSTANT_LONG();
            push(constant);
            break;
        }
        case OP_ADD:        BINARY_OP(NUMBER_VAL, +); break;
        case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
        case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
        case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0)))
            {
                runtime_error("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_RETURN: {
            print_value(pop());
            printf("\n");
            return INTERPRET_OK;
        }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

interpret_result_t interpret(const char* source)
{
    chunk_t chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk))
    {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    interpret_result_t result = run();

    free_chunk(&chunk);
    return result;
}

void push(value_t value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

value_t pop()
{
    vm.stack_top--;
    return *vm.stack_top;
}
