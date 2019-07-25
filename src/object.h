#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_METHOD(value) is_obj_type(value, OBJ_METHOD)

#define AS_STRING(value) ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)

#define AS_METHOD(value) ((obj_method_t*)AS_OBJ(value))

typedef enum {
    OBJ_STRING,
    OBJ_METHOD
} obj_type_t;

extern const char* OBJ_TYPE_STRING[];

struct sobj_t {
    obj_type_t type;
    struct sobj_t* next;
};

struct sobj_string_t {
    obj_t obj;
    int length;
    char* chars;
    uint32_t hash;
};

struct sobj_method_t {
    obj_t obj;
    obj_string_t* name;
    chunk_t* chunk;
};

obj_string_t* take_string(char* chars, int length);
obj_string_t* copy_string(const char* chars, int length);

obj_method_t* create_method(const char* name, int nameLength, chunk_t* chunk);

void print_object(value_t value);

static inline bool is_obj_type(value_t value, obj_type_t type)
{
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif
