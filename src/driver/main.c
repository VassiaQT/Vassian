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

#define VASSIAN_VERSION "0.0.1"
#define VASSIAN_GITHUB_REPO "VassiaQT/Vassian"

#ifdef _WIN32
#include <process.h>
#define VASSIAN_GETPID _getpid
#define VASSIAN_DEFAULT_OUTPUT "a.exe"
#define VASSIAN_EXE_SUFFIX ".exe"
#else
#include <sys/wait.h>
#include <unistd.h>
#define VASSIAN_GETPID getpid
#define VASSIAN_DEFAULT_OUTPUT "a.out"
#define VASSIAN_EXE_SUFFIX ""
#endif

typedef enum {
    EMIT_EXE,
    EMIT_LLVM,
    EMIT_ASM,
    EMIT_OBJ
} EmitMode;

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

static int run_command(const char* cmd) {
    int rc = system(cmd);
#ifdef _WIN32
    return rc;
#else
    if (rc == -1) return -1;
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return -1;
#endif
}

static void print_usage(const char* prog) {
    printf("Vassian %s - the Vassian language compiler\n\n", VASSIAN_VERSION);
    printf("Usage:\n");
    printf("  %s <input.vas> [options]        Compile to an executable\n", prog);
    printf("  %s build <input.vas> [options]  Compile to an executable\n", prog);
    printf("  %s run <input.vas> [args...]    Compile and run immediately\n", prog);
    printf("  %s --version                    Show version info\n", prog);
    printf("  %s --help                       Show this help\n\n", prog);
    printf("Options:\n");
    printf("  -o <file>      Output file (default: %s)\n", VASSIAN_DEFAULT_OUTPUT);
    printf("  --emit-llvm    Emit LLVM IR (.ll)\n");
    printf("  --emit-asm     Emit assembly (.s)\n");
    printf("  --emit-obj     Emit object file (.o)\n");
    printf("  --update       Show update instructions\n");
}

static bool compile_file(const char* input_file, const char* output_file, EmitMode emit_mode) {
    size_t source_len;
    const char* source = read_file(input_file, &source_len);
    if (!source) {
        fprintf(stderr, "Failed to read file: %s\n", input_file);
        return false;
    }

    bool success = false;
    Arena* arena = NULL;
    StringInterner* interner = NULL;
    Lexer* lexer = NULL;
    Parser* parser = NULL;
    Sema* sema = NULL;
    TargetInfo* target = NULL;
    Codegen* cg = NULL;

    arena = arena_create(0);
    if (!arena) {
        fprintf(stderr, "Failed to create arena\n");
        goto cleanup;
    }

    interner = interner_create(arena);
    if (!interner) {
        fprintf(stderr, "Failed to create interner\n");
        goto cleanup;
    }

    lexer = lexer_create(source, source_len, input_file, interner);
    if (!lexer) {
        fprintf(stderr, "Failed to create lexer\n");
        goto cleanup;
    }

    parser = parser_create(lexer, arena);
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        goto cleanup;
    }

    AstNode* program = parser_parse(parser);
    if (parser_had_error(parser)) {
        fprintf(stderr, "Parse errors occurred\n");
        goto cleanup;
    }

    sema = sema_create(arena);
    if (!sema) {
        fprintf(stderr, "Failed to create semantic analyzer\n");
        goto cleanup;
    }

    if (!sema_analyze(sema, program)) {
        fprintf(stderr, "Semantic errors occurred\n");
        goto cleanup;
    }

    target = target_info_create();
    if (!target) {
        fprintf(stderr, "Failed to get target info\n");
        goto cleanup;
    }

    cg = codegen_create("vassian", target_info_get_triple(target), target_info_get_cpu(target), target_info_get_features(target));
    if (!cg) {
        fprintf(stderr, "Failed to create codegen\n");
        goto cleanup;
    }

    if (!codegen_generate(cg, program)) {
        fprintf(stderr, "Code generation failed\n");
        goto cleanup;
    }

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
        case EMIT_EXE: {
            char obj_file[64];
            snprintf(obj_file, sizeof(obj_file), "vassian_%d.o", (int)VASSIAN_GETPID());
            if (codegen_emit_object(cg, obj_file)) {
                char cmd[2048];
#ifdef _WIN32
                snprintf(cmd, sizeof(cmd), "clang \"%s\" -o \"%s\"", obj_file, output_file);
#else
                snprintf(cmd, sizeof(cmd), "clang -no-pie \"%s\" -o \"%s\"", obj_file, output_file);
#endif
                success = run_command(cmd) == 0;
                remove(obj_file);
                if (!success) {
                    fprintf(stderr,
                        "Linking failed. Vassian uses 'clang' to link executables;\n"
                        "make sure clang is installed and available on your PATH.\n");
                }
            }
            break;
        }
    }

cleanup:
    if (cg) codegen_destroy(cg);
    if (target) target_info_destroy(target);
    if (arena) arena_destroy(arena);
    free((void*)source);
    return success;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* command = "build";
    const char* input_file = NULL;
    const char* output_file = NULL;
    EmitMode emit_mode = EMIT_EXE;
    int prog_arg_start = argc;

    int i = 1;
    if (strcmp(argv[i], "build") == 0 || strcmp(argv[i], "run") == 0) {
        command = argv[i];
        i++;
    }

    for (; i < argc; i++) {
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
        } else if (strcmp(argv[i], "--") == 0) {
            prog_arg_start = i + 1;
            break;
        } else if (argv[i][0] != '-') {
            if (!input_file) {
                input_file = argv[i];
            } else if (strcmp(command, "run") == 0) {
                prog_arg_start = i;
                break;
            } else {
                fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
                return 1;
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    if (!input_file) {
        fprintf(stderr, "No input file specified\n\n");
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(command, "run") == 0) {
        char temp_output[64];
        snprintf(temp_output, sizeof(temp_output), "vassian_run_%d%s",
                 (int)VASSIAN_GETPID(), VASSIAN_EXE_SUFFIX);

        if (!compile_file(input_file, temp_output, EMIT_EXE)) {
            remove(temp_output);
            fprintf(stderr, "Compilation failed\n");
            return 1;
        }

        char cmd[4096];
        int n;
#ifdef _WIN32
        n = snprintf(cmd, sizeof(cmd), "\"%s\"", temp_output);
#else
        n = snprintf(cmd, sizeof(cmd), "\"./%s\"", temp_output);
#endif
        for (int a = prog_arg_start; a < argc && n > 0 && (size_t)n < sizeof(cmd); a++) {
            n += snprintf(cmd + n, sizeof(cmd) - n, " \"%s\"", argv[a]);
        }

        int rc = run_command(cmd);
        remove(temp_output);
        return rc == 0 ? 0 : 1;
    }

    if (!output_file) output_file = VASSIAN_DEFAULT_OUTPUT;

    if (!compile_file(input_file, output_file, emit_mode)) {
        fprintf(stderr, "Compilation failed\n");
        return 1;
    }

    printf("Compilation successful: %s\n", output_file);
    return 0;
}
