#ifndef TIBIA_CRYPTO_H_
#define TIBIA_CRYPTO_H_ 1

#include "common.h"
#include <openssl/rsa.h>

struct TRSAPrivateKey{
	TRSAPrivateKey(void);
	~TRSAPrivateKey(void);
	bool initFromFile(const char *FileName);
	bool decrypt(uint8 *Data); // single 128 bytes block

	// DATA
	// =================
	RSA *m_RSA;
};

struct TXTEASymmetricKey{
	void init(TReadBuffer *Buffer);
	void encrypt(uint8 *Data); // single 8 bytes block
	void decrypt(uint8 *Data); // single 8 bytes block

	// DATA
	// =================
	uint32 m_SymmetricKey[4];
};

#endif //TIBIA_CRYPTO_H_
