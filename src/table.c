#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"

void init_table(table_t* table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void free_table(table_t* table)
{
    FREE_ARRAY(entry_t, table->entries, table->capacity);
    init_table(table);
}

bool table_set(table_t* table, obj_string_t* key, value_t value)
{
    entry_t* entry = find_entry(table->entries, table->capacity, key);

    bool newKey = entry->key == NULL;
    if (newKey)
    {
        table->count++;
    }

    entry->key = key;
    entry->value = value;
    return newKey;
}
