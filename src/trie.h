#ifndef clox_trie_h
#define clox_trie_h

typedef struct _trie_node_t trie_node_t;

struct _trie_node_t {
    char symbol;
    int value;

    trie_node_t* child;
    trie_node_t* next;
};

void trie_insert(trie_node_t** root, const char* string, int value);
int trie_contains(const trie_node_t* root, const char* string);

void trie_free(trie_node_t** root);

#endif
