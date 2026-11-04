# Low-Level Language with LLVM C API - Implementation Plan

## Project Overview
- **Goal**: Systems programming language with C++-like RAII memory model
- **Experience Level**: Beginner compiler developer
- **LLVM Integration**: C API (llvm-c)
- **Architecture**: Direct AST → LLVM IR emission
- **Targets**: x86_64, ARM64, RISC-V
- **Build System**: CMake
- **Parser**: Hand-written recursive descent
- **Testing**: Comprehensive (unit + integration + e2e)

---

## Phase 1: Project Setup & Infrastructure (Week 1-2)

### 1.1 Repository Structure
```
project/
├── CMakeLists.txt
├── src/
│   ├── lexer/          # Tokenizer
│   ├── parser/         # Recursive descent parser
│   ├── ast/            # AST node definitions
│   ├── sema/           # Semantic analysis
│   ├── codegen/        # LLVM IR generation
│   ├── driver/         # Compiler driver (main)
│   └── utils/          # Diagnostics, memory arena, etc.
├── tests/
│   ├── unit/           # Unit tests per component
│   ├── integration/    # Multi-pass tests
│   └── e2e/            # End-to-end test programs
├── examples/           # Sample programs
└── docs/               # Language spec, design notes
```

### 1.2 CMake Configuration
- Minimum CMake 3.16
- Find LLVM via `find_package(LLVM REQUIRED CONFIG)`
- Link against: `LLVMCore`, `LLVMSupport`, `LLVMTarget`, `LLVMMC`, `LLVMExecutionEngine`
- Enable C11/C++17 standard
- Add sanitizers (Address, Undefined) for debug builds
- Generate compile_commands.json for LSP

### 1.3 Core Utilities
- **Memory Arena**: Bump allocator for AST nodes (fast, no individual frees)
- **Diagnostics**: Source location tracking, error/warning reporting with colors
- **String Interning**: For identifiers, keywords
- **Target Triple Detection**: Host + cross-compilation support

---

## Phase 2: Lexer (Week 2)

### 2.1 Token Design
```c
typedef enum {
    // Literals
    TOK_INTEGER, TOK_FLOAT, TOK_STRING, TOK_CHAR,
    // Identifiers & Keywords
    TOK_IDENT, TOK_FN, TOK_LET, TOK_MUT, TOK_IF, TOK_ELSE,
    TOK_WHILE, TOK_FOR, TOK_RETURN, TOK_STRUCT, TOK_ENUM,
    TOK_IMPL, TOK_TRAIT, TOK_TYPE, TOK_MATCH, TOK_PUB,
    // Operators
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_EQ, TOK_EQEQ, TOK_NOTEQ, TOK_LT, TOK_GT, TOK_LTE, TOK_GTE,
    TOK_AND, TOK_OR, TOK_NOT, TOK_ARROW, TOK_FAT_ARROW,
    // Delimiters
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET, TOK_COMMA, TOK_SEMI,
    TOK_COLON, TOK_DOT, TOK_DOUBLE_COLON,
    // Special
    TOK_EOF, TOK_ERROR
} TokenKind;
```

### 2.2 Lexer Implementation
- Single-pass, streaming tokenizer
- UTF-8 support for identifiers
- Keyword recognition via perfect hash or trie
- Number parsing: decimal, hex (0x), binary (0b), octal (0o), float
- String escapes: \n, \t, \\, \", \xNN, \u{NNNN}
- Line/column tracking for diagnostics

### 2.3 Unit Tests
- Tokenize all keyword/operator combinations
- Edge cases: unicode identifiers, nested comments, raw strings
- Error recovery: invalid escapes, unterminated strings

---

## Phase 3: Parser - MVP (Week 3-4)

### 3.1 Grammar (MVP - Calculator Level)
```
program        := declaration* EOF
declaration    := fn_decl | var_decl
fn_decl        := "fn" ident "(" param_list? ")" ("->" type)? block
param_list     := param ("," param)*
param          := ident ":" type
var_decl       := "let" ("mut")? ident (":" type)? "=" expr ";"
block          := "{" statement* "}"
statement      := expr_stmt | var_decl | return_stmt | block
expr_stmt      := expr ";"
return_stmt    := "return" expr? ";"
expr           := binary (precedence climbing)
primary        := integer | float | string | ident | "(" expr ")" | call
call           := ident "(" arg_list? ")"
arg_list       := expr ("," expr)*
type           := "i32" | "i64" | "f32" | "f64" | "bool" | "void"
binary_ops     := * / % | + - | < <= > >= | == != | && | ||
```

