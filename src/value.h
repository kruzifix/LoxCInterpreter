#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double value_t;

typedef struct {
    int count;
    int capacity;
    value_t* values;
} value_array_t;

void init_value_array(value_array_t* array);
void free_value_array(value_array_t* array);
void write_value_array(value_array_t* array, value_t value);

void print_value(value_t value);

#endif
