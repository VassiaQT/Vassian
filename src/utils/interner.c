#include "vassian_utils.h"
#include <string.h>

#define INTERNER_INITIAL_BUCKETS 1024

struct StringInterner {
    Arena* arena;
    const char** buckets;
    size_t bucket_count;
    size_t count;
};

static uint64_t hash_string(const char* str, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)str[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

StringInterner* interner_create(Arena* arena) {
    StringInterner* interner = (StringInterner*)arena_alloc_zero(arena, sizeof(StringInterner));
    if (!interner) return NULL;
    interner->arena = arena;
    interner->bucket_count = INTERNER_INITIAL_BUCKETS;
    interner->buckets = (const char**)arena_alloc_zero(arena, interner->bucket_count * sizeof(char*));
    if (!interner->buckets) return NULL;
    return interner;
}

static const char* interner_intern_hash(StringInterner* interner, const char* str, size_t len, uint64_t hash) {
    size_t idx = hash & (interner->bucket_count - 1);
    const char* existing = interner->buckets[idx];
    while (existing) {
        if (strlen(existing) == len && memcmp(existing, str, len) == 0) {
            return existing;
        }
        idx = (idx + 1) & (interner->bucket_count - 1);
        existing = interner->buckets[idx];
    }
    char* copy = (char*)arena_alloc(interner->arena, len + 1);
    if (!copy) return NULL;
    memcpy(copy, str, len);
    copy[len] = '\0';
    interner->buckets[idx] = copy;
    interner->count++;
    return copy;
}

const char* interner_intern(StringInterner* interner, const char* str, size_t len) {
    uint64_t hash = hash_string(str, len);
    return interner_intern_hash(interner, str, len, hash);
}

const char* interner_intern_cstr(StringInterner* interner, const char* str) {
    return interner_intern(interner, str, strlen(str));
}

Arena* interner_get_arena(StringInterner* interner) {
    return interner ? interner->arena : NULL;
}