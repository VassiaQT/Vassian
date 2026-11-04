#include "codegen.h"
#include "ast.h"
#include "lexer/lexer.h"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Codegen {
    LLVMModuleRef module;
    LLVMContextRef context;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    const char* triple;
    const char* cpu;
    const char* features;
};

static LLVMTypeRef type_to_llvm(Codegen* cg, TypeKind kind) {
    switch (kind) {
        case TYPE_VOID: return LLVMVoidTypeInContext(cg->context);
        case TYPE_INT: return LLVMInt32TypeInContext(cg->context);
        case TYPE_FLOAT: return LLVMDoubleTypeInContext(cg->context);
        case TYPE_BOOL: return LLVMInt1TypeInContext(cg->context);
    }
    return LLVMVoidTypeInContext(cg->context);
}

static LLVMValueRef codegen_expr(Codegen* cg, AstNode* node);
static void codegen_stmt(Codegen* cg, AstNode* node);

Codegen* codegen_create(const char* module_name, const char* triple, const char* cpu, const char* features) {
    Codegen* cg = (Codegen*)malloc(sizeof(Codegen));
    if (!cg) return NULL;

    cg->context = LLVMContextCreate();
    cg->module = LLVMModuleCreateWithNameInContext(module_name, cg->context);
    cg->builder = LLVMCreateBuilderInContext(cg->context);
    cg->triple = triple;
    cg->cpu = cpu;
    cg->features = features;
    cg->locals.names = NULL;
    cg->locals.values = NULL;
    cg->locals.types = NULL;
    cg->locals.count = 0;
    cg->locals.capacity = 0;

    LLVMInitializeX86Target();
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmParser();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    LLVMTargetRef target;
    char* error = NULL;
    if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
        fprintf(stderr, "Failed to get target: %s\n", error);
        LLVMDisposeMessage(error);
        free(cg);
        return NULL;
    }

    cg->target_machine = LLVMCreateTargetMachine(target, triple, cpu, features,
        LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    if (!cg->target_machine) {
        fprintf(stderr, "Failed to create target machine\n");
        free(cg);
        return NULL;
    }

    LLVMSetTarget(cg->module, triple);
    // Data layout is derived from target machine automatically

    return cg;
}

void codegen_destroy(Codegen* cg) {
    if (!cg) return;
    LLVMDisposeBuilder(cg->builder);
    LLVMDisposeModule(cg->module);
    LLVMContextDispose(cg->context);
    LLVMDisposeTargetMachine(cg->target_machine);
    for (size_t i = 0; i < cg->locals.count; i++) {
        free(cg->locals.names[i]);
    }
    free(cg->locals.names);
    free(cg->locals.values);
    free(cg->locals.types);
    free(cg);
}

static LLVMValueRef codegen_literal(Codegen* cg, AstNode* node) {
    switch (node->as.literal.type) {
        case TYPE_INT:
            return LLVMConstInt(LLVMInt32TypeInContext(cg->context), (unsigned long long)node->as.literal.value.int_val, true);
        case TYPE_FLOAT:
            return LLVMConstReal(LLVMDoubleTypeInContext(cg->context), node->as.literal.value.float_val);
        case TYPE_BOOL:
            return LLVMConstInt(LLVMInt1TypeInContext(cg->context), node->as.literal.value.int_val, false);
        default:
            return LLVMConstNull(LLVMVoidTypeInContext(cg->context));
    }
}

