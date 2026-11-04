#ifndef VASSIAN_SEMA_H
#define VASSIAN_SEMA_H

#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYM_FUNCTION,
    SYM_VARIABLE,
    SYM_PARAMETER,
} SymbolKind;

typedef struct Symbol Symbol;

struct Symbol {
    SymbolKind kind;
    const char* name;
    TypeKind type;
    bool is_mutable;
    bool is_initialized;
    AstNode* decl;
    Symbol* next;
};

typedef struct Scope {
    Symbol* symbols;
    struct Scope* parent;
} Scope;

typedef struct {
    Arena* arena;
    Scope* global_scope;
    Scope* current_scope;
    AstNode* current_function;
    bool had_error;
    bool in_loop;
} Sema;

Sema* sema_create(Arena* arena);
void sema_destroy(Sema* sema);
bool sema_analyze(Sema* sema, AstNode* program);
bool sema_had_error(Sema* sema);

#ifdef __cplusplus
}
#endif

#endif