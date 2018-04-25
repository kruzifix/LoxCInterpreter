#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

vm_t vm;

void init_vm()
{

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
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
        case OP_CONSTANT: {
            value_t constant = READ_CONSTANT();
            print_value(constant);
            printf("\n");
            break;
        }
        case OP_CONSTANT_LONG: {
            value_t constant = READ_CONSTANT_LONG();
            print_value(constant);
            printf("\n");
            break;
        }
        case OP_RETURN: {
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