static LLVMValueRef codegen_ident(Codegen* cg, AstNode* node) {
    // Check local variables first
    for (size_t i = 0; i < cg->locals.count; i++) {
        if (strcmp(cg->locals.names[i], node->as.ident.name) == 0) {
            LLVMTypeRef elem_type = cg->locals.types[i];
            if (!elem_type) elem_type = LLVMInt32TypeInContext(cg->context);
            return LLVMBuildLoad2(cg->builder, elem_type, cg->locals.values[i], node->as.ident.name);
        }
    }
    // Check globals
    LLVMValueRef val = LLVMGetNamedGlobal(cg->module, node->as.ident.name);
    if (!val) {
        return LLVMConstNull(LLVMInt32TypeInContext(cg->context));
    }
    LLVMTypeRef global_type = LLVMTypeOf(val);
    LLVMTypeRef global_elem_type = LLVMGetElementType(global_type);
    if (!global_elem_type) global_elem_type = LLVMInt32TypeInContext(cg->context);
    return LLVMBuildLoad2(cg->builder, global_elem_type, val, node->as.ident.name);
}

static void locals_add(Codegen* cg, const char* name, LLVMValueRef value, LLVMTypeRef type) {
    if (cg->locals.count >= cg->locals.capacity) {
        size_t new_cap = cg->locals.capacity == 0 ? 8 : cg->locals.capacity * 2;
        cg->locals.names = realloc(cg->locals.names, new_cap * sizeof(char*));
        cg->locals.values = realloc(cg->locals.values, new_cap * sizeof(LLVMValueRef));
        cg->locals.types = realloc(cg->locals.types, new_cap * sizeof(LLVMTypeRef));
        cg->locals.capacity = new_cap;
    }
    cg->locals.names[cg->locals.count] = strdup(name);
    cg->locals.values[cg->locals.count] = value;
    cg->locals.types[cg->locals.count] = type;
    cg->locals.count++;
}

static LLVMValueRef codegen_binary(Codegen* cg, AstNode* node) {
    TokenKind op = node->as.binary.op;

    // Assignment needs special handling - LHS must be an alloca
    if (op == TOK_EQ) {
        AstNode* lhs_node = node->as.binary.lhs;
        if (lhs_node->kind != AST_IDENT) {
            return NULL;
        }
        // Find the alloca for the variable
        LLVMValueRef alloca = NULL;
        for (size_t i = 0; i < cg->locals.count; i++) {
            if (strcmp(cg->locals.names[i], lhs_node->as.ident.name) == 0) {
                alloca = cg->locals.values[i];
                break;
            }
        }
        if (!alloca) {
            // Check globals
            alloca = LLVMGetNamedGlobal(cg->module, lhs_node->as.ident.name);
        }
        if (!alloca) {
            return NULL;
        }

        LLVMValueRef rhs = codegen_expr(cg, node->as.binary.rhs);
        if (!rhs) return NULL;

        LLVMBuildStore(cg->builder, rhs, alloca);
        return rhs;
    }

    LLVMValueRef lhs = codegen_expr(cg, node->as.binary.lhs);
    LLVMValueRef rhs = codegen_expr(cg, node->as.binary.rhs);

    if (!lhs || !rhs) return NULL;

    switch (op) {
        case TOK_PLUS: return LLVMBuildAdd(cg->builder, lhs, rhs, "addtmp");
        case TOK_MINUS: return LLVMBuildSub(cg->builder, lhs, rhs, "subtmp");
        case TOK_STAR: return LLVMBuildMul(cg->builder, lhs, rhs, "multmp");
        case TOK_SLASH: return LLVMBuildSDiv(cg->builder, lhs, rhs, "divtmp");
        case TOK_PERCENT: return LLVMBuildSRem(cg->builder, lhs, rhs, "remtmp");
        case TOK_EQEQ: return LLVMBuildICmp(cg->builder, LLVMIntEQ, lhs, rhs, "eqtmp");
        case TOK_NOTEQ: return LLVMBuildICmp(cg->builder, LLVMIntNE, lhs, rhs, "netmp");
        case TOK_LT: return LLVMBuildICmp(cg->builder, LLVMIntSLT, lhs, rhs, "lttmp");
        case TOK_GT: return LLVMBuildICmp(cg->builder, LLVMIntSGT, lhs, rhs, "gttmp");
        case TOK_LTE: return LLVMBuildICmp(cg->builder, LLVMIntSLE, lhs, rhs, "letmp");
        case TOK_GTE: return LLVMBuildICmp(cg->builder, LLVMIntSGE, lhs, rhs, "getmp");
        case TOK_AND: return LLVMBuildAnd(cg->builder, lhs, rhs, "andtmp");
        case TOK_OR: return LLVMBuildOr(cg->builder, lhs, rhs, "ortmp");
        default: return NULL;
    }
}

