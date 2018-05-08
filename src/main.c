#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[])
{
    init_vm();

    chunk_t chunk;
    init_chunk(&chunk);

    write_constant(&chunk, 1.2, 123);
    write_constant(&chunk, 3.4, 123);
    write_chunk(&chunk, OP_ADD, 123);

    write_constant(&chunk, 5.6, 123);
    write_chunk(&chunk, OP_DIVIDE, 123);

    write_chunk(&chunk, OP_NEGATE, 123);

    write_chunk(&chunk, OP_RETURN, 123);

    disassemble_chunk(&chunk, "test chunk");

    printf("\n\n--- running code ---\n\n");

    interpret(&chunk);

    free_vm();
    free_chunk(&chunk);
}
