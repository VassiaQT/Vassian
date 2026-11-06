/* Minimal llvm-config.h used when building against the LLVM Windows binary
 * distribution, which does not ship development headers. The LLVM-C headers
 * only include this file; they do not depend on its macros. */
#ifndef LLVM_CONFIG_H
#define LLVM_CONFIG_H

#define LLVM_VERSION_MAJOR 18
#define LLVM_VERSION_MINOR 1
#define LLVM_VERSION_PATCH 8
#define LLVM_VERSION_STRING "18.1.8"

#ifndef LLVM_ENABLE_THREADS
#define LLVM_ENABLE_THREADS 1
#endif

#endif
