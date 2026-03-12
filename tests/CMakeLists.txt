# Tests will be added incrementally as modules are extracted.
# Each test file links against the modules it tests.

add_executable(test_byte_order test_byte_order.cpp)
target_include_directories(test_byte_order PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_byte_order PRIVATE cxx_std_17)
add_test(NAME test_byte_order COMMAND test_byte_order)

add_executable(test_utils test_utils.cpp ${CMAKE_SOURCE_DIR}/src/common/utils/utils.cpp)
target_include_directories(test_utils PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_utils PRIVATE cxx_std_17)
target_compile_definitions(test_utils PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_utils COMMAND test_utils)

add_executable(test_logging test_logging.cpp ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_logging PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_logging PRIVATE cxx_std_17)
target_compile_definitions(test_logging PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_logging COMMAND test_logging)
