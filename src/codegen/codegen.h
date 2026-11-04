#ifndef VASSIAN_CODEGEN_H
#define VASSIAN_CODEGEN_H

#include "ast.h"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    LLVMModuleRef module;
    LLVMContextRef context;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    const char* triple;
    const char* cpu;
    const char* features;
    // Local variable symbol table
    struct {
        char** names;
        LLVMValueRef* values;
        LLVMTypeRef* types;
        size_t count;
        size_t capacity;
    } locals;
} Codegen;

Codegen* codegen_create(const char* module_name, const char* triple, const char* cpu, const char* features);
void codegen_destroy(Codegen* cg);
bool codegen_generate(Codegen* cg, AstNode* program);
bool codegen_emit_object(Codegen* cg, const char* output_path);
bool codegen_emit_assembly(Codegen* cg, const char* output_path);
bool codegen_emit_ir(Codegen* cg, const char* output_path);
LLVMValueRef codegen_get_function(Codegen* cg, const char* name);

#ifdef __cplusplus
}
#endif

#endif