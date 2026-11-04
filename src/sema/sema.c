#include "sema.h"
#include <stdio.h>
#include <string.h>

static Scope* scope_create(Sema* sema, Scope* parent) {
    Scope* scope = (Scope*)arena_alloc_zero(sema->arena, sizeof(Scope));
    if (!scope) return NULL;
    scope->parent = parent;
    return scope;
}

static Symbol* scope_lookup(Scope* scope, const char* name) {
    while (scope) {
        Symbol* sym = scope->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) return sym;
            sym = sym->next;
        }
        scope = scope->parent;
    }
    return NULL;
}

static Symbol* scope_lookup_current(Scope* scope, const char* name) {
    Symbol* sym = scope->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) return sym;
        sym = sym->next;
    }
    return NULL;
}

static Symbol* scope_declare(Sema* sema, Scope* scope, const char* name, SymbolKind kind, TypeKind type, bool is_mutable, AstNode* decl) {
    if (scope_lookup_current(scope, name)) {
        return NULL;
    }
    Symbol* sym = (Symbol*)arena_alloc_zero(sema->arena, sizeof(Symbol));
    if (!sym) return NULL;
    sym->kind = kind;
    sym->name = name;
    sym->type = type;
    sym->is_mutable = is_mutable;
    sym->is_initialized = false;
    sym->decl = decl;
    sym->next = scope->symbols;
    scope->symbols = sym;
    return sym;
}

static void sema_enter_scope(Sema* sema) {
    Scope* new_scope = scope_create(sema, sema->current_scope);
    sema->current_scope = new_scope;
}

static void sema_exit_scope(Sema* sema) {
    if (sema->current_scope->parent) {
        sema->current_scope = sema->current_scope->parent;
    }
}

static TypeKind ast_type_to_kind(AstNode* type_node) {
    if (!type_node || type_node->kind != AST_TYPE) return TYPE_VOID;
    return type_node->as.type_ast.type;
}

static void report_error(Sema* sema, SourceLoc loc, const char* msg, const char* code) {
    sema->had_error = true;
    diag_error(&loc, msg, code);
}

static TypeKind sema_expr(Sema* sema, AstNode* node);
static void sema_stmt(Sema* sema, AstNode* node);

static TypeKind sema_binary(Sema* sema, AstNode* node) {
    TypeKind lhs = sema_expr(sema, node->as.binary.lhs);
    TypeKind rhs = sema_expr(sema, node->as.binary.rhs);
    TokenKind op = node->as.binary.op;

    if (lhs == TYPE_VOID || rhs == TYPE_VOID) {
        report_error(sema, node->loc, "Cannot use void type in expression", "E0010");
        return TYPE_VOID;
    }

    if (op == TOK_EQ) {
        if (node->as.binary.lhs->kind != AST_IDENT) {
            report_error(sema, node->loc, "Left-hand side of assignment must be a variable", "E0011");
        } else {
            Symbol* sym = scope_lookup(sema->current_scope, node->as.binary.lhs->as.ident.name);
            if (!sym || (sym->kind != SYM_VARIABLE && sym->kind != SYM_PARAMETER)) {
                report_error(sema, node->loc, "Cannot assign to non-variable", "E0012");
            }
        }
        if (lhs != rhs) {
            report_error(sema, node->loc, "Type mismatch in assignment", "E0013");
        }
        return lhs;
    }

    if (op == TOK_EQEQ || op == TOK_NOTEQ || op == TOK_LT || op == TOK_GT || op == TOK_LTE || op == TOK_GTE) {
        if (lhs != rhs) {
            report_error(sema, node->loc, "Type mismatch in comparison", "E0014");
        }
        return TYPE_BOOL;
    }

    if (op == TOK_AND || op == TOK_OR) {
        if (lhs != TYPE_BOOL || rhs != TYPE_BOOL) {
            report_error(sema, node->loc, "Logical operators require bool operands", "E0015");
        }
        return TYPE_BOOL;
    }

    if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR || op == TOK_SLASH || op == TOK_PERCENT) {
        if (lhs != rhs || (lhs != TYPE_INT && lhs != TYPE_FLOAT)) {
            report_error(sema, node->loc, "Arithmetic operators require matching int or float operands", "E0016");
        }
        return lhs;
    }

    return TYPE_VOID;
}

