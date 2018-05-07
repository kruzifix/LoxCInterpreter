#include <stdlib.h>
#include <stdio.h>

#include "trie.h"

void trie_insert(trie_node_t** root, const char* string, int value)
{
    if (!*string)
        return;
    trie_node_t* node = *root;
    if (!node)
    {
        node = malloc(sizeof(trie_node_t));
        if (!node)
            return;
        node->symbol = *string;
        node->child = NULL;
        node->next = NULL;
        node->value = -1;
        *root = node;
    }
    if (*string < node->symbol)
    {
        trie_node_t* prev = NULL;
        trie_insert(&prev, string, value);
        prev->next = node;
        *root = prev;
    }
    else if (*string == node->symbol)
    {
        const char* next = ++string;
        if (*next)
            trie_insert(&(node->child), next, value);
        else
            node->value = value;
    }
    else if (*string > node->symbol)
    {
        trie_insert(&(node->next), string, value);
    }
}

int trie_contains(const trie_node_t* root, const char* string)
{
    const trie_node_t* node = root;
    const trie_node_t* last = node;

    while (node && *string)
    {
        while (node && *string > node->symbol)
            node = node->next;
        if (!node || node->symbol != *string)
            return -1;
        last = node;
        node = node->child;
        ++string;
    }

    return last ? last->value : -1;
}

void trie_free(trie_node_t** root)
{
    trie_node_t* node = *root;
    if (!node)
        return;
    trie_free(&(node->child));
    trie_free(&(node->next));

    free(node);
    *root = NULL;
}
