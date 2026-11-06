#include "vassian_utils.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "ast/ast.h"
#include "sema/sema.h"
#include "codegen/codegen.h"
#include <llvm-c/Target.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VASSIAN_VERSION "0.1.0"
#define VASSIAN_GITHUB_REPO "VassiaQT/Vassian"

#ifdef _WIN32
#define VASSIAN_DEFAULT_OUTPUT "a.exe"
#else
#define VASSIAN_DEFAULT_OUTPUT "a.out"
#endif

static void print_version(void) {
    printf("Vassian %s\n", VASSIAN_VERSION);
    printf("GitHub: https://github.com/%s\n", VASSIAN_GITHUB_REPO);
}

static void print_update_info(void) {
    printf("To update Vassian:\n");
    printf("  1. Download latest release from:\n");
    printf("     https://github.com/%s/releases/latest\n", VASSIAN_GITHUB_REPO);
    printf("  2. Or build from source:\n");
    printf("     git pull && mkdir build && cd build && cmake .. && make\n");
    printf("  3. Or use package manager (if available):\n");
    printf("     scoop update vassian\n");
    printf("     choco upgrade vassian\n");
}

static const char* read_file(const char* filename, size_t* out_len) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = (char*)malloc(len + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buffer, 1, len, f);
    buffer[read] = '\0';
    fclose(f);

    *out_len = read;
    return buffer;
}

static void print_usage(const char* prog) {
    printf("Usage: %s [options] <input.vas>\n", prog);
    printf("Options:\n");
    printf("  -o <file>      Output file (default: %s)\n", VASSIAN_DEFAULT_OUTPUT);
    printf("  --emit-llvm    Emit LLVM IR (.ll)\n");
    printf("  --emit-asm     Emit assembly (.s)\n");
    printf("  --emit-obj     Emit object file (.o)\n");
    printf("  -v, --version  Show version info\n");
    printf("  --update       Show update instructions\n");
    printf("  -h, --help     Show this help\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* input_file = NULL;
    const char* output_file = VASSIAN_DEFAULT_OUTPUT;
    enum { EMIT_EXE, EMIT_LLVM, EMIT_ASM, EMIT_OBJ } emit_mode = EMIT_EXE;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "--update") == 0) {
            print_update_info();
            return 0;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "--emit-llvm") == 0) {
            emit_mode = EMIT_LLVM;
        } else if (strcmp(argv[i], "--emit-asm") == 0) {
            emit_mode = EMIT_ASM;
        } else if (strcmp(argv[i], "--emit-obj") == 0) {
            emit_mode = EMIT_OBJ;
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    if (!input_file) {
        fprintf(stderr, "No input file specified\n");
        return 1;
    }

    size_t source_len;
    const char* source = read_file(input_file, &source_len);
    if (!source) {
        fprintf(stderr, "Failed to read file: %s\n", input_file);
        return 1;
    }

    Arena* arena = arena_create(0);
    if (!arena) {
        fprintf(stderr, "Failed to create arena\n");
        free((void*)source);
        return 1;
    }

    StringInterner* interner = interner_create(arena);
    if (!interner) {
        fprintf(stderr, "Failed to create interner\n");
        arena_destroy(arena);
        free((void*)source);
        return 1;
    }

    Lexer* lexer = lexer_create(source, source_len, input_file, interner);
    if (!lexer) {
        fprintf(stderr, "Failed to create lexer\n");
        arena_destroy(arena);
        free((void*)source);
        return 1;
    }

    Parser* parser = parser_create(lexer, arena);
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        arena_destroy(arena);
        free((void*)source);
        return 1;
    }

    AstNode* program = parser_parse(parser);
    if (parser_had_error(parser)) {
        fprintf(stderr, "Parse errors occurred\n");
        arena_destroy(arena);
        free((void*)source);
        return 1;
    }

    Sema* sema = sema_create(arena);
    if (!sema) {
        fprintf(stderr, "Failed to create semantic analyzer\n");
        arena_destroy(arena);
        free((void*)source);
        return 1;
    }

    if (!sema_analyze(sema, program)) {
        fprintf(stderr, "Semantic errors occurred\n");
        arena_destroy(arena);
        free((void*)source);
        return 1;
    }

    TargetInfo* target = target_info_create();
    if (!target) {
        fprintf(stderr, "Failed to get target info\n");
        arena_destroy(arena);
        free((void*)source);
        return 1;
    }

    Codegen* cg = codegen_create("vassian", target_info_get_triple(target), target_info_get_cpu(target), target_info_get_features(target));
    if (!cg) {
        fprintf(stderr, "Failed to create codegen\n");
        target_info_destroy(target);
        arena_destroy(arena);
        free((void*)source);
        return 1;
    }

    if (!codegen_generate(cg, program)) {
        fprintf(stderr, "Code generation failed\n");
        codegen_destroy(cg);
        target_info_destroy(target);
        arena_destroy(arena);
        free((void*)source);
        return 1;
    }

    bool success = false;
    switch (emit_mode) {
        case EMIT_LLVM:
            success = codegen_emit_ir(cg, output_file);
            break;
        case EMIT_ASM:
            success = codegen_emit_assembly(cg, output_file);
            break;
        case EMIT_OBJ:
            success = codegen_emit_object(cg, output_file);
            break;
        case EMIT_EXE:
            if (codegen_emit_object(cg, "temp.o")) {
                char cmd[1024];
#ifdef _WIN32
                snprintf(cmd, sizeof(cmd), "clang temp.o -o \"%s\"", output_file);
#else
                snprintf(cmd, sizeof(cmd), "clang -no-pie temp.o -o \"%s\"", output_file);
#endif
                success = system(cmd) == 0;
                remove("temp.o");
            }
            break;
    }

    codegen_destroy(cg);
    target_info_destroy(target);
    arena_destroy(arena);
    free((void*)source);

    if (success) {
        printf("Compilation successful: %s\n", output_file);
        return 0;
    } else {
        fprintf(stderr, "Compilation failed\n");
        return 1;
    }
}