static LLVMValueRef codegen_call(Codegen* cg, AstNode* node) {
    LLVMValueRef func = LLVMGetNamedFunction(cg->module, node->as.call.name);
    if (!func) {
        return NULL;
    }

    if (strcmp(node->as.call.name, "print") == 0) {
        if (node->as.call.args.count == 1) {
            LLVMValueRef arg = codegen_expr(cg, node->as.call.args.items[0]);
            if (arg) {
                // Zero-extend i1 (bool) to i32 for print
                LLVMTypeRef arg_type = LLVMTypeOf(arg);
                if (LLVMGetTypeKind(arg_type) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(arg_type) == 1) {
                    arg = LLVMBuildZExt(cg->builder, arg, LLVMInt32TypeInContext(cg->context), "print_arg_zext");
                }
                LLVMBuildCall2(cg->builder, LLVMGlobalGetValueType(func), func, &arg, 1, "");
            }
        }
        return NULL;
    }

    if (node->as.call.args.count > 0) {
        return NULL;
    }

    LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
    LLVMTypeRef ret_type = LLVMGetReturnType(func_type);
    const char* call_name = (ret_type == LLVMVoidTypeInContext(cg->context)) ? "" : "calltmp";
    return LLVMBuildCall2(cg->builder, func_type, func, NULL, 0, call_name);
}

static LLVMValueRef codegen_expr(Codegen* cg, AstNode* node) {
    if (!node) return NULL;

    switch (node->kind) {
        case AST_LITERAL: return codegen_literal(cg, node);
        case AST_IDENT: return codegen_ident(cg, node);
        case AST_BINARY: return codegen_binary(cg, node);
        case AST_CALL: return codegen_call(cg, node);
        default: return NULL;
    }
}

static void codegen_var_decl(Codegen* cg, AstNode* node) {
    LLVMTypeRef type = type_to_llvm(cg, node->as.var_decl.type->as.type_ast.type);
    LLVMValueRef alloca = LLVMBuildAlloca(cg->builder, type, node->as.var_decl.name);

    // Add to local symbol table
    locals_add(cg, node->as.var_decl.name, alloca, type);

    if (node->as.var_decl.init) {
        LLVMValueRef init = codegen_expr(cg, node->as.var_decl.init);
        if (init) {
            LLVMBuildStore(cg->builder, init, alloca);
        }
    }
}

static void codegen_return(Codegen* cg, AstNode* node) {
    if (node->as.return_stmt.expr) {
        LLVMValueRef val = codegen_expr(cg, node->as.return_stmt.expr);
        if (val) {
            LLVMBuildRet(cg->builder, val);
        }
    } else {
        LLVMBuildRetVoid(cg->builder);
    }
}

static void codegen_if(Codegen* cg, AstNode* node) {
    LLVMValueRef cond = codegen_expr(cg, node->as.if_stmt.cond);
    if (!cond) return;

    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));
    LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(cg->context, func, "then");
    LLVMBasicBlockRef else_bb = node->as.if_stmt.else_branch ? LLVMAppendBasicBlockInContext(cg->context, func, "else") : NULL;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(cg->context, func, "ifmerge");

    LLVMBuildCondBr(cg->builder, cond, then_bb, else_bb ? else_bb : merge_bb);

    LLVMPositionBuilderAtEnd(cg->builder, then_bb);
    codegen_stmt(cg, node->as.if_stmt.then_branch);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
        LLVMBuildBr(cg->builder, merge_bb);
    }

    if (else_bb) {
        LLVMPositionBuilderAtEnd(cg->builder, else_bb);
        codegen_stmt(cg, node->as.if_stmt.else_branch);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
            LLVMBuildBr(cg->builder, merge_bb);
        }
    }

    LLVMPositionBuilderAtEnd(cg->builder, merge_bb);
}