static TypeKind sema_unary(Sema* sema, AstNode* node) {
    TypeKind expr_type = sema_expr(sema, node->as.unary.expr);
    TokenKind op = node->as.unary.op;

    if (op == TOK_NOT) {
        if (expr_type != TYPE_BOOL) {
            report_error(sema, node->loc, "Logical not requires bool operand", "E0014");
        }
        return TYPE_BOOL;
    }

    if (op == TOK_MINUS) {
        if (expr_type != TYPE_INT && expr_type != TYPE_FLOAT) {
            report_error(sema, node->loc, "Unary minus requires numeric operand", "E0015");
        }
        return expr_type;
    }

    return expr_type;
}

static TypeKind sema_call(Sema* sema, AstNode* node) {
    Symbol* func = scope_lookup(sema->global_scope, node->as.call.name);
    if (!func || func->kind != SYM_FUNCTION) {
        report_error(sema, node->loc, "Undefined function", "E0016");
        return TYPE_VOID;
    }

    if (strcmp(node->as.call.name, "print") == 0) {
        if (node->as.call.args.count != 1) {
            report_error(sema, node->loc, "print() requires exactly one argument", "E0017");
        } else {
            TypeKind arg_type = sema_expr(sema, node->as.call.args.items[0]);
            if (arg_type != TYPE_INT && arg_type != TYPE_FLOAT && arg_type != TYPE_BOOL) {
                report_error(sema, node->loc, "print() argument must be int, float, or bool", "E0018");
            }
        }
        return TYPE_VOID;
    }

    if (node->as.call.args.count != 0) {
        report_error(sema, node->loc, "Function call with arguments not yet supported", "E0017");
    }

    return func->type;
}

static TypeKind sema_ident(Sema* sema, AstNode* node) {
    Symbol* sym = scope_lookup(sema->current_scope, node->as.ident.name);
    if (!sym) {
        report_error(sema, node->loc, "Undefined variable", "E0018");
        return TYPE_VOID;
    }
    if (sym->kind == SYM_VARIABLE || sym->kind == SYM_PARAMETER) {
        sym->is_initialized = true;
        return sym->type;
    }
    report_error(sema, node->loc, "Not a variable", "E0019");
    return TYPE_VOID;
}

static TypeKind sema_literal(Sema* sema, AstNode* node) {
    return node->as.literal.type;
}

static TypeKind sema_expr(Sema* sema, AstNode* node) {
    if (!node) return TYPE_VOID;

    switch (node->kind) {
        case AST_BINARY: return sema_binary(sema, node);
        case AST_UNARY: return sema_unary(sema, node);
        case AST_CALL: return sema_call(sema, node);
        case AST_IDENT: return sema_ident(sema, node);
        case AST_LITERAL: return sema_literal(sema, node);
        default:
            report_error(sema, node->loc, "Invalid expression", "E0020");
            return TYPE_VOID;
    }
}

static void sema_var_decl(Sema* sema, AstNode* node) {
    TypeKind type = ast_type_to_kind(node->as.var_decl.type);
    if (type == TYPE_VOID) {
        report_error(sema, node->loc, "Variable cannot have void type", "E0021");
        return;
    }

    Symbol* sym = scope_declare(sema, sema->current_scope, node->as.var_decl.name, SYM_VARIABLE, type, false, node);
    if (!sym) {
        report_error(sema, node->loc, "Variable already declared in this scope", "E0022");
        return;
    }

    if (node->as.var_decl.init) {
        TypeKind init_type = sema_expr(sema, node->as.var_decl.init);
        if (init_type != type) {
            report_error(sema, node->loc, "Type mismatch in variable initialization", "E0023");
        }
        sym->is_initialized = true;
    }
}

static void sema_return(Sema* sema, AstNode* node) {
    if (!sema->current_function) {
        report_error(sema, node->loc, "Return outside function", "E0024");
        return;
    }

    TypeKind ret_type = TYPE_VOID;
    if (node->as.return_stmt.expr) {
        ret_type = sema_expr(sema, node->as.return_stmt.expr);
    }

    TypeKind fn_ret_type = ast_type_to_kind(sema->current_function->as.fn_decl.ret_type);
    if (ret_type != fn_ret_type) {
        report_error(sema, node->loc, "Return type mismatch", "E0025");
    }
}