### 3.2 AST Node Types (MVP)
```c
typedef enum {
    AST_PROGRAM,
    AST_FN_DECL, AST_PARAM,
    AST_VAR_DECL,
    AST_BLOCK,
    AST_RETURN, AST_EXPR_STMT,
    AST_BINARY, AST_UNARY,
    AST_CALL, AST_IDENT, AST_LITERAL,
    AST_TYPE
} AstKind;

typedef struct AstNode {
    AstKind kind;
    SourceLoc loc;
    union {
        struct { AstNode** items; size_t count; } program;
        struct { char* name; AstNode* params; AstNode* ret_type; AstNode* body; } fn_decl;
        struct { char* name; AstNode* type; bool mut; AstNode* init; } var_decl;
        struct { AstNode* lhs; TokenKind op; AstNode* rhs; } binary;
        // ... other variants
    } as;
} AstNode;
```

### 3.3 Parser Implementation
- Recursive descent with precedence climbing for expressions
- Error recovery: synchronize at `;`, `}`, `fn`, `let`
- Single lookahead token
- Produce concrete AST with source locations

### 3.4 Unit Tests
- Parse all valid MVP constructs
- Reject invalid syntax with good error messages
- Round-trip: parse → print → parse (AST equality)

---

## Phase 4: Semantic Analysis - MVP (Week 4-5)

### 4.1 Symbol Table
- Scoped hash map: `Scope { Scope* parent; Map<String, Symbol*> symbols; }`
- Symbol kinds: Function, Variable, Type, Parameter
- Shadowing allowed, redeclaration in same scope = error

### 4.2 Type System (MVP)
- Primitive types: `i8, i16, i32, i64, u8...u64, f32, f64, bool, void`
- Type checking for binary ops, calls, returns, assignments
- Implicit conversions: none (explicit casts only)
- Function signature matching

### 4.3 Semantic Passes
1. **Name Resolution**: Build symbol table, resolve identifiers
2. **Type Checking**: Bottom-up type inference + verification
3. **Const Evaluation**: (Optional) fold constant expressions

