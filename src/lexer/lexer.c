#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static bool is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static void lexer_advance(Lexer* lexer) {
    if (lexer->pos >= lexer->source_len) return;
    if (lexer->source[lexer->pos] == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    lexer->pos++;
}

static char lexer_peek_char(Lexer* lexer) {
    if (lexer->pos + 1 >= lexer->source_len) return '\0';
    return lexer->source[lexer->pos + 1];
}

static char lexer_current_char(Lexer* lexer) {
    if (lexer->pos >= lexer->source_len) return '\0';
    return lexer->source[lexer->pos];
}

static void lexer_skip_whitespace(Lexer* lexer) {
    while (lexer->pos < lexer->source_len) {
        char c = lexer_current_char(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            lexer_advance(lexer);
        } else if (c == '/' && lexer_peek_char(lexer) == '/') {
            while (lexer->pos < lexer->source_len && lexer_current_char(lexer) != '\n') {
                lexer_advance(lexer);
            }
        } else {
            break;
        }
    }
}

static Token make_token(Lexer* lexer, TokenKind kind, const char* start, size_t len) {
    Token tok = {0};
    tok.kind = kind;
    tok.loc.file = lexer->filename;
    tok.loc.line = lexer->line;
    tok.loc.column = lexer->column - len;
    tok.text = start;
    tok.length = len;
    if (kind == TOK_IDENT || kind == TOK_STRING) {
        tok.text = interner_intern(lexer->interner, start, len);
    }
    return tok;
}

static Token make_error(Lexer* lexer, const char* msg) {
    Token tok = {0};
    tok.kind = TOK_ERROR;
    tok.loc.file = lexer->filename;
    tok.loc.line = lexer->line;
    tok.loc.column = lexer->column;
    tok.text = msg;
    tok.length = strlen(msg);
    return tok;
}

static Token scan_ident(Lexer* lexer) {
    const char* start = lexer->source + lexer->pos;
    size_t len = 0;
    while (lexer->pos < lexer->source_len && is_ident_char(lexer_current_char(lexer))) {
        lexer_advance(lexer);
        len++;
    }

    const char* text = start;
    if (len == 3 && strncmp(text, "var", 3) == 0) return make_token(lexer, TOK_VAR, text, len);
    if (len == 2 && strncmp(text, "fn", 2) == 0) return make_token(lexer, TOK_FN, text, len);
    if (len == 6 && strncmp(text, "return", 6) == 0) return make_token(lexer, TOK_RETURN, text, len);
    if (len == 2 && strncmp(text, "if", 2) == 0) return make_token(lexer, TOK_IF, text, len);
    if (len == 4 && strncmp(text, "else", 4) == 0) return make_token(lexer, TOK_ELSE, text, len);
    if (len == 5 && strncmp(text, "while", 5) == 0) return make_token(lexer, TOK_WHILE, text, len);
    if (len == 5 && strncmp(text, "print", 5) == 0) return make_token(lexer, TOK_PRINT, text, len);
    if (len == 3 && strncmp(text, "int", 3) == 0) return make_token(lexer, TOK_INT, text, len);
    if (len == 5 && strncmp(text, "float", 5) == 0) return make_token(lexer, TOK_FLOAT_KW, text, len);
    if (len == 4 && strncmp(text, "bool", 4) == 0) return make_token(lexer, TOK_BOOL, text, len);
    if (len == 4 && strncmp(text, "void", 4) == 0) return make_token(lexer, TOK_VOID, text, len);
    if (len == 4 && strncmp(text, "true", 4) == 0) return make_token(lexer, TOK_TRUE, text, len);
    if (len == 5 && strncmp(text, "false", 5) == 0) return make_token(lexer, TOK_FALSE, text, len);

    return make_token(lexer, TOK_IDENT, text, len);
}

