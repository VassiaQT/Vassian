#include "vassian_utils.h"
#include <stdlib.h>
#include <string.h>

struct Arena {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
    Arena* next;
    Arena* current;
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
    arena->current = arena;
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
    Arena* current = arena->current;
    size_t new_capacity = current->capacity * 2;
    while (new_capacity < size) {
        new_capacity *= 2;
    }

    Arena* block = (Arena*)malloc(sizeof(Arena));
    if (!block) return NULL;
    block->buffer = (uint8_t*)malloc(new_capacity);
    if (!block->buffer) {
        free(block);
        return NULL;
    }
    block->capacity = new_capacity;
    block->offset = size;
    block->next = arena->next;
    block->current = block;
    arena->next = block;
    arena->current = block;

    void* ptr = block->buffer;
    if (zero) memset(ptr, 0, size);
    return ptr;
}

void* arena_alloc(Arena* arena, size_t size) {
    size = (size + 7) & ~7;
    Arena* current = arena->current;
    if (current->offset + size <= current->capacity) {
        void* ptr = current->buffer + current->offset;
        current->offset += size;
        return ptr;
    }
    return arena_alloc_slow(arena, size, false);
}

void* arena_alloc_zero(Arena* arena, size_t size) {
    size = (size + 7) & ~7;
    Arena* current = arena->current;
    if (current->offset + size <= current->capacity) {
        void* ptr = current->buffer + current->offset;
        current->offset += size;
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