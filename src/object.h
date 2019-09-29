#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_FUNCTION(value) ((obj_function_t*)AS_OBJ(value))
#define AS_CLOSURE(value) ((obj_closure_t*)AS_OBJ(value))
#define AS_NATIVE(value) (((obj_native_t*)AS_OBJ(value))->function)
#define AS_STRING(value) ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
} obj_type_t;

struct sobj_t {
    obj_type_t type;
    struct sobj_t* next;
};

typedef struct {
    obj_t obj;
    int arity;
    int upvalueCount;
    chunk_t chunk;
    obj_string_t* name;
} obj_function_t;

typedef value_t(*native_func_t)(int argCount, value_t* args);

typedef struct {
    obj_t obj;
    native_func_t function;
} obj_native_t;

struct sobj_string_t {
    obj_t obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct sobj_upvalue_t {
    obj_t obj;
    value_t* location;
    value_t closed;
    struct sobj_upvalue_t* next;
} obj_upvalue_t;

typedef struct {
    obj_t obj;
    obj_function_t* function;
    obj_upvalue_t** upvalues;
    int upvalueCount;
} obj_closure_t;

obj_function_t* new_function(void);
obj_closure_t* new_closure(obj_function_t* function);
obj_native_t* new_native(native_func_t func);
obj_string_t* take_string(char* chars, int length);
obj_string_t* copy_string(const char* chars, int length);
obj_upvalue_t* new_upvalue(value_t* slot);

void print_object(value_t value);

static inline bool is_obj_type(value_t value, obj_type_t type)
{
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif
