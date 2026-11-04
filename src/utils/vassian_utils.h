#ifndef VASSIAN_UTILS_H
#define VASSIAN_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* file;
    size_t line;
    size_t column;
} SourceLoc;

typedef struct {
    SourceLoc loc;
    const char* message;
    const char* note;
    const char* help;
    const char* error_code;
} Diagnostic;

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE,
} DiagnosticKind;

typedef struct Arena Arena;

Arena* arena_create(size_t initial_capacity);
void arena_destroy(Arena* arena);
void* arena_alloc(Arena* arena, size_t size);
void* arena_alloc_zero(Arena* arena, size_t size);
char* arena_strdup(Arena* arena, const char* str);
char* arena_strndup(Arena* arena, const char* str, size_t len);

typedef struct StringInterner StringInterner;

StringInterner* interner_create(Arena* arena);
const char* interner_intern(StringInterner* interner, const char* str, size_t len);
const char* interner_intern_cstr(StringInterner* interner, const char* str);
Arena* interner_get_arena(StringInterner* interner);

typedef struct TargetInfo TargetInfo;

TargetInfo* target_info_create(void);
void target_info_destroy(TargetInfo* target);
const char* target_info_get_triple(TargetInfo* target);
const char* target_info_get_cpu(TargetInfo* target);
const char* target_info_get_features(TargetInfo* target);
bool target_info_is_big_endian(TargetInfo* target);
uint64_t target_info_get_pointer_size(TargetInfo* target);

void diag_report(DiagnosticKind kind, const Diagnostic* diag);
void diag_error(const SourceLoc* loc, const char* message, const char* error_code);
void diag_warning(const SourceLoc* loc, const char* message);
void diag_note(const SourceLoc* loc, const char* message);

#ifdef __cplusplus
}
#endif

#endif