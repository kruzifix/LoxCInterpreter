#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[])
{
    chunk_t chunk;
    init_chunk(&chunk);
    int constant = add_constant(&chunk, 13.5);

    write_chunk(&chunk, OP_CONSTANT);
    write_chunk(&chunk, constant);

    write_chunk(&chunk, OP_RETURN);

    disassemble_chunk(&chunk, "test chunk");

    free_chunk(&chunk);
}
