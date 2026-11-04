#ifndef VASSIAN_PARSER_H
#define VASSIAN_PARSER_H

#include "lexer.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Lexer* lexer;
    Arena* arena;
    Token current;
    bool had_error;
    bool panic_mode;
} Parser;

Parser* parser_create(Lexer* lexer, Arena* arena);
void parser_destroy(Parser* parser);
AstNode* parser_parse(Parser* parser);
bool parser_had_error(Parser* parser);

#ifdef __cplusplus
}
#endif

#endif