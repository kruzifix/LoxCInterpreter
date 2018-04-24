#include <stdlib.h>

#include "chunk.h"

void init_chunk(chunk_t* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
}
