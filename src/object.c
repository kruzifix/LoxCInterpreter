#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) (type*)allocate_object(sizeof(type), objectType)

static obj_t* allocate_object(size_t size, obj_type_t type)
{
    obj_t* obj = (obj_t*)reallocate(NULL, 0, size);
    obj->type = type;

    return obj;
}

static obj_string_t* allocate_string(char* chars, int length)
{
    obj_string_t* str = ALLOCATE_OBJ(obj_string_t, OBJ_STRING);
    str->chars = chars;
    str->length = length;

    return str;
}

obj_string_t* take_string(char* chars, int length)
{
    return allocate_string(chars, length);
}

obj_string_t* copy_string(const char* chars, int length)
{
    char* heapChars = ALLOCATE(char, length + 1);

    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocate_string(heapChars, length);
}

void print_object(value_t value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    }
}
