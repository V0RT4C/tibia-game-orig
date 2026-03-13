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

add_executable(test_containers test_containers.cpp ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_containers PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_containers PRIVATE cxx_std_17)
target_compile_definitions(test_containers PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_containers COMMAND test_containers)

add_executable(test_read_stream test_read_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/read_stream/read_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_read_stream PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_read_stream PRIVATE cxx_std_17)
target_compile_definitions(test_read_stream PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_read_stream COMMAND test_read_stream)

add_executable(test_write_stream test_write_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/write_stream/write_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_write_stream PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_write_stream PRIVATE cxx_std_17)
target_compile_definitions(test_write_stream PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_write_stream COMMAND test_write_stream)

add_executable(test_dynamic_buffer test_dynamic_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/common/dynamic_buffer/dynamic_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/common/write_stream/write_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_dynamic_buffer PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_dynamic_buffer PRIVATE cxx_std_17)
target_compile_definitions(test_dynamic_buffer PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_dynamic_buffer COMMAND test_dynamic_buffer)

add_executable(test_binary_file test_binary_file.cpp
    ${CMAKE_SOURCE_DIR}/src/common/binary_file/binary_file.cpp
    ${CMAKE_SOURCE_DIR}/src/common/read_stream/read_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/write_stream/write_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_binary_file PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_binary_file PRIVATE cxx_std_17)
target_compile_definitions(test_binary_file PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_binary_file COMMAND test_binary_file)

add_executable(test_strings test_strings.cpp
    ${CMAKE_SOURCE_DIR}/src/common/strings/strings.cpp
    ${CMAKE_SOURCE_DIR}/src/common/utils/utils.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_strings PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_strings PRIVATE cxx_std_17)
target_compile_definitions(test_strings PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_strings COMMAND test_strings)

# test_config: struct layout and buffer size checks only (compile-time sizeof, no
# runtime calls into config parsing machinery).
add_executable(test_config test_config.cpp)
target_include_directories(test_config PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_config PRIVATE cxx_std_17)
target_compile_definitions(test_config PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_config COMMAND test_config)

add_executable(test_xtea test_xtea.cpp
    ${CMAKE_SOURCE_DIR}/src/crypto/xtea/xtea.cpp
    ${CMAKE_SOURCE_DIR}/src/common/read_stream/read_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_xtea PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_xtea PRIVATE cxx_std_17)
target_compile_definitions(test_xtea PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_xtea COMMAND test_xtea)

add_executable(test_semaphore test_semaphore.cpp
    ${CMAKE_SOURCE_DIR}/src/threads/semaphore/semaphore.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_semaphore PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_semaphore PRIVATE cxx_std_17)
