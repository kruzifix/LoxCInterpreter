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
