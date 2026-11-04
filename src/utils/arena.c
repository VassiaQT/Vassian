#include "vassian_utils.h"
#include <stdlib.h>
#include <string.h>

struct Arena {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
    Arena* next;
};

#define ARENA_DEFAULT_CAPACITY (64 * 1024)

Arena* arena_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = ARENA_DEFAULT_CAPACITY;
    }
    Arena* arena = (Arena*)malloc(sizeof(Arena));
    if (!arena) return NULL;
    arena->buffer = (uint8_t*)malloc(initial_capacity);
    if (!arena->buffer) {
        free(arena);
        return NULL;
    }
    arena->capacity = initial_capacity;
    arena->offset = 0;
    arena->next = NULL;
    return arena;
}

void arena_destroy(Arena* arena) {
    while (arena) {
        Arena* next = arena->next;
        free(arena->buffer);
        free(arena);
        arena = next;
    }
}

static void* arena_alloc_slow(Arena* arena, size_t size, bool zero) {
    size_t new_capacity = arena->capacity * 2;
    while (new_capacity < arena->offset + size) {
        new_capacity *= 2;
    }
    uint8_t* new_buffer = (uint8_t*)malloc(new_capacity);
    if (!new_buffer) return NULL;
    memcpy(new_buffer, arena->buffer, arena->offset);
    free(arena->buffer);
    arena->buffer = new_buffer;
    arena->capacity = new_capacity;
    void* ptr = arena->buffer + arena->offset;
    arena->offset += size;
    if (zero) memset(ptr, 0, size);
    return ptr;
}

void* arena_alloc(Arena* arena, size_t size) {
    size = (size + 7) & ~7;
    if (arena->offset + size <= arena->capacity) {
        void* ptr = arena->buffer + arena->offset;
        arena->offset += size;
        return ptr;
    }
    return arena_alloc_slow(arena, size, false);
}

void* arena_alloc_zero(Arena* arena, size_t size) {
    size = (size + 7) & ~7;
    if (arena->offset + size <= arena->capacity) {
        void* ptr = arena->buffer + arena->offset;
        arena->offset += size;
        memset(ptr, 0, size);
        return ptr;
    }
    return arena_alloc_slow(arena, size, true);
}

char* arena_strdup(Arena* arena, const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)arena_alloc(arena, len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

char* arena_strndup(Arena* arena, const char* str, size_t len) {
    if (!str) return NULL;
    char* copy = (char*)arena_alloc(arena, len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}