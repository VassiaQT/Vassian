#ifndef VASSIAN_LEXER_H
#define VASSIAN_LEXER_H

#include "vassian_utils.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOK_EOF,
    TOK_ERROR,

    TOK_IDENT,
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_STRING,

    TOK_VAR,
    TOK_FN,
    TOK_RETURN,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_PRINT,
    TOK_INT,
    TOK_FLOAT_KW,
    TOK_BOOL,
    TOK_VOID,
    TOK_TRUE,
    TOK_FALSE,

    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_EQ,
    TOK_EQEQ,
    TOK_NOTEQ,
    TOK_LT,
    TOK_GT,
    TOK_LTE,
    TOK_GTE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_ARROW,

    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_SEMI,
    TOK_COLON,
    TOK_DOT,
} TokenKind;

typedef struct {
    TokenKind kind;
    SourceLoc loc;
    const char* text;
    size_t length;
    union {
        int64_t int_val;
        double float_val;
    } value;
} Token;

typedef struct {
    const char* source;
    size_t source_len;
    size_t pos;
    size_t line;
    size_t column;
    const char* filename;
    Token current;
    StringInterner* interner;
} Lexer;

Lexer* lexer_create(const char* source, size_t len, const char* filename, StringInterner* interner);
void lexer_destroy(Lexer* lexer);
Token lexer_next(Lexer* lexer);
Token lexer_peek(Lexer* lexer);
const char* token_kind_to_str(TokenKind kind);

#ifdef __cplusplus
}
#endif

#endif