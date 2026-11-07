#Syntax of Vassian language

## Basics

- Variables:

To declare them:
var [type] [name]; 
or 
var [type] [name] = [value];

type must be always written, and value can be assigned later (var is just here to declare we create a variable, to assign a value, it is not needed ; same for the type)

structs will be considered as variables, enums too:
var struct { var int a, var float b };
var enum { MONDAY = 1, TUESDAY = 2 };

- Conditions:

if ([condition]) {

}
elif ([condition]) {

}
else {

}

----

while ([condition]) {

}

----
for ([condition]) {

}

In the condition, you can have the ability to write:
for (var int i = 10;  i < 20; i = i + 1) {

} 

or 

var int i = 10;
for (i; i < 20; i = i + 1) {

}


- Functions:

fn [name]() -> [type] {
    return [something];
}

ex: 
fn add(int a, int b) -> int {
    return a+b;
}

we always need a fn main() -> int {} to be the start of our program (same thing than main in C++). Return must be always have the ; at the end. 

- Console:

print([value]); 


----

## Building on Windows

The LLVM Windows binaries ship `LLVM-C.lib`/`LLVM-C.dll` but not the `llvm-c`
development headers. This repository vendors the matching headers under
`cmake/llvm-c-headers`, so only LLVM itself needs to be installed:

    choco install llvm --version=18.1.8 -y

Configure and build with Ninja + clang-cl:

    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="C:\Program Files\LLVM\bin\clang-cl.exe" -DCMAKE_CXX_COMPILER="C:\Program Files\LLVM\bin\clang-cl.exe" -DLLVM_ROOT_DIR="C:\Program Files\LLVM" -B build
    cmake --build build --parallel 2

The build automatically copies `LLVM-C.dll` next to `vassian.exe`. Then run:

    build\src\vassian.exe --version
    build\src\vassian.exe test.vas

GitHub Actions builds the same way (see `.github/workflows/build-windows.yml`).

## Installing a release (Windows)

Download and run `vassian-<version>-windows-x64-setup.exe` from the releases
page. The installer copies `vassian.exe` and `LLVM-C.dll` into
`%LOCALAPPDATA%\Programs\Vassian` and adds that folder to your user `PATH`.
Open a **new** terminal afterwards so the updated `PATH` is picked up.

`vassian.exe` is a command-line compiler, not a GUI application. Double-clicking
it opens a console and closes it immediately because no arguments were passed.
Run it from a terminal instead.

Compiling to a native executable also requires `clang` on your `PATH`
(`choco install llvm`, or install LLVM from https://releases.llvm.org/).
Emitting IR, assembly, or object files does not require `clang`.

## Command line

    vassian --version
    vassian --help

    vassian build hello.vas              # -> a.exe
    vassian build hello.vas -o hello.exe
    vassian run hello.vas                # compile and run

    vassian hello.vas                    # same as "vassian build hello.vas"

    vassian hello.vas --emit-llvm -o hello.ll
    vassian hello.vas --emit-asm  -o hello.s
    vassian hello.vas --emit-obj  -o hello.o