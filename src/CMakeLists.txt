file(GLOB_RECURSE SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ast/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sema/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/codegen/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/*.c
)

add_library(vassian_lib ${SRC_FILES})
target_include_directories(vassian_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer
    ${CMAKE_CURRENT_SOURCE_DIR}/parser
    ${CMAKE_CURRENT_SOURCE_DIR}/ast
    ${CMAKE_CURRENT_SOURCE_DIR}/sema
    ${CMAKE_CURRENT_SOURCE_DIR}/codegen
    ${CMAKE_CURRENT_SOURCE_DIR}/driver
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${LLVM_INCLUDE_DIRS})
target_compile_definitions(vassian_lib PUBLIC LLVM_ENABLE_THREADS=0)

if(WIN32)
    target_link_libraries(vassian_lib ${LLVM_LIBRARIES})
else()
    target_link_libraries(vassian_lib LLVM)
endif()

add_executable(vassian ${CMAKE_CURRENT_SOURCE_DIR}/driver/main.c)
target_link_libraries(vassian vassian_lib)

if(NOT WIN32)
    target_link_libraries(vassian LLVM)
endif()

if(WIN32 AND EXISTS "${LLVM_BINARY_DIRS}/LLVM-C.dll")
    add_custom_command(TARGET vassian POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LLVM_BINARY_DIRS}/LLVM-C.dll"
            "$<TARGET_FILE_DIR:vassian>/LLVM-C.dll")
endif()