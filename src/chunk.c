#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void init_chunk(chunk_t* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;

    init_value_array(&chunk->constants);
}

void free_chunk(chunk_t* chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    free_value_array(&chunk->constants);
    init_chunk(chunk);
}

void write_chunk(chunk_t* chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1)
    {
        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(chunk->code, uint8_t, old_capacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(chunk->lines, int, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void write_constant(chunk_t* chunk, value_t value, int line)
{
    int constant = add_constant(chunk, value);

    if (constant <= 0xFF)
    {
        write_chunk(chunk, OP_CONSTANT, line);
        write_chunk(chunk, (uint8_t)constant, line);
    }
    else
    {
        write_chunk(chunk, OP_CONSTANT_LONG, line);
        write_chunk(chunk, (constant >> 16) & 0xFF, line);
        write_chunk(chunk, (constant >> 8) & 0xFF, line);
        write_chunk(chunk, constant & 0xFF, line);
    }
}

int add_constant(chunk_t* chunk, value_t value)
{
    // check if value already in constants and return reference to that
    for (int i = 0; i < chunk->constants.capacity; ++i)
    {
        if (values_equal(chunk->constants.values[i], value))
        {
            return i;
        }
    }
    write_value_array(&chunk->constants, value);
    return chunk->constants.count - 1;
}

static uint8_t op_short_to_long(uint8_t op)
{
    switch (op)
    {
    case OP_CONSTANT: return OP_CONSTANT_LONG;
    case OP_GET_GLOBAL: return OP_GET_GLOBAL_LONG;
    case OP_SET_GLOBAL: return OP_SET_GLOBAL_LONG;
    case OP_DEFINE_GLOBAL: return OP_DEFINE_GLOBAL_LONG;
    default: return 0;
    }
}

static uint8_t op_long_to_short(uint8_t op)
{
    switch (op)
    {
    case OP_CONSTANT_LONG: return OP_CONSTANT;
    case OP_GET_GLOBAL_LONG: return OP_GET_GLOBAL;
    case OP_SET_GLOBAL_LONG: return OP_SET_GLOBAL;
    case OP_DEFINE_GLOBAL_LONG: return OP_DEFINE_GLOBAL;
    default: return 0;
    }
}

void append_chunk(chunk_t* chunk, chunk_t* other)
{
    int* constantMapping = ALLOCATE(int, other->constants.count);
    // copy constants
    for (int i = 0; i < other->constants.count; ++i)
    {
        constantMapping[i] = add_constant(chunk, other->constants.values[i]);
    }

    for (int i = 0; i < other->count; ++i)
    {
#define APPEND() write_chunk(chunk, other->code[i], other->lines[i])

        uint8_t op = other->code[i];

        switch (op)
        {
            // 2 byte
        case OP_CONSTANT:
        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_DEFINE_GLOBAL: {
            // obacht!! 2 byte instructions could expand to 4 byte!!
            // in case of constant index grows over 256!!!
            int oldIndex = other->code[i + 1];
            int newIndex = constantMapping[oldIndex];
            if (newIndex <= 0xFF)
            {
                APPEND();
                ++i;
                write_chunk(chunk, newIndex & 0xFF, other->lines[i]);
            }
            else
            {
                write_chunk(chunk, op_short_to_long(op), other->lines[i]);
                ++i;
                write_chunk(chunk, (newIndex >> 16) & 0xFF, other->lines[i]);
                write_chunk(chunk, (newIndex >> 8) & 0xFF, other->lines[i]);
                write_chunk(chunk, newIndex & 0xFF, other->lines[i]);
            }
            break;
        }
        case OP_SET_LOCAL:
        case OP_GET_LOCAL:
        case OP_POPN:
            APPEND(); APPEND();
            break;
            // 4 byte
        case OP_CONSTANT_LONG:
        case OP_GET_GLOBAL_LONG:
        case OP_DEFINE_GLOBAL_LONG:
        case OP_SET_GLOBAL_LONG: {
            // obacht, could morph to 2 byte instruction!
            int oldIndex = other->code[i + 1];
            int newIndex = constantMapping[oldIndex];

            if (newIndex <= 0xFF)
            {
                write_chunk(chunk, op_short_to_long(op), other->lines[i]);
                ++i;
                write_chunk(chunk, newIndex & 0xFF, other->lines[i]);
                ++i; ++i;
            }
            else
            {
                APPEND();
                ++i;
                write_chunk(chunk, (newIndex >> 16) & 0xFF, other->lines[i]);
                ++i;
                write_chunk(chunk, (newIndex >> 8) & 0xFF, other->lines[i]);
                ++i;
                write_chunk(chunk, newIndex & 0xFF, other->lines[i]);
            }
            break;
        }
        case OP_JUMP:
        case OP_JUMP_FALSE: {
            APPEND(); ++i;
            APPEND(); ++i;
            APPEND(); ++i;
            APPEND();
            break;
        }
            // 1 byte
        default:
            APPEND();
            break;
        }
#undef APPEND
    }

    FREE_ARRAY(int, constantMapping, other->constants.count);
}
