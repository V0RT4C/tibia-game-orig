#ifndef GAMESERVER_COMMON_BYTE_ORDER_H
#define GAMESERVER_COMMON_BYTE_ORDER_H

#include "common/types/types.h"

// Little-endian read helpers (matching TReadBuffer encoding)
inline uint16 read_u16_le(const uint8* buf) {
    return static_cast<uint16>(buf[0])
         | (static_cast<uint16>(buf[1]) << 8);
}

inline uint32 read_u32_le(const uint8* buf) {
    return static_cast<uint32>(buf[0])
         | (static_cast<uint32>(buf[1]) << 8)
         | (static_cast<uint32>(buf[2]) << 16)
         | (static_cast<uint32>(buf[3]) << 24);
}

// Little-endian write helpers (matching TWriteBuffer encoding)
inline void write_u16_le(uint8* buf, uint16 val) {
    buf[0] = static_cast<uint8>(val);
    buf[1] = static_cast<uint8>(val >> 8);
}

inline void write_u32_le(uint8* buf, uint32 val) {
    buf[0] = static_cast<uint8>(val);
    buf[1] = static_cast<uint8>(val >> 8);
    buf[2] = static_cast<uint8>(val >> 16);
    buf[3] = static_cast<uint8>(val >> 24);
}

#endif // GAMESERVER_COMMON_BYTE_ORDER_H
