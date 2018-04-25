#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[])
{
    chunk_t chunk;
    init_chunk(&chunk);
    
    for (int i = 0; i < 400; i++)
    {
        write_constant(&chunk, i * 0.1, i);
    }

    write_chunk(&chunk, OP_RETURN, 123);

    disassemble_chunk(&chunk, "test chunk");

    free_chunk(&chunk);
}