static Token scan_number(Lexer* lexer) {
    const char* start = lexer->source + lexer->pos;
    bool is_float = false;
    size_t len = 0;

    while (lexer->pos < lexer->source_len && is_digit(lexer_current_char(lexer))) {
        lexer_advance(lexer);
        len++;
    }

    if (lexer->pos < lexer->source_len && lexer_current_char(lexer) == '.') {
        char next = lexer_peek_char(lexer);
        if (is_digit(next)) {
            is_float = true;
            lexer_advance(lexer);
            len++;
            while (lexer->pos < lexer->source_len && is_digit(lexer_current_char(lexer))) {
                lexer_advance(lexer);
                len++;
            }
        }
    }

    if (is_float) {
        double val = strtod(start, NULL);
        Token tok = make_token(lexer, TOK_FLOAT, start, len);
        tok.value.float_val = val;
        return tok;
    } else {
        int64_t val = strtoll(start, NULL, 10);
        Token tok = make_token(lexer, TOK_INTEGER, start, len);
        tok.value.int_val = val;
        return tok;
    }
}

static Token scan_string(Lexer* lexer) {
    lexer_advance(lexer);
    const char* start = lexer->source + lexer->pos;
    size_t len = 0;

    while (lexer->pos < lexer->source_len) {
        char c = lexer_current_char(lexer);
        if (c == '"') break;
        if (c == '\\') {
            lexer_advance(lexer);
            if (lexer->pos < lexer->source_len) lexer_advance(lexer);
            len += 2;
        } else {
            lexer_advance(lexer);
            len++;
        }
    }

    if (lexer->pos >= lexer->source_len) {
        return make_error(lexer, "Unterminated string literal");
    }

    Token tok = make_token(lexer, TOK_STRING, start, len);
    lexer_advance(lexer);
    return tok;
}

static Token scan_operator(Lexer* lexer) {
    char c = lexer_current_char(lexer);
    char next = lexer_peek_char(lexer);
    const char* start = lexer->source + lexer->pos;

    switch (c) {
        case '+': lexer_advance(lexer); return make_token(lexer, TOK_PLUS, start, 1);
        case '-':
            if (next == '>') { lexer_advance(lexer); lexer_advance(lexer); return make_token(lexer, TOK_ARROW, start, 2); }
            lexer_advance(lexer); return make_token(lexer, TOK_MINUS, start, 1);
        case '*': lexer_advance(lexer); return make_token(lexer, TOK_STAR, start, 1);
        case '/': lexer_advance(lexer); return make_token(lexer, TOK_SLASH, start, 1);
        case '%': lexer_advance(lexer); return make_token(lexer, TOK_PERCENT, start, 1);
        case '=':
            if (next == '=') { lexer_advance(lexer); lexer_advance(lexer); return make_token(lexer, TOK_EQEQ, start, 2); }
            lexer_advance(lexer); return make_token(lexer, TOK_EQ, start, 1);
        case '!':
            if (next == '=') { lexer_advance(lexer); lexer_advance(lexer); return make_token(lexer, TOK_NOTEQ, start, 2); }
            lexer_advance(lexer); return make_token(lexer, TOK_NOT, start, 1);
        case '<':
            if (next == '=') { lexer_advance(lexer); lexer_advance(lexer); return make_token(lexer, TOK_LTE, start, 2); }
            lexer_advance(lexer); return make_token(lexer, TOK_LT, start, 1);
        case '>':
            if (next == '=') { lexer_advance(lexer); lexer_advance(lexer); return make_token(lexer, TOK_GTE, start, 2); }
            lexer_advance(lexer); return make_token(lexer, TOK_GT, start, 1);
        case '&':
            if (next == '&') { lexer_advance(lexer); lexer_advance(lexer); return make_token(lexer, TOK_AND, start, 2); }
            break;
        case '|':
            if (next == '|') { lexer_advance(lexer); lexer_advance(lexer); return make_token(lexer, TOK_OR, start, 2); }
            break;
    }

    return make_error(lexer, "Invalid operator");
}

