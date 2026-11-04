#include "parser.h"
#include <stdio.h>
#include <string.h>

static void parser_advance(Parser* parser) {
    parser->current = lexer_next(parser->lexer);
}

static bool parser_check(Parser* parser, TokenKind kind) {
    return parser->current.kind == kind;
}

static bool parser_match(Parser* parser, TokenKind kind) {
    if (parser_check(parser, kind)) {
        parser_advance(parser);
        return true;
    }
    return false;
}

static void parser_expect(Parser* parser, TokenKind kind, const char* msg) {
    if (!parser_match(parser, kind)) {
        parser->had_error = true;
        SourceLoc loc = parser->current.loc;
        diag_error(&loc, msg, "E0001");
    }
}

static void parser_synchronize(Parser* parser) {
    parser->panic_mode = false;
    while (!parser_check(parser, TOK_EOF)) {
        if (parser->current.kind == TOK_SEMI) {
            parser_advance(parser);
            return;
        }
        switch (parser->current.kind) {
            case TOK_FN:
            case TOK_VAR:
            case TOK_IF:
            case TOK_WHILE:
            case TOK_RETURN:
                return;
            default:
                break;
        }
        parser_advance(parser);
    }
}

static AstNode* parse_type(Parser* parser) {
    SourceLoc loc = parser->current.loc;
    TypeKind type = TYPE_VOID;

    if (parser_match(parser, TOK_INT)) type = TYPE_INT;
    else if (parser_match(parser, TOK_FLOAT_KW)) type = TYPE_FLOAT;
    else if (parser_match(parser, TOK_BOOL)) type = TYPE_BOOL;
    else if (parser_match(parser, TOK_VOID)) type = TYPE_VOID;
    else {
        parser->had_error = true;
        diag_error(&parser->current.loc, "Expected type", "E0002");
        return NULL;
    }

    return ast_create_type(parser->arena, loc, type);
}

static AstNode* parse_expression(Parser* parser);
static AstNode* parse_primary(Parser* parser);
static AstNode* parse_call(Parser* parser, const char* name, SourceLoc loc);
static AstNode* parse_block(Parser* parser);

static AstNode* parse_primary(Parser* parser) {
    Token tok = parser->current;

    if (parser_match(parser, TOK_INTEGER)) {
        return ast_create_literal_int(parser->arena, tok.loc, tok.value.int_val);
    }

    if (parser_match(parser, TOK_FLOAT)) {
        return ast_create_literal_float(parser->arena, tok.loc, tok.value.float_val);
    }

    if (parser_match(parser, TOK_STRING)) {
        return ast_create_literal_string(parser->arena, tok.loc, tok.text);
    }

    if (parser_match(parser, TOK_TRUE)) {
        AstNode* node = ast_create_literal_int(parser->arena, tok.loc, 1);
        if (node) node->as.literal.type = TYPE_BOOL;
        return node;
    }

    if (parser_match(parser, TOK_FALSE)) {
        AstNode* node = ast_create_literal_int(parser->arena, tok.loc, 0);
        if (node) node->as.literal.type = TYPE_BOOL;
        return node;
    }

    if (parser_match(parser, TOK_IDENT)) {
        const char* name = tok.text;
        if (parser_check(parser, TOK_LPAREN)) {
            return parse_call(parser, name, tok.loc);
        }
        return ast_create_ident(parser->arena, tok.loc, name);
    }

    if (parser_match(parser, TOK_LPAREN)) {
        AstNode* expr = parse_expression(parser);
        parser_expect(parser, TOK_RPAREN, "Expected ')' after expression");
        return expr;
    }

    parser->had_error = true;
    diag_error(&parser->current.loc, "Expected expression", "E0003");
    parser_advance(parser);
    return NULL;
}

static AstNode* parse_unary(Parser* parser) {
    if (parser_match(parser, TOK_MINUS) || parser_match(parser, TOK_NOT)) {
        TokenKind op = parser->current.kind == TOK_MINUS ? TOK_MINUS : TOK_NOT;
        SourceLoc loc = parser->current.loc;
        parser_advance(parser);
        AstNode* expr = parse_unary(parser);
        AstNode* node = ast_create_unary(parser->arena, loc, op);
        if (node) node->as.unary.expr = expr;
        return node;
    }
    return parse_primary(parser);
}

static int get_precedence(TokenKind kind) {
    switch (kind) {
        case TOK_EQ: return 1;
        case TOK_OR: return 2;
        case TOK_AND: return 3;
        case TOK_EQEQ:
        case TOK_NOTEQ: return 4;
        case TOK_LT:
        case TOK_GT:
        case TOK_LTE:
        case TOK_GTE: return 5;
        case TOK_PLUS:
        case TOK_MINUS: return 6;
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT: return 7;
        default: return 0;
    }
}

