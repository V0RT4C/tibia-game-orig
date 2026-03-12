#ifndef GAMESERVER_CRYPTO_XTEA_H
#define GAMESERVER_CRYPTO_XTEA_H

#include "common/types/types.h"
#include "common/read_stream/read_stream.h"

struct XteaSymmetricKey {
	void init(TReadBuffer *Buffer);
	void encrypt(uint8 *Data); // single 8-byte block
	void decrypt(uint8 *Data); // single 8-byte block

	// DATA
	// =================
	uint32 m_SymmetricKey[4];
};

#endif // GAMESERVER_CRYPTO_XTEA_H
