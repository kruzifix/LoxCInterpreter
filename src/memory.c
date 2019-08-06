#include <stdlib.h>

#include "common.h"
#include "memory.h"
#include "vm.h"

void* reallocate(void* previous, size_t old_size, size_t new_size)
{
    if (new_size == 0)
    {
        free(previous);
        return NULL;
    }

    return realloc(previous, new_size);
}

static void free_object(obj_t* obj)
{
    switch (obj->type)
    {
    case OBJ_FUNCTION: {
        obj_function_t* func = (obj_function_t*)obj;
        free_chunk(&(func->chunk));
        FREE(obj_function_t, func);
        break;
    }
    case OBJ_NATIVE: {
        FREE(obj_native_t, obj);
        break;
    }
    case OBJ_LIST: {
        obj_list_t* arr = (obj_list_t*)obj;
        free_value_array(&(arr->array));
        FREE(obj_list_t, arr);
        break;
    }
    case OBJ_MAP: {
        obj_map_t* map = (obj_map_t*)obj;
        free_table(&(map->table));
        FREE(obj_map_t, map);
        break;
    }
    case OBJ_STRING: {
        obj_string_t* str = (obj_string_t*)obj;
        FREE_ARRAY(char, str->chars, str->length + 1);
        FREE(obj_string_t, obj);
        break;
    }
    }
}

void free_objects()
{
    obj_t* node = vm.objects;
    while (node != NULL)
    {
        obj_t* next = node->next;
        free_object(node);
        node = next;
    }
}
