#include "ast.h"
#include <stdlib.h>
#include <string.h>

static AstNode* ast_alloc(Arena* arena, AstKind kind, SourceLoc loc) {
    AstNode* node = (AstNode*)arena_alloc_zero(arena, sizeof(AstNode));
    if (!node) return NULL;
    node->kind = kind;
    node->loc = loc;
    return node;
}

AstNode* ast_create_program(Arena* arena, SourceLoc loc) {
    return ast_alloc(arena, AST_PROGRAM, loc);
}

AstNode* ast_create_fn_decl(Arena* arena, SourceLoc loc, const char* name) {
    AstNode* node = ast_alloc(arena, AST_FN_DECL, loc);
    if (node) node->as.fn_decl.name = name;
    return node;
}

AstNode* ast_create_param(Arena* arena, SourceLoc loc, const char* name) {
    AstNode* node = ast_alloc(arena, AST_PARAM, loc);
    if (node) node->as.param.name = name;
    return node;
}

AstNode* ast_create_var_decl(Arena* arena, SourceLoc loc, const char* name) {
    AstNode* node = ast_alloc(arena, AST_VAR_DECL, loc);
    if (node) node->as.var_decl.name = name;
    return node;
}

AstNode* ast_create_block(Arena* arena, SourceLoc loc) {
    return ast_alloc(arena, AST_BLOCK, loc);
}

AstNode* ast_create_return(Arena* arena, SourceLoc loc) {
    return ast_alloc(arena, AST_RETURN, loc);
}

AstNode* ast_create_expr_stmt(Arena* arena, SourceLoc loc) {
    return ast_alloc(arena, AST_EXPR_STMT, loc);
}

AstNode* ast_create_if(Arena* arena, SourceLoc loc) {
    return ast_alloc(arena, AST_IF, loc);
}

AstNode* ast_create_while(Arena* arena, SourceLoc loc) {
    return ast_alloc(arena, AST_WHILE, loc);
}

AstNode* ast_create_binary(Arena* arena, SourceLoc loc, TokenKind op) {
    AstNode* node = ast_alloc(arena, AST_BINARY, loc);
    if (node) node->as.binary.op = op;
    return node;
}

AstNode* ast_create_unary(Arena* arena, SourceLoc loc, TokenKind op) {
    AstNode* node = ast_alloc(arena, AST_UNARY, loc);
    if (node) node->as.unary.op = op;
    return node;
}

AstNode* ast_create_call(Arena* arena, SourceLoc loc, const char* name) {
    AstNode* node = ast_alloc(arena, AST_CALL, loc);
    if (node) node->as.call.name = name;
    return node;
}

AstNode* ast_create_ident(Arena* arena, SourceLoc loc, const char* name) {
    AstNode* node = ast_alloc(arena, AST_IDENT, loc);
    if (node) node->as.ident.name = name;
    return node;
}

AstNode* ast_create_literal_int(Arena* arena, SourceLoc loc, int64_t value) {
    AstNode* node = ast_alloc(arena, AST_LITERAL, loc);
    if (node) {
        node->as.literal.type = TYPE_INT;
        node->as.literal.value.int_val = value;
    }
    return node;
}

AstNode* ast_create_literal_float(Arena* arena, SourceLoc loc, double value) {
    AstNode* node = ast_alloc(arena, AST_LITERAL, loc);
    if (node) {
        node->as.literal.type = TYPE_FLOAT;
        node->as.literal.value.float_val = value;
    }
    return node;
}

AstNode* ast_create_literal_string(Arena* arena, SourceLoc loc, const char* value) {
    AstNode* node = ast_alloc(arena, AST_LITERAL, loc);
    if (node) {
        node->as.literal.type = TYPE_INT;
        node->as.literal.value.str_val = value;
    }
    return node;
}

AstNode* ast_create_type(Arena* arena, SourceLoc loc, TypeKind type) {
    AstNode* node = ast_alloc(arena, AST_TYPE, loc);
    if (node) node->as.type_ast.type = type;
    return node;
}

void ast_list_append(Arena* arena, AstList* list, AstNode* node) {
    if (!list || !node) return;
    size_t new_count = list->count + 1;
    AstNode** new_items = (AstNode**)arena_alloc(arena, new_count * sizeof(AstNode*));
    if (!new_items) return;
    if (list->items) {
        memcpy(new_items, list->items, list->count * sizeof(AstNode*));
    }
    new_items[list->count] = node;
    list->items = new_items;
    list->count = new_count;
}

const char* type_kind_to_str(TypeKind kind) {
    switch (kind) {
        case TYPE_VOID: return "void";
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
    }
    return "unknown";
}