# Tests will be added incrementally as modules are extracted.
# Each test file links against the modules it tests.

add_executable(test_byte_order test_byte_order.cpp)
target_include_directories(test_byte_order PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_byte_order PRIVATE cxx_std_17)
add_test(NAME test_byte_order COMMAND test_byte_order)
