#ifndef GAMESERVER_CRYPTO_RSA_H
#define GAMESERVER_CRYPTO_RSA_H

#include "common/types/types.h"
#include <openssl/rsa.h>

struct RsaPrivateKey {
	RsaPrivateKey(void);
	~RsaPrivateKey(void);
	bool initFromFile(const char *FileName);
	bool decrypt(uint8 *Data); // single 128-byte block

	// DATA
	// =================
	RSA *m_RSA;
};

#endif // GAMESERVER_CRYPTO_RSA_H
