#ifndef GAMESERVER_COMMON_TYPES_H
#define GAMESERVER_COMMON_TYPES_H

#include <cstddef>
#include <cstdint>
#include <iterator> // for std::size

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using int64 = int64_t;
using uint64 = uint64_t;
using uintptr = uintptr_t;
using usize = size_t;

constexpr usize kb(usize x) { return x << 10; }
constexpr usize mb(usize x) { return x << 20; }
constexpr usize gb(usize x) { return x << 30; }

constexpr bool is_pow2(usize x) { return x != 0 && (x & (x - 1)) == 0; }

// Array length helper — works in all contexts including Type::Member,
// this->Member, static_assert, and VLA-like declarations.
#define NARRAY(arr) static_cast<int>(sizeof(arr) / sizeof((arr)[0]))

#ifndef TIBIA772
#define TIBIA772 0
#endif

#endif // GAMESERVER_COMMON_TYPES_H
