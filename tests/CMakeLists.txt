add_executable(wgcs_tests test_main.cpp)
target_link_libraries(wgcs_tests PRIVATE wgcs_core)
add_test(NAME wgcs_tests COMMAND wgcs_tests)