static AstNode* parse_binary(Parser* parser, int min_prec) {
    AstNode* lhs = parse_unary(parser);
    if (!lhs) return NULL;

    while (true) {
        TokenKind op = parser->current.kind;
        int prec = get_precedence(op);
        if (prec < min_prec) break;

        parser_advance(parser);
        int next_min_prec = (op == TOK_EQ) ? prec : prec + 1;
        AstNode* rhs = parse_binary(parser, next_min_prec);
        if (!rhs) return lhs;

        AstNode* node = ast_create_binary(parser->arena, lhs->loc, op);
        if (node) {
            node->as.binary.lhs = lhs;
            node->as.binary.rhs = rhs;
        }
        lhs = node;
    }
    return lhs;
}

static AstNode* parse_expression(Parser* parser) {
    return parse_binary(parser, 1);
}

static AstNode* parse_call(Parser* parser, const char* name, SourceLoc loc) {
    parser_expect(parser, TOK_LPAREN, "Expected '(' after function name");
    AstNode* node = ast_create_call(parser->arena, loc, name);

    if (!parser_check(parser, TOK_RPAREN)) {
        do {
            AstNode* arg = parse_expression(parser);
            if (arg) ast_list_append(parser->arena, &node->as.call.args, arg);
        } while (parser_match(parser, TOK_COMMA));
    }

    parser_expect(parser, TOK_RPAREN, "Expected ')' after arguments");
    return node;
}

static AstNode* parse_var_decl(Parser* parser) {
    SourceLoc loc = parser->current.loc;
    parser_expect(parser, TOK_VAR, "Expected 'var'");
    
    AstNode* type = parse_type(parser);
    if (!type) return NULL;
    
    Token name_tok = parser->current;
    parser_expect(parser, TOK_IDENT, "Expected variable name");
    const char* name = name_tok.text;

    AstNode* init = NULL;
    if (parser_match(parser, TOK_EQ)) {
        init = parse_expression(parser);
    }

    parser_expect(parser, TOK_SEMI, "Expected ';' after variable declaration");

    AstNode* node = ast_create_var_decl(parser->arena, loc, name);
    if (node) {
        node->as.var_decl.type = type;
        node->as.var_decl.init = init;
    }
    return node;
}

static AstNode* parse_return(Parser* parser) {
    SourceLoc loc = parser->current.loc;
    parser_expect(parser, TOK_RETURN, "Expected 'return'");
    AstNode* expr = NULL;
    if (!parser_check(parser, TOK_SEMI) && !parser_check(parser, TOK_RBRACE)) {
        expr = parse_expression(parser);
    }
    parser_expect(parser, TOK_SEMI, "Expected ';' after return");
    AstNode* node = ast_create_return(parser->arena, loc);
    if (node) node->as.return_stmt.expr = expr;
    return node;
}

static AstNode* parse_if(Parser* parser) {
    SourceLoc loc = parser->current.loc;
    parser_expect(parser, TOK_IF, "Expected 'if'");
    parser_expect(parser, TOK_LPAREN, "Expected '(' after 'if'");
    AstNode* cond = parse_expression(parser);
    parser_expect(parser, TOK_RPAREN, "Expected ')' after condition");
    AstNode* then_branch = parse_block(parser);
    AstNode* else_branch = NULL;
    if (parser_match(parser, TOK_ELSE)) {
        else_branch = parse_block(parser);
    }
    AstNode* node = ast_create_if(parser->arena, loc);
    if (node) {
        node->as.if_stmt.cond = cond;
        node->as.if_stmt.then_branch = then_branch;
        node->as.if_stmt.else_branch = else_branch;
    }
    return node;
}

static AstNode* parse_while(Parser* parser) {
    SourceLoc loc = parser->current.loc;
    parser_expect(parser, TOK_WHILE, "Expected 'while'");
    parser_expect(parser, TOK_LPAREN, "Expected '(' after 'while'");
    AstNode* cond = parse_expression(parser);
    parser_expect(parser, TOK_RPAREN, "Expected ')' after condition");
    AstNode* body = parse_block(parser);
    AstNode* node = ast_create_while(parser->arena, loc);
    if (node) {
        node->as.while_stmt.cond = cond;
        node->as.while_stmt.body = body;
    }
    return node;
}