static void codegen_while(Codegen* cg, AstNode* node) {
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(cg->builder));
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(cg->context, func, "whilecond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(cg->context, func, "whilebody");
    LLVMBasicBlockRef after_bb = LLVMAppendBasicBlockInContext(cg->context, func, "whileafter");

    LLVMBuildBr(cg->builder, cond_bb);

    LLVMPositionBuilderAtEnd(cg->builder, cond_bb);
    LLVMValueRef cond = codegen_expr(cg, node->as.while_stmt.cond);
    if (cond) {
        LLVMBuildCondBr(cg->builder, cond, body_bb, after_bb);
    }

    LLVMPositionBuilderAtEnd(cg->builder, body_bb);
    codegen_stmt(cg, node->as.while_stmt.body);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
        LLVMBuildBr(cg->builder, cond_bb);
    }

    LLVMPositionBuilderAtEnd(cg->builder, after_bb);
}

static void codegen_block(Codegen* cg, AstNode* node) {
    for (size_t i = 0; i < node->as.block.stmts.count; i++) {
        codegen_stmt(cg, node->as.block.stmts.items[i]);
    }
}

static void codegen_expr_stmt(Codegen* cg, AstNode* node) {
    if (node->as.expr_stmt.expr) {
        codegen_expr(cg, node->as.expr_stmt.expr);
    }
}

static void codegen_stmt(Codegen* cg, AstNode* node) {
    if (!node) return;

    switch (node->kind) {
        case AST_VAR_DECL: codegen_var_decl(cg, node); break;
        case AST_RETURN: codegen_return(cg, node); break;
        case AST_IF: codegen_if(cg, node); break;
        case AST_WHILE: codegen_while(cg, node); break;
        case AST_BLOCK: codegen_block(cg, node); break;
        case AST_EXPR_STMT: codegen_expr_stmt(cg, node); break;
        default: break;
    }
}

static void codegen_fn_decl(Codegen* cg, AstNode* node) {
    // Clear local symbol table for new function
    for (size_t i = 0; i < cg->locals.count; i++) {
        free(cg->locals.names[i]);
    }
    cg->locals.count = 0;

    LLVMTypeRef ret_type = type_to_llvm(cg, node->as.fn_decl.ret_type->as.type_ast.type);

    LLVMTypeRef* param_types = NULL;
    if (node->as.fn_decl.params.count > 0) {
        param_types = (LLVMTypeRef*)malloc(node->as.fn_decl.params.count * sizeof(LLVMTypeRef));
        for (size_t i = 0; i < node->as.fn_decl.params.count; i++) {
            param_types[i] = type_to_llvm(cg, node->as.fn_decl.params.items[i]->as.param.type->as.type_ast.type);
        }
    }

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types, node->as.fn_decl.params.count, false);
    LLVMValueRef func = LLVMAddFunction(cg->module, node->as.fn_decl.name, fn_type);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(cg->context, func, "entry");
    LLVMPositionBuilderAtEnd(cg->builder, entry);

    for (size_t i = 0; i < node->as.fn_decl.params.count; i++) {
        LLVMValueRef param = LLVMGetParam(func, i);
        LLVMSetValueName2(param, node->as.fn_decl.params.items[i]->as.param.name, strlen(node->as.fn_decl.params.items[i]->as.param.name));
        LLVMTypeRef param_type = type_to_llvm(cg, node->as.fn_decl.params.items[i]->as.param.type->as.type_ast.type);
        LLVMValueRef alloca = LLVMBuildAlloca(cg->builder, param_type, node->as.fn_decl.params.items[i]->as.param.name);
        LLVMBuildStore(cg->builder, param, alloca);
        // Add parameter to local symbol table
        locals_add(cg, node->as.fn_decl.params.items[i]->as.param.name, alloca, param_type);
    }

    codegen_stmt(cg, node->as.fn_decl.body);

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
        if (ret_type == LLVMVoidTypeInContext(cg->context)) {
            LLVMBuildRetVoid(cg->builder);
        } else {
            LLVMBuildRet(cg->builder, LLVMConstNull(ret_type));
        }
    }

    if (param_types) free(param_types);
}