static void sema_if(Sema* sema, AstNode* node) {
    TypeKind cond_type = sema_expr(sema, node->as.if_stmt.cond);
    if (cond_type != TYPE_BOOL) {
        report_error(sema, node->loc, "If condition must be bool", "E0026");
    }
    sema_enter_scope(sema);
    sema_stmt(sema, node->as.if_stmt.then_branch);
    sema_exit_scope(sema);
    if (node->as.if_stmt.else_branch) {
        sema_enter_scope(sema);
        sema_stmt(sema, node->as.if_stmt.else_branch);
        sema_exit_scope(sema);
    }
}

static void sema_while(Sema* sema, AstNode* node) {
    TypeKind cond_type = sema_expr(sema, node->as.while_stmt.cond);
    if (cond_type != TYPE_BOOL) {
        report_error(sema, node->loc, "While condition must be bool", "E0027");
    }
    bool prev_in_loop = sema->in_loop;
    sema->in_loop = true;
    sema_enter_scope(sema);
    sema_stmt(sema, node->as.while_stmt.body);
    sema_exit_scope(sema);
    sema->in_loop = prev_in_loop;
}

static void sema_block(Sema* sema, AstNode* node) {
    for (size_t i = 0; i < node->as.block.stmts.count; i++) {
        sema_stmt(sema, node->as.block.stmts.items[i]);
    }
}

static void sema_expr_stmt(Sema* sema, AstNode* node) {
    if (node->as.expr_stmt.expr) {
        sema_expr(sema, node->as.expr_stmt.expr);
    }
}

static void sema_fn_decl(Sema* sema, AstNode* node) {
    TypeKind ret_type = ast_type_to_kind(node->as.fn_decl.ret_type);
    Symbol* sym = scope_declare(sema, sema->global_scope, node->as.fn_decl.name, SYM_FUNCTION, ret_type, false, node);
    if (!sym) {
        report_error(sema, node->loc, "Function already declared", "E0028");
        return;
    }

    AstNode* prev_fn = sema->current_function;
    sema->current_function = node;
    sema_enter_scope(sema);

    for (size_t i = 0; i < node->as.fn_decl.params.count; i++) {
        AstNode* param = node->as.fn_decl.params.items[i];
        TypeKind param_type = ast_type_to_kind(param->as.param.type);
        Symbol* p = scope_declare(sema, sema->current_scope, param->as.param.name, SYM_PARAMETER, param_type, false, param);
        if (!p) {
            report_error(sema, param->loc, "Duplicate parameter name", "E0029");
        }
    }

    sema_stmt(sema, node->as.fn_decl.body);
    sema_exit_scope(sema);
    sema->current_function = prev_fn;
}

static void sema_stmt(Sema* sema, AstNode* node) {
    if (!node) return;

    switch (node->kind) {
        case AST_VAR_DECL: sema_var_decl(sema, node); break;
        case AST_RETURN: sema_return(sema, node); break;
        case AST_IF: sema_if(sema, node); break;
        case AST_WHILE: sema_while(sema, node); break;
        case AST_BLOCK: sema_block(sema, node); break;
        case AST_EXPR_STMT: sema_expr_stmt(sema, node); break;
        default:
            report_error(sema, node->loc, "Invalid statement", "E0030");
            break;
    }
}

Sema* sema_create(Arena* arena) {
    Sema* sema = (Sema*)arena_alloc_zero(arena, sizeof(Sema));
    if (!sema) return NULL;
    sema->arena = arena;
    sema->global_scope = scope_create(sema, NULL);
    sema->current_scope = sema->global_scope;
    sema->had_error = false;
    sema->in_loop = false;

    scope_declare(sema, sema->global_scope, "print", SYM_FUNCTION, TYPE_VOID, false, NULL);

    return sema;
}

void sema_destroy(Sema* sema) {
    (void)sema;
}

bool sema_analyze(Sema* sema, AstNode* program) {
    if (!program || program->kind != AST_PROGRAM) return false;

    for (size_t i = 0; i < program->as.program.decls.count; i++) {
        AstNode* decl = program->as.program.decls.items[i];
        if (decl->kind == AST_FN_DECL) {
            sema_fn_decl(sema, decl);
        }
    }

    Symbol* main_fn = scope_lookup(sema->global_scope, "main");
    if (!main_fn || main_fn->kind != SYM_FUNCTION) {
        report_error(sema, (SourceLoc){0}, "Program must have a main function", "E0031");
    } else if (main_fn->type != TYPE_INT) {
        report_error(sema, (SourceLoc){0}, "Main function must return int", "E0032");
    }

    return !sema->had_error;
}

bool sema_had_error(Sema* sema) {
    return sema->had_error;
}