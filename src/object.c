#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

const char* OBJ_TYPE_STRING[] = {
    "function",
    "native",
    "string",
    "array"
};

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

obj_native_t* new_native(native_func_t func)
{
    obj_native_t* nat = ALLOCATE_OBJ(obj_native_t, OBJ_NATIVE);
    nat->function = func;
    return nat;
}

obj_array_t* new_array(int size)
{
    obj_array_t* arr = ALLOCATE_OBJ(obj_array_t, OBJ_ARRAY);
    init_value_array_size(&(arr->array), size);
    return arr;
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
    case OBJ_FUNCTION: {
        obj_function_t* func = AS_FUNCTION(value);
        printf("<fn %s>", func->name != NULL ? func->name->chars : "SCRIPT");
        break;
    }
    case OBJ_NATIVE: {
        printf("<native fn>");
        break;
    }
    case OBJ_ARRAY: {
        value_array_t* ar = &(AS_ARRAY(value)->array);
        printf("[");
        for (int i = 0; i < ar->count; i++)
        {
            print_value(ar->values[i]);
            if (i < ar->count - 1)
                printf(", ");
        }
        printf("]");
        break;
    }
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    }
}
