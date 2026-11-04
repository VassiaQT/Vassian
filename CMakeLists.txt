cmake_minimum_required(VERSION 3.16)
project(Vassian LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zi /Ob0 /Od /RTC1")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi /Ob0 /Od /RTC1")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
    endif()
endif()

if(WIN32)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
    find_package(LLVM REQUIRED)
else()
    find_package(LLVM REQUIRED CONFIG)
endif()

message(STATUS "Found LLVM")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libraries: ${LLVM_LIBRARIES}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(-DLLVM_ENABLE_THREADS=0)

# Modern LLVM linking
if(NOT WIN32)
    add_definitions(${LLVM_DEFINITIONS})
    include_directories(${LLVM_INCLUDE_DIRS})
endif()

add_subdirectory(src)
add_subdirectory(tests)