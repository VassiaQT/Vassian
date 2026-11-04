#include "../src/utils/vassian_utils.h"
#include <assert.h>
#include <string.h>

void test_arena_basic() {
    Arena* arena = arena_create(1024);
    assert(arena != NULL);

    void* ptr1 = arena_alloc(arena, 16);
    assert(ptr1 != NULL);
    memset(ptr1, 0xAA, 16);

    void* ptr2 = arena_alloc(arena, 32);
    assert(ptr2 != NULL);
    assert(ptr2 != ptr1);
    memset(ptr2, 0xBB, 32);

    void* ptr3 = arena_alloc_zero(arena, 8);
    assert(ptr3 != NULL);
    for (int i = 0; i < 8; i++) {
        assert(((uint8_t*)ptr3)[i] == 0);
    }

    arena_destroy(arena);
}

void test_arena_strdup() {
    Arena* arena = arena_create(1024);
    assert(arena != NULL);

    char* s1 = arena_strdup(arena, "hello");
    assert(s1 != NULL);
    assert(strcmp(s1, "hello") == 0);

    char* s2 = arena_strndup(arena, "world!", 5);
    assert(s2 != NULL);
    assert(strcmp(s2, "world") == 0);

    arena_destroy(arena);
}

void test_arena_growth() {
    Arena* arena = arena_create(64);
    assert(arena != NULL);

    for (int i = 0; i < 100; i++) {
        void* ptr = arena_alloc(arena, 100);
        assert(ptr != NULL);
    }

    arena_destroy(arena);
}

void test_interner() {
    Arena* arena = arena_create(1024);
    StringInterner* interner = interner_create(arena);
    assert(interner != NULL);

    const char* s1 = interner_intern_cstr(interner, "test");
    const char* s2 = interner_intern_cstr(interner, "test");
    const char* s3 = interner_intern_cstr(interner, "different");

    assert(s1 == s2);
    assert(s1 != s3);
    assert(strcmp(s1, "test") == 0);
    assert(strcmp(s3, "different") == 0);

    arena_destroy(arena);
}

int main() {
    test_arena_basic();
    test_arena_strdup();
    test_arena_growth();
    test_interner();
    printf("All unit tests passed!\n");
    return 0;
}