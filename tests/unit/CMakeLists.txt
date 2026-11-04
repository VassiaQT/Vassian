file(GLOB TEST_SOURCES *.c)
add_executable(test_utils ${TEST_SOURCES})
target_link_libraries(test_utils vassian_lib)
add_test(NAME test_utils COMMAND test_utils)