target_compile_definitions(test_semaphore PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
target_link_libraries(test_semaphore PRIVATE pthread)
add_test(NAME test_semaphore COMMAND test_semaphore)

add_executable(test_char_script test_char_script.cpp
    ${CMAKE_SOURCE_DIR}/src/game_data/script/script.cpp
    ${CMAKE_SOURCE_DIR}/src/common/binary_file/binary_file.cpp
    ${CMAKE_SOURCE_DIR}/src/common/read_stream/read_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/write_stream/write_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/strings/strings.cpp
    ${CMAKE_SOURCE_DIR}/src/common/utils/utils.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_script PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_script PRIVATE cxx_std_17)
target_compile_definitions(test_char_script PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_script COMMAND test_char_script)

add_executable(test_char_objects test_char_objects.cpp
    ${CMAKE_SOURCE_DIR}/src/game_data/objects/objects.cpp
    ${CMAKE_SOURCE_DIR}/src/config/config.cpp
    ${CMAKE_SOURCE_DIR}/src/game_data/script/script.cpp
    ${CMAKE_SOURCE_DIR}/src/common/binary_file/binary_file.cpp
    ${CMAKE_SOURCE_DIR}/src/common/read_stream/read_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/write_stream/write_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/strings/strings.cpp
    ${CMAKE_SOURCE_DIR}/src/common/utils/utils.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_objects PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_objects PRIVATE cxx_std_17)
target_compile_definitions(test_char_objects PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_objects COMMAND test_char_objects)

add_executable(test_char_map test_char_map.cpp
    ${CMAKE_SOURCE_DIR}/src/game_data/objects/objects.cpp
    ${CMAKE_SOURCE_DIR}/src/config/config.cpp
    ${CMAKE_SOURCE_DIR}/src/game_data/script/script.cpp
    ${CMAKE_SOURCE_DIR}/src/common/binary_file/binary_file.cpp
    ${CMAKE_SOURCE_DIR}/src/common/read_stream/read_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/write_stream/write_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/strings/strings.cpp
    ${CMAKE_SOURCE_DIR}/src/common/utils/utils.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_map PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_map PRIVATE cxx_std_17)
target_compile_definitions(test_char_map PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_map COMMAND test_char_map)

add_executable(test_char_houses test_char_houses.cpp
    ${CMAKE_SOURCE_DIR}/src/game_data/objects/objects.cpp
    ${CMAKE_SOURCE_DIR}/src/config/config.cpp
    ${CMAKE_SOURCE_DIR}/src/game_data/script/script.cpp
    ${CMAKE_SOURCE_DIR}/src/common/binary_file/binary_file.cpp
    ${CMAKE_SOURCE_DIR}/src/common/read_stream/read_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/write_stream/write_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/strings/strings.cpp
    ${CMAKE_SOURCE_DIR}/src/common/utils/utils.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_houses PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_houses PRIVATE cxx_std_17)
target_compile_definitions(test_char_houses PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_houses COMMAND test_char_houses)

add_executable(test_char_operate test_char_operate.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_operate PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_operate PRIVATE cxx_std_17)
target_compile_definitions(test_char_operate PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_operate COMMAND test_char_operate)

add_executable(test_char_moveuse test_char_moveuse.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_moveuse PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_moveuse PRIVATE cxx_std_17)
target_compile_definitions(test_char_moveuse PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_moveuse COMMAND test_char_moveuse)

add_executable(test_char_magic test_char_magic.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_magic PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_magic PRIVATE cxx_std_17)
target_compile_definitions(test_char_magic PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_magic COMMAND test_char_magic)

# test_char_skill: skill struct layout characterization tests (compile-time sizeof,
# array sizes). Does NOT link skill.cpp.
add_executable(test_char_skill test_char_skill.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_skill PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_skill PRIVATE cxx_std_17)
target_compile_definitions(test_char_skill PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_skill COMMAND test_char_skill)

# test_char_combat: combat struct layout characterization tests (compile-time sizeof,
# array sizes). Does NOT link combat.cpp.
add_executable(test_char_combat test_char_combat.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_combat PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_combat PRIVATE cxx_std_17)
target_compile_definitions(test_char_combat PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_combat COMMAND test_char_combat)

add_executable(test_char_monster test_char_monster.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_monster PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_monster PRIVATE cxx_std_17)
target_compile_definitions(test_char_monster PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_monster COMMAND test_char_monster)

add_executable(test_char_npc test_char_npc.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_npc PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_npc PRIVATE cxx_std_17)
target_compile_definitions(test_char_npc PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_npc COMMAND test_char_npc)

add_executable(test_char_player test_char_player.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_player PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_player PRIVATE cxx_std_17)
target_compile_definitions(test_char_player PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_player COMMAND test_char_player)

add_executable(test_char_race test_char_race.cpp
    ${CMAKE_SOURCE_DIR}/src/game_data/objects/objects.cpp
    ${CMAKE_SOURCE_DIR}/src/config/config.cpp
    ${CMAKE_SOURCE_DIR}/src/game_data/script/script.cpp
    ${CMAKE_SOURCE_DIR}/src/common/binary_file/binary_file.cpp
    ${CMAKE_SOURCE_DIR}/src/common/read_stream/read_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/write_stream/write_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/strings/strings.cpp
    ${CMAKE_SOURCE_DIR}/src/common/utils/utils.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_race PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_race PRIVATE cxx_std_17)
target_compile_definitions(test_char_race PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_char_race COMMAND test_char_race)

add_executable(test_tcp_transport test_tcp_transport.cpp
    ${CMAKE_SOURCE_DIR}/src/network/transport/tcp_transport/tcp_transport.cpp)
target_include_directories(test_tcp_transport PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_tcp_transport PRIVATE cxx_std_17)
target_compile_definitions(test_tcp_transport PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
add_test(NAME test_tcp_transport COMMAND test_tcp_transport)

add_executable(test_char_connection test_char_connection.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_char_connection PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_char_connection PRIVATE cxx_std_17)
target_compile_definitions(test_char_connection PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
    target_include_directories(test_char_connection SYSTEM PRIVATE ${OPENSSL_INCLUDE_DIR})
endif()
add_test(NAME test_char_connection COMMAND test_char_connection)

add_executable(test_spsc_ring_buffer test_spsc_ring_buffer.cpp)
target_include_directories(test_spsc_ring_buffer PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_spsc_ring_buffer PRIVATE cxx_std_17)
target_compile_definitions(test_spsc_ring_buffer PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1)
target_link_libraries(test_spsc_ring_buffer PRIVATE pthread)
add_test(NAME test_spsc_ring_buffer COMMAND test_spsc_ring_buffer)

add_executable(test_websocket_transport test_websocket_transport.cpp
    ${CMAKE_SOURCE_DIR}/src/network/transport/websocket_transport/websocket_transport.cpp
    ${CMAKE_SOURCE_DIR}/src/logging/logging.cpp)
target_include_directories(test_websocket_transport PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/uWebSockets/src
    ${CMAKE_SOURCE_DIR}/third_party/uSockets/src
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_websocket_transport PRIVATE cxx_std_17)
target_compile_definitions(test_websocket_transport PRIVATE OS_LINUX=1 ENABLE_ASSERTIONS=1 LIBUS_NO_SSL=1)
target_link_libraries(test_websocket_transport PRIVATE uSockets pthread z)
add_test(NAME test_websocket_transport COMMAND test_websocket_transport)
