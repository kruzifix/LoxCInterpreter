#ifndef clox_memory_h
#define clox_memory_h

#define GROW_CAPACITY(capacity) ((capacity < 8) ? 8 : (capacity) * 2)

#define GROW_ARRAY(previous, type, old_count, count) \
    (type*)reallocate(previous, sizeof(type) * (old_count), sizeof(type) * (count))

void* reallocate(void* previous, size_t old_size, size_t new_size);

#endif
