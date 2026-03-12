cmake_minimum_required(VERSION 3.16)
project(tibia-gameserver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_ASSERTIONS "Enable runtime assertions" OFF)
option(TIBIA772 "Target Tibia 7.72 protocol" OFF)

# All source files (flat structure for now — will be reorganized in later tasks)
set(SOURCES
    src/communication.cpp
    src/config.cpp
    src/connections.cpp
    src/cract.cpp
    src/crcombat.cpp
    src/crmain.cpp
    src/crnonpl.cpp
    src/crplayer.cpp
    src/crskill.cpp
    src/crypto.cpp
    src/houses.cpp
    src/info.cpp
    src/magic.cpp
    src/main.cpp
    src/map.cpp
    src/moveuse.cpp
    src/objects.cpp
    src/operate.cpp
    src/query.cpp
    src/reader.cpp
    src/receiving.cpp
    src/script.cpp
    src/sending.cpp
    src/shm.cpp
    src/strings.cpp
    src/threads.cpp
    src/time.cpp
    src/utils.cpp
    src/writer.cpp
)

add_executable(game ${SOURCES})

target_include_directories(game PRIVATE src)

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
set_source_files_properties(src/crypto.cpp PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")

find_package(OpenSSL REQUIRED)
target_link_libraries(game PRIVATE OpenSSL::Crypto pthread)

# Tests
enable_testing()
add_subdirectory(tests)
