#ifndef clox_memory_h
#define clox_memory_h

#include "object.h"

#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity < 8) ? 8 : (capacity) * 2)

#define GROW_ARRAY(previous, type, old_count, count) \
    (type*)reallocate(previous, sizeof(type) * (old_count), sizeof(type) * (count))

#define FREE_ARRAY(type, pointer, old_count) \
    reallocate(pointer, sizeof(type) * (old_count), 0)

void* reallocate(void* previous, size_t old_size, size_t new_size);
void free_objects();

#endif
