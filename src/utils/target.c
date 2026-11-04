#include "vassian_utils.h"
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Core.h>
#include <stdlib.h>
#include <string.h>

struct TargetInfo {
    char* triple;
    char* cpu;
    char* features;
    bool big_endian;
    uint64_t pointer_size;
};

static char* strdup_or_null(const char* str) {
    if (!str) return NULL;
    char* copy = malloc(strlen(str) + 1);
    if (copy) strcpy(copy, str);
    return copy;
}

TargetInfo* target_info_create(void) {
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmParser();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    TargetInfo* target = (TargetInfo*)malloc(sizeof(TargetInfo));
    if (!target) return NULL;

    char* default_triple = LLVMGetDefaultTargetTriple();
    target->triple = default_triple;
    target->cpu = strdup_or_null("generic");
    target->features = strdup_or_null("");
    target->big_endian = false;
    target->pointer_size = 8;

    LLVMTargetRef target_ref;
    char* error = NULL;
    if (LLVMGetTargetFromTriple(target->triple, &target_ref, &error) == 0) {
        LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
            target_ref, target->triple, target->cpu, target->features,
            LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault
        );
        if (tm) {
            LLVMDisposeTargetMachine(tm);
        }
    }
    if (error) LLVMDisposeMessage(error);

    return target;
}

void target_info_destroy(TargetInfo* target) {
    if (!target) return;
    if (target->triple) LLVMDisposeMessage(target->triple);
    free(target->cpu);
    free(target->features);
    free(target);
}

const char* target_info_get_triple(TargetInfo* target) {
    return target ? target->triple : NULL;
}

const char* target_info_get_cpu(TargetInfo* target) {
    return target ? target->cpu : NULL;
}

const char* target_info_get_features(TargetInfo* target) {
    return target ? target->features : NULL;
}

bool target_info_is_big_endian(TargetInfo* target) {
    return target ? target->big_endian : false;
}

uint64_t target_info_get_pointer_size(TargetInfo* target) {
    return target ? target->pointer_size : 8;
}