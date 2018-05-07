//#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

//#include "trie.h"

/*
#define OK 1
#define ERROR 0

int lookahead;

int isDigit(int i)
{
    if (i >= '0' && i <= '9')
        return OK;
    return ERROR;
}

int match(int t)
{
    if (t == lookahead)
    {
        lookahead = getchar();
        return OK;
    }
    return ERROR;
}

int term()
{
    if (isDigit(lookahead))
    {
        printf("%c", lookahead);
        return match(lookahead);
    }
    return ERROR;
}

int expr()
{
    if (term() == ERROR)
        return ERROR;
    while (true)
    {
        if (lookahead == '+')
        {
            match('+');
            if (term() == ERROR)
                return ERROR;
            printf("+");
        }
        else if (lookahead == '-')
        {
            match('-');
            if (term() == ERROR)
                return ERROR;
            printf("-");
        }
        else
            return OK;
    }
}
*/

int main(int argc, const char* argv[])
{
    /*
    trie_node_t* root = NULL;

    trie_insert(&root, "david");
    trie_insert(&root, "davu");
    trie_insert(&root, "eva");
    trie_insert(&root, "anna");

    printf("trie contains 'anna': %s\n", trie_contains(root, "anna") ? "yes" : "no");
    printf("trie contains 'banana': %s\n", trie_contains(root, "banana") ? "yes" : "no");
    printf("trie contains 'david': %s\n", trie_contains(root, "david") ? "yes" : "no");
    printf("trie contains 'eva': %s\n", trie_contains(root, "eva") ? "yes" : "no");
    printf("trie contains 'davu': %s\n", trie_contains(root, "davu") ? "yes" : "no");
    printf("trie contains 'dav': %s\n", trie_contains(root, "dav") ? "yes" : "no");
    printf("trie contains 'random': %s\n", trie_contains(root, "random") ? "yes" : "no");
    printf("trie contains 'count': %s\n", trie_contains(root, "count") ? "yes" : "no");

    trie_insert(&root, "dav");

    printf("\n\ntrie contains 'anna': %s\n", trie_contains(root, "anna") ? "yes" : "no");
    printf("trie contains 'banana': %s\n", trie_contains(root, "banana") ? "yes" : "no");
    printf("trie contains 'david': %s\n", trie_contains(root, "david") ? "yes" : "no");
    printf("trie contains 'eva': %s\n", trie_contains(root, "eva") ? "yes" : "no");
    printf("trie contains 'davu': %s\n", trie_contains(root, "davu") ? "yes" : "no");
    printf("trie contains 'dav': %s\n", trie_contains(root, "dav") ? "yes" : "no");
    printf("trie contains 'random': %s\n", trie_contains(root, "random") ? "yes" : "no");
    printf("trie contains 'count': %s\n", trie_contains(root, "count") ? "yes" : "no");

    trie_free(&root);

    return 0;
    */

    /*
    lookahead = getchar();
    int res = expr();
    if (res == ERROR)
        printf("\nsyntax error!");
    return;
    */
    /*if (argc == 2)
    {
        FILE* file = fopen(argv[1], "r");
        if (file)
        {
            int c;
            while ((c = getc(file)) != EOF)
            {
                putchar(c);
            }
            fclose(file);
        }
    }*/
    
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
