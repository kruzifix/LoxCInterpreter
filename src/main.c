#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

#include <vld.h>

#define _EXIT(code) { free_vm(); exit(code); }

static void repl(interpreter_params_t* params)
{
    char line[1024];

    for (;;)
    {
        printf("> ");

        if (!fgets(line, 1024, stdin))
        {
            printf("\n");
            break;
        }

        interpret_result_t result = interpret(line, params);
        printf("\nresult: %s\n", INTERPRET_RESULT_STRING[result]);
    }
}

static char* read_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        _EXIT(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = malloc(file_size + 1);
    if (!buffer)
    {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        _EXIT(74);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size)
    {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        _EXIT(74);
    }

    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

static void run_file(interpreter_params_t* params)
{
    char* source = read_file(params->file_path);
    interpret_result_t result = interpret(source, params);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) _EXIT(65);
    if (result == INTERPRET_RUNTIME_ERROR) _EXIT(70);
}

/*
int main_simple(int argc, const char* argv[])
{
    init_vm();
    if (argc == 1)
    {
        repl();
    }
    else if (argc == 2)
    {
        run_file(argv[1]);
    }
    else
    {
        fprintf(stderr, "Usage: clox [path]\n");
        _EXIT(64);
    }

    free_vm();
}
*/

int main(int argc, const char* argv[])
{
    // arguments:
    // path  file path
    // -te  trace execution
    // -pd  print disassembly

    interpreter_params_t params;
    params.file_path = NULL;
    params.trace_execution = false;
    params.print_disassembly = false;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp("-te", argv[i]) == 0)
        {
            params.trace_execution = true;
        }
        else if (strcmp("-pd", argv[i]) == 0)
        {
            params.print_disassembly = true;
        }
        else if (argv[i][0] != '-' && params.file_path == NULL)
        {
            params.file_path = argv[i];
        }
        else
        {
            printf("unknown parameter '%s'\n", argv[i]);
            printf("usage: clox [path] [-te] [-pd]\n");
            return 1;
        }
    }

    init_vm();

    if (params.file_path == NULL)
    {
        repl(&params);
    }
    else
    {
        run_file(&params);
    }

    free_vm();

    return 0;
}
