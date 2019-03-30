#include <stdio.h>

#include "debug.h"
#include "value.h"
#include "object.h"

void disassemble_chunk(chunk_t* chunk, const char* name)
{
    printf("=== %s ===\n", name);

    printf("\n-- constants --\n");

    for (int i = 0; i < chunk->constants.count; ++i)
    {
        value_t v = chunk->constants.values[i];

        printf("%3d: %-8s", i, VALUE_TYPE_STRING[v.type]);

        if (IS_OBJ(v))
        {
            printf("[%s]\t'", OBJ_TYPE_STRING[v.as.obj->type]);
        }
        else
        {
            printf("\t\t'");
        }

        print_value(v);

        printf("'\n");
    }

    printf("\n\n");

    for (int i = 0; i < chunk->count;)
    {
        i = disassemble_instruction(chunk, i);
    }
}

static int constant_instruction(const char* name, chunk_t* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int constant_long_instruction(const char* name, chunk_t* chunk, int offset)
{
    int constant = (chunk->code[offset + 1] << 16) |
        (chunk->code[offset + 2] << 8) |
        (chunk->code[offset + 3]);
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}

static int simple_instruction(const char* name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int byte_instruction(const char* name, chunk_t* chunk, int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int multi_byte_instruction(const char* name, chunk_t* chunk, int offset)
{
    int value = (chunk->code[offset + 1] << 16) |
        (chunk->code[offset + 2] << 8) |
        (chunk->code[offset + 3]);
    printf("%-16s %4d\n", name, value);
    return offset + 4;
}

int disassemble_instruction(chunk_t* chunk, int offset)
{
    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
    {
        printf("   | ");
    }
    else
    {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
    case OP_CONSTANT:
        return constant_instruction("OP_CONSTANT", chunk, offset);
    case OP_NIL:
        return simple_instruction("OP_NIL", offset);
    case OP_TRUE:
        return simple_instruction("OP_TRUE", offset);
    case OP_FALSE:
        return simple_instruction("OP_FALSE", offset);
    case OP_POP:
        return simple_instruction("OP_POP", offset);
    case OP_POPN:
        return byte_instruction("OP_POPN", chunk, offset);
    case OP_GET_LOCAL:
        return byte_instruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
        return byte_instruction("OP_SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
        return constant_instruction("OP_GET_GLOBAL", chunk, offset);
    case OP_GET_GLOBAL_LONG:
        return constant_long_instruction("OP_GET_GLOBAL_LONG", chunk, offset);
    case OP_DEFINE_GLOBAL:
        return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL_LONG:
        return constant_long_instruction("OP_DEFINE_GLOBAL_LONG", chunk, offset);
    case OP_SET_GLOBAL:
        return constant_instruction("OP_SET_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL_LONG:
        return constant_long_instruction("OP_SET_GLOBAL_LONG", chunk, offset);
    case OP_JUMP_FALSE:
        return multi_byte_instruction("OP_JUMP_FALSE", chunk, offset);
    case OP_JUMP:
        return multi_byte_instruction("OP_JUMP", chunk, offset);
    case OP_EQUAL:
        return simple_instruction("OP_EQUAL", offset);
    case OP_GREATER:
        return simple_instruction("OP_GREATER", offset);
    case OP_LESS:
        return simple_instruction("OP_LESS", offset);
    case OP_CONSTANT_LONG:
        return constant_long_instruction("OP_CONSTANT_LONG", chunk, offset);
    case OP_ADD:
        return simple_instruction("OP_ADD", offset);
    case OP_SUBTRACT:
        return simple_instruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
        return simple_instruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
        return simple_instruction("OP_DIVIDE", offset);
    case OP_NOT:
        return simple_instruction("OP_NOT", offset);
    case OP_NEGATE:
        return simple_instruction("OP_NEGATE", offset);
    case OP_PRINT:
        return simple_instruction("OP_PRINT", offset);
    case OP_RETURN:
        return simple_instruction("OP_RETURN", offset);
    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
}
