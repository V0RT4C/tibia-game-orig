# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
cmake -B build && cmake --build build -j$(nproc)           # release build
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)  # debug build
cmake -B build -DENABLE_ASSERTIONS=ON && cmake --build build -j$(nproc)    # release + assertions
cd build && ctest --output-on-failure                       # run all tests
cd build && ctest -R test_xtea --output-on-failure          # run a single test
```

The only external dependency is OpenSSL's libcrypto (`apt install libssl-dev` on Debian).

## What This Is

A decompiled and rewritten Tibia 7.7 MMORPG game server in C++17, Linux-only. The codebase preserves the original server architecture (signal-driven game loop, thread-per-connection, exception-based error handling) while modernizing to compile with current toolchains.

The game server requires a separate [Query Manager](https://github.com/fusion32/tibia-querymanager) to boot (handles character database). Optional companions: Login Server, Web Server.

## Architecture

**Game loop**: Signal-driven via POSIX timers. `SIGALRM` fires on each game beat to advance game state (`AdvanceGame` in `src/main/main.cpp`). `SIGUSR1` signals incoming client data for processing.

**Threading model**:
- Main thread: game logic (signal-driven loop in `LaunchGame`)
- Per-connection threads: client I/O, authentication, protocol parsing (64KB stack each)
- Reader thread: async disk loads (sectors, characters) via `src/game_data/reader/`
- Writer thread: async disk saves via `src/game_data/writer/`

**Network**: Dual transport — TCP (port 7172) and WebSocket (port 7979) via `ITransport` interface. Custom binary protocol, not HTTP. Client connections managed in `src/network/connection_pool/`.

**Protocol**: Binary message protocol defined in `src/protocol/enums/`. Receiving (client->server parsing) in `src/protocol/receiving/`, sending (server->client) in `src/protocol/sending/`. Thread-safe IPC via `src/protocol/communication/`.

**World data**: Tile-based map split into 32x32 sectors. Objects (items) have 62+ attributes per type. Sectors load/save asynchronously via the reader/writer threads.

**Creature hierarchy**: `TCreature` base -> `TPlayer`, `TNonplayer` (-> `TMonster`, `TNpc`). All in `src/creature/`.

**Crypto**: RSA key exchange (OpenSSL) + XTEA stream encryption, in `src/crypto/`.

## Key Source Layout

- `src/main/main.cpp` — entry point, initialization sequence (`InitAll`), game loop (`LaunchGame`, `AdvanceGame`)
- `src/creature/` — player, monster, NPC, combat, skills, race system
- `src/game_data/` — map, objects, houses, magic, moveuse, operate, scripts, reader/writer
- `src/protocol/` — binary protocol enums, receiving, sending, communication IPC
- `src/network/` — connection pool, TCP/WebSocket transports
- `src/common/` — utilities, byte order, read/write streams, dynamic buffers, containers, SPSC ring buffer
- `src/config/` — `.tibia` config file parsing
- `src/query/` — Query Manager client
- `tests/` — doctest-based tests, each test is a standalone executable

## CMake Options

- `ENABLE_ASSERTIONS=ON` — runtime assertions (always on in Debug)
- `TIBIA772=ON` — target Tibia 7.72 protocol variant
- `CMAKE_BUILD_TYPE=Debug` — debug build with `-Og`

## Conventions

- C++17 standard, no extensions (`CMAKE_CXX_EXTENSIONS OFF`)
- Strict warnings: `-pedantic -Wall -Wextra`
- clang-tidy checks: bugprone-*, modernize-use-nullptr/override/using, readability-braces-around-statements, performance-*
- Test framework: doctest (single-header, in `tests/doctest.h`)
- Each test links only the specific source files it needs (no monolithic test binary)
- `-rdynamic` is used for backtrace symbols on crash