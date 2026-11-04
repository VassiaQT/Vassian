#ifndef VASSIAN_AST_H
#define VASSIAN_AST_H

#include "vassian_utils.h"
#include "lexer/lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AST_PROGRAM,
    AST_FN_DECL,
    AST_PARAM,
    AST_VAR_DECL,
    AST_BLOCK,
    AST_RETURN,
    AST_EXPR_STMT,
    AST_IF,
    AST_WHILE,
    AST_BINARY,
    AST_UNARY,
    AST_CALL,
    AST_IDENT,
    AST_LITERAL,
    AST_TYPE,
} AstKind;

typedef enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
} TypeKind;

typedef struct AstNode AstNode;

typedef struct {
    AstNode** items;
    size_t count;
} AstList;

struct AstNode {
    AstKind kind;
    SourceLoc loc;
    union {
        struct { AstList decls; } program;
        struct { const char* name; AstList params; AstNode* ret_type; AstNode* body; } fn_decl;
        struct { const char* name; AstNode* type; } param;
        struct { const char* name; AstNode* type; AstNode* init; } var_decl;
        struct { AstList stmts; } block;
        struct { AstNode* expr; } return_stmt;
        struct { AstNode* expr; } expr_stmt;
        struct { AstNode* cond; AstNode* then_branch; AstNode* else_branch; } if_stmt;
        struct { AstNode* cond; AstNode* body; } while_stmt;
        struct { TokenKind op; AstNode* lhs; AstNode* rhs; } binary;
        struct { TokenKind op; AstNode* expr; } unary;
        struct { const char* name; AstList args; } call;
        struct { const char* name; } ident;
        struct { TypeKind type; union { int64_t int_val; double float_val; const char* str_val; } value; } literal;
        struct { TypeKind type; } type_ast;
    } as;
};

AstNode* ast_create_program(Arena* arena, SourceLoc loc);
AstNode* ast_create_fn_decl(Arena* arena, SourceLoc loc, const char* name);
AstNode* ast_create_param(Arena* arena, SourceLoc loc, const char* name);
AstNode* ast_create_var_decl(Arena* arena, SourceLoc loc, const char* name);
AstNode* ast_create_block(Arena* arena, SourceLoc loc);
AstNode* ast_create_return(Arena* arena, SourceLoc loc);
AstNode* ast_create_expr_stmt(Arena* arena, SourceLoc loc);
AstNode* ast_create_if(Arena* arena, SourceLoc loc);
AstNode* ast_create_while(Arena* arena, SourceLoc loc);
AstNode* ast_create_binary(Arena* arena, SourceLoc loc, TokenKind op);
AstNode* ast_create_unary(Arena* arena, SourceLoc loc, TokenKind op);
AstNode* ast_create_call(Arena* arena, SourceLoc loc, const char* name);
AstNode* ast_create_ident(Arena* arena, SourceLoc loc, const char* name);
AstNode* ast_create_literal_int(Arena* arena, SourceLoc loc, int64_t value);
AstNode* ast_create_literal_float(Arena* arena, SourceLoc loc, double value);
AstNode* ast_create_literal_string(Arena* arena, SourceLoc loc, const char* value);
AstNode* ast_create_type(Arena* arena, SourceLoc loc, TypeKind type);

void ast_list_append(Arena* arena, AstList* list, AstNode* node);
const char* type_kind_to_str(TypeKind kind);

#ifdef __cplusplus
}
#endif

#endif