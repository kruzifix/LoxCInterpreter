#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

vm_t vm;

static void reset_stack()
{
    vm.stack_top = vm.stack;
}

void init_vm()
{
    reset_stack();
}

void free_vm()
{

}

static interpret_result_t run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (vm.chunk->constants.values[(READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE()])

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
        case OP_CONSTANT_LONG: {
            value_t constant = READ_CONSTANT_LONG();
            push(constant);
            break;
        }
        case OP_NEGATE:
            push(-pop());
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
}

interpret_result_t interpret(chunk_t* chunk)
{
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;

    interpret_result_t result = run();
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
