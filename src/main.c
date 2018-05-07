#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

#include "trie.h"

typedef enum {
    TOK_EOF,
    TOK_OP,
    TOK_NUM,
    TOK_ID,
    TOK_FALSE,
    TOK_TRUE
} token_type_t;

typedef struct {
    token_type_t type;
    union {
        int value;
        char* lexeme;
    };
} token_t;

char* read_file(const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = malloc(length + 1);
    if (buffer)
    {
        memset(buffer, '\0', length + 1);
        fread(buffer, 1, length, f);
    }
    fclose(f);
    return buffer;
}

#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_LETTER(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) >= 'Z'))

int line = 1;
char* peek = NULL;
trie_node_t* words = NULL;

void scan(token_t* tok)
{
    for (; *peek; ++peek)
    {
        if (*peek == ' ' || *peek == '\t' || *peek == '\r')
            continue;
        else if (*peek == '\n')
            line++;
        else
            break;
    }

    if (!*peek)
    {
        tok->type = TOK_EOF;
        return;
    }

    if (IS_DIGIT(*peek))
    {
        int v = 0;
        do {
            v = 10 * v + (*peek - '0');
            ++peek;
        } while (IS_DIGIT(*peek));

        tok->type = TOK_NUM;
        tok->value = v;
        return;
    }
    else if (IS_LETTER(*peek))
    {
        char* start = peek;
        do {
            ++peek;
        } while (IS_LETTER(*peek) || IS_DIGIT(*peek));

        size_t len = (peek - start);
        char* word = malloc(len + 1);
        memset(word, '\0', len + 1);
        strncpy(word, start, len);

        int type = trie_contains(words, word);

        if (type >= 0)
        {
            free(word);
            tok->type = type;
            return;
        }

        trie_insert(&words, word, TOK_ID);

        tok->type = TOK_ID;
        tok->lexeme = word;
        return;
    }

    tok->type = TOK_OP;
    tok->value = *peek++;
}

int main(int argc, const char* argv[])
{
    if (argc != 2)
    {
        printf("expected file path\n");
    }

    char* content = read_file(argv[1]);
    if (!content)
    {
        printf("empty file\n");
        return 1;
    }
    peek = content;
    trie_insert(&words, "true", TOK_TRUE);
    trie_insert(&words, "false", TOK_FALSE);

    token_t tok;
    do {
        scan(&tok);
        switch (tok.type)
        {
        case TOK_ID:
            printf("<ID, %s>\n", tok.lexeme);
            free(tok.lexeme);
            break;
        case TOK_NUM:
            printf("<NUM, %i>\n", tok.value);
            break;
        case TOK_EOF:
            printf("<EOF>\n");
            break;
        case TOK_OP:
            printf("<OP, %c>\n", tok.value);
            break;
        case TOK_FALSE:
            printf("<false>\n");
            break;
        case TOK_TRUE:
            printf("<true>\n");
            break;
        default:
            printf("<%c>\n", tok.value);
            break;
        }
    } while (tok.type != TOK_EOF);

    free(content);
    trie_free(&words);

    return 0;

    /*
    trie_node_t* root = NULL;

    trie_insert(&root, "david", 5);
    trie_insert(&root, "davu", 3);
    trie_insert(&root, "eva", 2);
    trie_insert(&root, "anna", 10);

    printf("trie contains 'anna': %i\n", trie_contains(root, "anna"));
    printf("trie contains 'banana': %i\n", trie_contains(root, "banana"));
    printf("trie contains 'david': %i\n", trie_contains(root, "david"));
    printf("trie contains 'eva': %i\n", trie_contains(root, "eva"));
    printf("trie contains 'davu': %i\n", trie_contains(root, "davu"));
    printf("trie contains 'dav': %i\n", trie_contains(root, "dav"));
    printf("trie contains 'random': %i\n", trie_contains(root, "random"));
    printf("trie contains 'count': %i\n", trie_contains(root, "count"));

    trie_free(&root);

    return 0;
    */

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