bool codegen_generate(Codegen* cg, AstNode* program) {
    if (!cg || !program || program->kind != AST_PROGRAM) return false;

    // Create printf declaration
    LLVMValueRef printf_func = LLVMGetNamedFunction(cg->module, "printf");
    if (!printf_func) {
        LLVMTypeRef int_type = LLVMInt32TypeInContext(cg->context);
        LLVMTypeRef i8_ptr_type = LLVMPointerType(LLVMInt8TypeInContext(cg->context), 0);
        LLVMTypeRef printf_type = LLVMFunctionType(int_type, &i8_ptr_type, 1, true);
        printf_func = LLVMAddFunction(cg->module, "printf", printf_type);
    }

    // Create print function that calls printf
    LLVMValueRef print_func = LLVMGetNamedFunction(cg->module, "print");
    if (!print_func) {
        LLVMTypeRef int_type = LLVMInt32TypeInContext(cg->context);
        LLVMTypeRef print_type = LLVMFunctionType(LLVMVoidTypeInContext(cg->context), &int_type, 1, false);
        print_func = LLVMAddFunction(cg->module, "print", print_type);

        // Generate print function body
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(cg->context, print_func, "entry");
        LLVMBuilderRef old_builder = cg->builder;
        LLVMPositionBuilderAtEnd(cg->builder, entry);

        LLVMValueRef arg = LLVMGetParam(print_func, 0);
        LLVMValueRef format_str = LLVMBuildGlobalStringPtr(cg->builder, "%d\n", "fmt");
        LLVMValueRef args[] = { format_str, arg };
        LLVMBuildCall2(cg->builder, LLVMGlobalGetValueType(printf_func), printf_func, args, 2, "");
        LLVMBuildRetVoid(cg->builder);

        cg->builder = old_builder;
    }

    for (size_t i = 0; i < program->as.program.decls.count; i++) {
        AstNode* decl = program->as.program.decls.items[i];
        if (decl->kind == AST_FN_DECL) {
            codegen_fn_decl(cg, decl);
        }
    }

    char* error = NULL;
    if (LLVMVerifyModule(cg->module, LLVMAbortProcessAction, &error) != 0) {
        fprintf(stderr, "Module verification failed: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }

    return true;
}

bool codegen_emit_object(Codegen* cg, const char* output_path) {
    char* error = NULL;
    if (LLVMTargetMachineEmitToFile(cg->target_machine, cg->module, output_path, LLVMObjectFile, &error) != 0) {
        fprintf(stderr, "Failed to emit object file: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }
    return true;
}

bool codegen_emit_assembly(Codegen* cg, const char* output_path) {
    char* error = NULL;
    if (LLVMTargetMachineEmitToFile(cg->target_machine, cg->module, output_path, LLVMAssemblyFile, &error) != 0) {
        fprintf(stderr, "Failed to emit assembly: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }
    return true;
}

bool codegen_emit_ir(Codegen* cg, const char* output_path) {
    char* error = NULL;
    if (LLVMPrintModuleToFile(cg->module, output_path, &error) != 0) {
        fprintf(stderr, "Failed to emit IR: %s\n", error);
        LLVMDisposeMessage(error);
        return false;
    }
    return true;
}

LLVMValueRef codegen_get_function(Codegen* cg, const char* name) {
    return LLVMGetNamedFunction(cg->module, name);
}