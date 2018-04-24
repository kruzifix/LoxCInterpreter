#include <stdio.h>

#include "debug.h"

void disassemble_chunk(chunk_t* chunk, const char* name)
{
    printf("=== %s ===\n", name);

    for (int i = 0; i < chunk->count;)
    {
        i = disassemble_instruction(chunk, i);
    }
}

static int simple_instruction(const char* name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

int disassemble_instruction(chunk_t* chunk, int offset)
{
    printf("%04d ", offset);

    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
    case OP_RETURN:
        return simple_instruction("OP_RETURN", offset);
        break;
    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
}
