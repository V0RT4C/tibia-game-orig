cmake_minimum_required(VERSION 3.16)
project(tibia-gameserver LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_ASSERTIONS "Enable runtime assertions" OFF)
option(TIBIA772 "Target Tibia 7.72 protocol" OFF)

# uSockets — lightweight C networking library (dependency of uWebSockets)
# Explicit file list — do NOT glob. Exclude crypto/openssl.c (we use LIBUS_NO_SSL)
# and quic.c (has extra dependencies we don't need).
set(USOCKETS_SOURCES
    third_party/uSockets/src/bsd.c
    third_party/uSockets/src/context.c
    third_party/uSockets/src/loop.c
    third_party/uSockets/src/socket.c
    third_party/uSockets/src/udp.c
    third_party/uSockets/src/eventing/epoll_kqueue.c
    third_party/uSockets/src/crypto/sni_tree.cpp
)
add_library(uSockets STATIC ${USOCKETS_SOURCES})
target_include_directories(uSockets PUBLIC third_party/uSockets/src)
target_compile_definitions(uSockets PRIVATE LIBUS_NO_SSL=1)
target_compile_options(uSockets PRIVATE -Wno-unused-parameter -Wno-pedantic)

# All source files (flat structure for now — will be reorganized in later tasks)
set(SOURCES
    src/common/utils/utils.cpp
    src/logging/logging.cpp
    src/protocol/communication/communication.cpp
    src/config/config.cpp
    src/network/connection_pool/connection_pool.cpp
    src/network/connection/connection.cpp
    src/creature/actions/actions.cpp
    src/creature/combat/combat.cpp
    src/creature/creature/creature.cpp
    src/creature/creature_main/creature_main.cpp
    src/creature/race/race.cpp
    src/creature/monster/monster.cpp
    src/creature/npc/npc.cpp
    src/creature/nonplayer/nonplayer.cpp
    src/creature/player/player.cpp
    src/creature/skill/skill.cpp
    src/crypto/xtea/xtea.cpp
    src/crypto/rsa/rsa.cpp
    src/game_data/houses/houses.cpp
    src/game_data/info/info.cpp
    src/game_data/magic/magic.cpp
    src/main/main.cpp
    src/game_data/map/map.cpp
    src/game_data/moveuse/moveuse.cpp
    src/game_data/objects/objects.cpp
    src/game_data/operate/operate.cpp
    src/query/query_client/query_client.cpp
    src/query/query_connection_pool/query_connection_pool.cpp
    src/game_data/reader/reader.cpp
    src/protocol/receiving/receiving.cpp
    src/game_data/script/script.cpp
    src/protocol/sending/sending.cpp
    src/shm/shm.cpp
    src/common/strings/strings.cpp
    src/threads/threads.cpp
    src/threads/semaphore/semaphore.cpp
    src/time/time_utils.cpp
    src/common/read_stream/read_stream.cpp
    src/common/write_stream/write_stream.cpp
    src/common/dynamic_buffer/dynamic_buffer.cpp
    src/common/binary_file/binary_file.cpp
    src/game_data/writer/writer.cpp
    src/network/transport/tcp_transport/tcp_transport.cpp
    src/network/transport/websocket_transport/websocket_transport.cpp
)

add_executable(game ${SOURCES})

target_include_directories(game PRIVATE src)
target_include_directories(game PRIVATE third_party/uWebSockets/src)

target_compile_options(game PRIVATE
    -m64
    -fno-strict-aliasing
    -pedantic
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-format-truncation
    -pthread
)

target_compile_definitions(game PRIVATE
    OS_LINUX=1
    ARCH_X64=1
    $<$<BOOL:${ENABLE_ASSERTIONS}>:ENABLE_ASSERTIONS=1>
    $<$<BOOL:${TIBIA772}>:TIBIA772=1>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(game PRIVATE -g -Og)
    target_compile_definitions(game PRIVATE ENABLE_ASSERTIONS=1)
else()
    target_compile_options(game PRIVATE -O2)
endif()

# Suppress OpenSSL deprecated API warnings for the RSA compilation unit only
set_source_files_properties(src/crypto/rsa/rsa.cpp PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")

find_package(OpenSSL REQUIRED)
target_link_libraries(game PRIVATE OpenSSL::Crypto pthread uSockets z)

# Tests
enable_testing()
add_subdirectory(tests)