static AstNode* parse_statement(Parser* parser) {
    switch (parser->current.kind) {
        case TOK_VAR: return parse_var_decl(parser);
        case TOK_RETURN: return parse_return(parser);
        case TOK_IF: return parse_if(parser);
        case TOK_WHILE: return parse_while(parser);
        case TOK_LBRACE: return parse_block(parser);
        case TOK_PRINT: {
            SourceLoc loc = parser->current.loc;
            parser_advance(parser);
            parser_expect(parser, TOK_LPAREN, "Expected '(' after 'print'");
            AstNode* expr = parse_expression(parser);
            parser_expect(parser, TOK_RPAREN, "Expected ')' after print argument");
            parser_expect(parser, TOK_SEMI, "Expected ';' after print");
            AstNode* call = ast_create_call(parser->arena, loc, "print");
            if (call) ast_list_append(parser->arena, &call->as.call.args, expr);
            AstNode* stmt = ast_create_expr_stmt(parser->arena, loc);
            if (stmt) stmt->as.expr_stmt.expr = call;
            return stmt;
        }
        default: {
            AstNode* expr = parse_expression(parser);
            parser_expect(parser, TOK_SEMI, "Expected ';' after expression");
            AstNode* node = ast_create_expr_stmt(parser->arena, expr ? expr->loc : parser->current.loc);
            if (node) node->as.expr_stmt.expr = expr;
            return node;
        }
    }
}

static AstNode* parse_block(Parser* parser) {
    SourceLoc loc = parser->current.loc;
    parser_expect(parser, TOK_LBRACE, "Expected '{'");
    AstNode* node = ast_create_block(parser->arena, loc);

    while (!parser_check(parser, TOK_RBRACE) && !parser_check(parser, TOK_EOF)) {
        AstNode* stmt = parse_statement(parser);
        if (stmt) ast_list_append(parser->arena, &node->as.block.stmts, stmt);
        if (parser->had_error && parser->panic_mode) {
            parser_synchronize(parser);
        }
    }

    parser_expect(parser, TOK_RBRACE, "Expected '}'");
    return node;
}

static AstNode* parse_fn_decl(Parser* parser) {
    SourceLoc loc = parser->current.loc;
    parser_expect(parser, TOK_FN, "Expected 'fn'");
    
    Token name_tok = parser->current;
    parser_expect(parser, TOK_IDENT, "Expected function name");
    const char* name = name_tok.text;

    parser_expect(parser, TOK_LPAREN, "Expected '(' after function name");

    AstNode* fn = ast_create_fn_decl(parser->arena, loc, name);

    if (!parser_check(parser, TOK_RPAREN)) {
        do {
            Token param_name_tok = parser->current;
            parser_expect(parser, TOK_IDENT, "Expected parameter name");
            const char* param_name = param_name_tok.text;
            parser_expect(parser, TOK_COLON, "Expected ':' after parameter name");
            AstNode* param_type = parse_type(parser);

            AstNode* param = ast_create_param(parser->arena, loc, param_name);
            if (param) param->as.param.type = param_type;
            ast_list_append(parser->arena, &fn->as.fn_decl.params, param);
        } while (parser_match(parser, TOK_COMMA));
    }

    parser_expect(parser, TOK_RPAREN, "Expected ')' after parameters");

    parser_expect(parser, TOK_ARROW, "Expected '->' after function parameters");
    fn->as.fn_decl.ret_type = parse_type(parser);

    fn->as.fn_decl.body = parse_block(parser);
    return fn;
}

static AstNode* parse_declaration(Parser* parser) {
    if (parser_check(parser, TOK_FN)) {
        return parse_fn_decl(parser);
    }
    parser->had_error = true;
    diag_error(&parser->current.loc, "Expected function declaration", "E0004");
    parser_advance(parser);
    return NULL;
}

Parser* parser_create(Lexer* lexer, Arena* arena) {
    Parser* parser = (Parser*)arena_alloc_zero(arena, sizeof(Parser));
    if (!parser) return NULL;
    parser->lexer = lexer;
    parser->arena = arena;
    parser->had_error = false;
    parser->panic_mode = false;
    parser->current = lexer_peek(lexer);
    return parser;
}

void parser_destroy(Parser* parser) {
    (void)parser;
}

AstNode* parser_parse(Parser* parser) {
    AstNode* program = ast_create_program(parser->arena, (SourceLoc){.file = parser->lexer->filename, .line = 1, .column = 1});

    while (!parser_check(parser, TOK_EOF)) {
        AstNode* decl = parse_declaration(parser);
        if (decl) ast_list_append(parser->arena, &program->as.program.decls, decl);
        if (parser->had_error && parser->panic_mode) {
            parser_synchronize(parser);
        }
    }

    return program;
}

bool parser_had_error(Parser* parser) {
    return parser->had_error;
}