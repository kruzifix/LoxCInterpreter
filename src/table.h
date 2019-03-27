#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
    obj_string_t* key;
    value_t value;
} entry_t;

typedef struct {
    int count;
    int capacity;
    entry_t* entries;
} table_t;

void init_table(table_t* table);
void free_table(table_t* table);

bool table_get(table_t* table, obj_string_t* key, value_t* value);
// returns true if new entry was made
bool table_set(table_t* table, obj_string_t* key, value_t value);
// returns true if entry was deleted
bool table_delete(table_t* table, obj_string_t* key);
void table_add_all(table_t* from, table_t* to);
obj_string_t* table_find_string(table_t* table, const char* chars, int length, uint32_t hash);

#endif
