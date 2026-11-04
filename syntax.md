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

llvm-config --version
ls "C:\Program Files\LLVM\include\llvm-c\"  # Should show Core.h, Target.h, etc.
Then rebuild:
cd C:\Users\vassi\Documents\Vassian\build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug