enable_testing()

add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(e2e)

add_test(NAME vassian_driver COMMAND vassian)