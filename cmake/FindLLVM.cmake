# Find LLVM for Vassian.
#
# On Windows the official LLVM binary distribution (including the Chocolatey
# package) ships LLVM-C.lib and LLVM-C.dll, but NOT the llvm-c development
# headers. In that case we fall back to the vendored headers in
# cmake/llvm-c-headers so the C API can still be compiled against.

set(_vassian_llvm_roots "")

# Explicit hints first.
if(DEFINED LLVM_ROOT_DIR AND NOT "${LLVM_ROOT_DIR}" STREQUAL "")
    list(APPEND _vassian_llvm_roots "${LLVM_ROOT_DIR}")
endif()
if(DEFINED ENV{LLVM_ROOT_DIR})
    list(APPEND _vassian_llvm_roots "$ENV{LLVM_ROOT_DIR}")
endif()

# LLVM_DIR is conventionally <root>/lib/cmake/llvm, but accept a root too.
if(DEFINED LLVM_DIR AND NOT "${LLVM_DIR}" STREQUAL "")
    list(APPEND _vassian_llvm_roots "${LLVM_DIR}")
    get_filename_component(_vassian_llvm_dir_root "${LLVM_DIR}/../.." ABSOLUTE)
    list(APPEND _vassian_llvm_roots "${_vassian_llvm_dir_root}")
endif()

# Common install locations.
list(APPEND _vassian_llvm_roots
    "C:/Program Files/LLVM"
    "C:/Program Files (x86)/LLVM"
    "$ENV{ProgramW6432}/LLVM"
    "$ENV{USERPROFILE}/scoop/apps/llvm/current"
    "$ENV{ChocolateyInstall}/lib/llvm/tools/LLVM"
    "$ENV{ChocolateyInstall}/lib/llvm/tools"
)

# Keep a user-provided cache value; otherwise pick the first valid root.
set(LLVM_ROOT_DIR "${LLVM_ROOT_DIR}" CACHE PATH "LLVM installation root")
if(NOT EXISTS "${LLVM_ROOT_DIR}/lib/LLVM-C.lib")
    set(LLVM_ROOT_DIR "")
    foreach(_root IN LISTS _vassian_llvm_roots)
        if(EXISTS "${_root}/lib/LLVM-C.lib")
            set(LLVM_ROOT_DIR "${_root}" CACHE PATH "LLVM installation root" FORCE)
            break()
        endif()
    endforeach()
endif()

if(NOT EXISTS "${LLVM_ROOT_DIR}/lib/LLVM-C.lib")
    message(FATAL_ERROR
        "Could not find LLVM-C.lib. Install LLVM (e.g. 'choco install llvm'), "
        "or pass -DLLVM_ROOT_DIR=<path to your LLVM installation>.")
endif()

set(LLVM_INCLUDE_DIRS "${LLVM_ROOT_DIR}/include")
set(LLVM_LIBRARY_DIRS "${LLVM_ROOT_DIR}/lib")
set(LLVM_BINARY_DIRS "${LLVM_ROOT_DIR}/bin")
set(LLVM_LIBRARIES "${LLVM_LIBRARY_DIRS}/LLVM-C.lib")

# The Windows binary distribution usually has no llvm-c headers. Use the
# vendored headers in that case (they are compatible with the C ABI).
set(_vassian_vendored_headers "${CMAKE_CURRENT_LIST_DIR}/llvm-c-headers")
if(NOT EXISTS "${LLVM_INCLUDE_DIRS}/llvm-c/Core.h")
    list(APPEND LLVM_INCLUDE_DIRS "${_vassian_vendored_headers}")
    message(STATUS "LLVM-C headers not found in LLVM install; using vendored headers")
endif()

if(NOT EXISTS "${LLVM_INCLUDE_DIRS}/llvm-c/Core.h" AND
   NOT EXISTS "${_vassian_vendored_headers}/llvm-c/Core.h")
    message(FATAL_ERROR "llvm-c/Core.h still not found after adding vendored headers")
endif()

if(NOT EXISTS "${LLVM_BINARY_DIRS}/LLVM-C.dll")
    message(WARNING "LLVM-C.dll not found in ${LLVM_BINARY_DIRS}; the built executable may fail to start")
endif()

message(STATUS "Found LLVM at ${LLVM_ROOT_DIR}")
message(STATUS "LLVM libraries: ${LLVM_LIBRARIES}")

# Allow running the freshly built executable/tests without manually editing PATH.
if(EXISTS "${LLVM_BINARY_DIRS}")
    set(ENV{PATH} "${LLVM_BINARY_DIRS};$ENV{PATH}")
endif()