static Token scan_delimiter(Lexer* lexer) {
    char c = lexer_current_char(lexer);
    const char* start = lexer->source + lexer->pos;

    switch (c) {
        case '(': lexer_advance(lexer); return make_token(lexer, TOK_LPAREN, start, 1);
        case ')': lexer_advance(lexer); return make_token(lexer, TOK_RPAREN, start, 1);
        case '{': lexer_advance(lexer); return make_token(lexer, TOK_LBRACE, start, 1);
        case '}': lexer_advance(lexer); return make_token(lexer, TOK_RBRACE, start, 1);
        case '[': lexer_advance(lexer); return make_token(lexer, TOK_LBRACKET, start, 1);
        case ']': lexer_advance(lexer); return make_token(lexer, TOK_RBRACKET, start, 1);
        case ',': lexer_advance(lexer); return make_token(lexer, TOK_COMMA, start, 1);
        case ';': lexer_advance(lexer); return make_token(lexer, TOK_SEMI, start, 1);
        case ':': lexer_advance(lexer); return make_token(lexer, TOK_COLON, start, 1);
        case '.': lexer_advance(lexer); return make_token(lexer, TOK_DOT, start, 1);
    }

    return make_error(lexer, "Invalid delimiter");
}

Lexer* lexer_create(const char* source, size_t len, const char* filename, StringInterner* interner) {
    Lexer* lexer = (Lexer*)arena_alloc_zero(interner_get_arena(interner), sizeof(Lexer));
    if (!lexer) return NULL;
    lexer->source = source;
    lexer->source_len = len;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->filename = filename;
    lexer->interner = interner;
    lexer->current = lexer_next(lexer);
    return lexer;
}

void lexer_destroy(Lexer* lexer) {
    (void)lexer;
}

Token lexer_next(Lexer* lexer) {
    lexer_skip_whitespace(lexer);

    if (lexer->pos >= lexer->source_len) {
        return make_token(lexer, TOK_EOF, "", 0);
    }

    char c = lexer_current_char(lexer);

    if (is_ident_start(c)) {
        return scan_ident(lexer);
    }

    if (is_digit(c)) {
        return scan_number(lexer);
    }

    if (c == '"') {
        return scan_string(lexer);
    }

    if (strchr("+-*/%=<!>&|", c)) {
        return scan_operator(lexer);
    }

    if (strchr("(){}[];,.:", c)) {
        return scan_delimiter(lexer);
    }

    return make_error(lexer, "Unexpected character");
}

Token lexer_peek(Lexer* lexer) {
    return lexer->current;
}

const char* token_kind_to_str(TokenKind kind) {
    switch (kind) {
        case TOK_EOF: return "EOF";
        case TOK_ERROR: return "ERROR";
        case TOK_IDENT: return "IDENT";
        case TOK_INTEGER: return "INTEGER";
        case TOK_FLOAT: return "FLOAT";
        case TOK_STRING: return "STRING";
        case TOK_VAR: return "VAR";
        case TOK_FN: return "FN";
        case TOK_RETURN: return "RETURN";
        case TOK_IF: return "IF";
        case TOK_ELSE: return "ELSE";
        case TOK_WHILE: return "WHILE";
        case TOK_PRINT: return "PRINT";
        case TOK_INT: return "INT";
        case TOK_FLOAT_KW: return "FLOAT_KW";
case TOK_BOOL: return "BOOL";
    case TOK_VOID: return "VOID";
    case TOK_TRUE: return "TRUE";
    case TOK_FALSE: return "FALSE";
    case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH";
        case TOK_PERCENT: return "PERCENT";
        case TOK_EQ: return "EQ";
        case TOK_EQEQ: return "EQEQ";
        case TOK_NOTEQ: return "NOTEQ";
        case TOK_LT: return "LT";
        case TOK_GT: return "GT";
        case TOK_LTE: return "LTE";
        case TOK_GTE: return "GTE";
        case TOK_AND: return "AND";
        case TOK_OR: return "OR";
        case TOK_NOT: return "NOT";
        case TOK_ARROW: return "ARROW";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
        case TOK_COMMA: return "COMMA";
        case TOK_SEMI: return "SEMI";
        case TOK_COLON: return "COLON";
        case TOK_DOT: return "DOT";
    }
    return "UNKNOWN";
}