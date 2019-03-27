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
