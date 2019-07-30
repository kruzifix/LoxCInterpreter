#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) (type*)allocate_object(sizeof(type), objectType)

static obj_t* allocate_object(size_t size, obj_type_t type)
{
    obj_t* obj = (obj_t*)reallocate(NULL, 0, size);
    obj->type = type;

    obj->next = vm.objects;
    vm.objects = obj;

    return obj;
}


static obj_string_t* allocate_string(char* chars, int length, uint32_t hash)
{
    obj_string_t* str = ALLOCATE_OBJ(obj_string_t, OBJ_STRING);
    str->chars = chars;
    str->length = length;
    str->hash = hash;

    table_set(&vm.strings, str, NIL_VAL);

    return str;
}

// FNV-1a
static uint32_t hash_string(const char* key, int length)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; ++i)
    {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

obj_function_t* new_function(void)
{
    obj_function_t* func = ALLOCATE_OBJ(obj_function_t, OBJ_FUNCTION);

    func->arity = 0;
    func->name = NULL;
    init_chunk(&(func->chunk));
    return func;
}

obj_string_t* take_string(char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    obj_string_t* interned = table_find_string(&vm.strings, chars, length, hash);

    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(chars, length, hash);
}

obj_string_t* copy_string(const char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    obj_string_t* interned = table_find_string(&vm.strings, chars, length, hash);

    if (interned != NULL)
        return interned;

    char* heapChars = ALLOCATE(char, length + 1);

    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocate_string(heapChars, length, hash);
}

void print_object(value_t value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJ_FUNCTION:
        printf("<fn %s>", AS_FUNCTION(value)->name->chars);
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    }
}