### 4.4 Error Reporting
- Multiple errors per run (don't stop at first)
- Structured: file, line, column, message, note, help
- Error codes: E0001, E0002... for documentation linking

---

## Phase 5: LLVM IR Codegen - MVP (Week 5-7)

### 5.1 LLVM C API Setup
```c
// Module, context, builder
LLVMModuleRef module;
LLVMContextRef context;
LLVMBuilderRef builder;

// Target machine for object emission
LLVMTargetMachineRef target_machine;
```

### 5.2 Type Lowering
| Language Type | LLVM Type |
|---------------|-----------|
| i8..i64       | i8..i64   |
| u8..u64       | i8..i64   |
| f32           | float     |
| f64           | double    |
| bool          | i1        |
| void          | void      |

### 5.3 Codegen Strategy
- **Functions**: Create `LLVMFunctionRef`, add basic blocks, emit body
- **Variables**: `LLVMBuildAlloca` in entry block, `store`/`load`
- **Control Flow**: `LLVMBuildBr`, `LLVMBuildCondBr`, phi nodes for SSA
- **Expressions**: Direct mapping to LLVM instructions
- **Calls**: `LLVMBuildCall2` with proper calling convention

### 5.4 Memory Model (C++-like RAII)
- **Destructors**: Register cleanup via `llvm.lifetime.end` or explicit calls
- **Scope-based cleanup**: Emit destructor calls at block exit
- **Move semantics**: (Later) bitcast + lifetime markers

### 5.5 Optimization Pipeline
```c
LLVMPassManagerBuilderRef pm_builder = LLVMPassManagerBuilderCreate();
LLVMPassManagerBuilderSetOptLevel(pm_builder, 2);
LLVMPassManagerBuilderPopulateModulePassManager(pm_builder, module_pm);
LLVMRunPassManager(module_pm, module);
```

### 5.6 Output Modes
- **JIT**: `LLVMExecutionEngine` for REPL / fast iteration
- **Object**: `LLVMTargetMachineEmitToFile` → `.o`
- **Assembly**: `LLVMTargetMachineEmitToFile` → `.s`
- **IR**: `LLVMPrintModuleToFile` → `.ll`

---

## Phase 6: Testing Infrastructure (Week 6-7, parallel)

### 6.1 Unit Tests (C + Check/Unity)
- Lexer: token streams
- Parser: AST structure
- Sema: symbol resolution, type errors
- Codegen: IR pattern matching

### 6.2 Integration Tests
- Multi-file compilation
- Separate compilation + linking
- Cross-module type checking

### 6.3 End-to-End Tests
```bash
# Test program
fn main() -> i32 {
    let mut x: i32 = 0;
    while x < 10 {
        x = x + 1;
    }
    return x;
}
```
- Compile → run → verify exit code / stdout
- Test on x86_64, ARM64 (QEMU), RISC-V (QEMU)

### 6.4 CI Pipeline (GitHub Actions)
- Build on Ubuntu, macOS, Windows
- Test matrix: Debug/Release × x86_64/ARM64
- Run test suite, check for regressions
- Publish artifacts (binaries, test reports)

---

## Phase 7: Language Evolution - Post-MVP

### 7.1 Structs & Pointers
- `struct Name { field: Type, ... }`
- Pointer types: `*Type`, `*mut Type`
- Field access: `expr.field`
- Address-of: `&expr`, `&mut expr`
- Deref: `*expr`

### 7.2 Control Flow Extensions
- `if expr { } else { }` as expression
- `match` expression (exhaustive)
- `loop`, `break`, `continue`
- `for` over ranges/iterators

### 7.3 Polymorphism (Generics + Traits)
```
trait Display {
    fn fmt(self) -> String;
}

fn print<T: Display>(x: T) { ... }

impl Display for i32 { ... }
```
- Monomorphization at codegen time
- Trait objects (dyn Trait) via vtables

### 7.4 Memory Management (Full RAII)
- Destructors: `fn drop(self) { }`
- Move semantics (destructive move)
- Borrow checker (simplified, optional):
  - Unique mutable reference OR multiple shared references
  - Compile-time lifetime analysis

### 7.5 Advanced Features
- Modules / packages / visibility (`pub`, `pub(crate)`)
- Compile-time execution (const fn, comptime)
- Inline assembly
- SIMD intrinsics
- FFI: `extern "C"`, `#[link]`

---

## Risk Assessment & Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| LLVM C API complexity | High | High | Start with minimal IR, use llvm-c docs, study clang's usage |
| Parser ambiguity | Medium | Medium | Precedence climbing, extensive test cases |
| Type system soundness | Medium | High | Formalize rules early, property-based testing |
| Cross-compilation bugs | Medium | Medium | Test on QEMU early, use LLVM target triples correctly |
| Performance (compile time) | Low | Medium | Arena allocation, avoid string copies, profile |
| Scope creep | High | High | Strict MVP, defer all non-calculator features |

---

## Validation Checkpoints

### Checkpoint 1: Lexer + Parser (End of Week 4)
- [ ] All MVP syntax parses correctly
- [ ] Error messages are actionable
- [ ] Round-trip tests pass

### Checkpoint 2: Semantic Analysis (End of Week 5)
- [ ] Symbol resolution works for nested scopes
- [ ] Type errors caught with correct locations
- [ ] No crashes on invalid code

### Checkpoint 3: Codegen + Execution (End of Week 7)
- [ ] MVP programs compile to working executables
- [ ] JIT mode runs simple functions
- [ ] Output matches expected on x86_64 + ARM64
- [ ] Test suite runs in CI

### Checkpoint 4: Structs + Pointers (Post-MVP)
- [ ] Struct layout matches C ABI
- [ ] Pointer arithmetic works
- [ ] Destructors run at scope exit

---

## Recommended Learning Resources

### LLVM C API
- `llvm/include/llvm-c/*.h` - Official headers
- `llvm/examples/HowToUseJIT/HowToUseJIT.c` - JIT example
- `clang/lib/CodeGen/` - Reference for C→IR patterns

### Compiler Construction
- "Crafting Interpreters" (Bob Nystrom) - Recursive descent, bytecode
- "Engineering a Compiler" (Cooper & Torczon) - Optimization theory
- LLVM Language Reference Manual - IR semantics

### Systems Programming
- "The C++ Object Model" (Lippman) - RAII, vtables, layout
- "Linkers and Loaders" (Levine) - Object files, relocation

---

## Next Steps

1. **Initialize repo** with CMake + LLVM detection
2. **Implement lexer** with full test coverage
3. **Build parser** for MVP grammar
4. **Add semantic analysis** (symbol table + type checker)
5. **Wire up LLVM codegen** for functions, variables, control flow
6. **Create test harness** and CI

Would you like me to elaborate on any specific phase, adjust the timeline, or proceed with implementation of Phase 1?