file(GLOB TEST_SOURCES *.c)
add_executable(test_utils ${TEST_SOURCES})
target_link_libraries(test_utils vassian_lib)
add_test(NAME test_utils COMMAND test_utils)

if(WIN32 AND EXISTS "${LLVM_BINARY_DIRS}/LLVM-C.dll")
    add_custom_command(TARGET test_utils POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LLVM_BINARY_DIRS}/LLVM-C.dll"
            "$<TARGET_FILE_DIR:test_utils>/LLVM-C.dll")
endif()