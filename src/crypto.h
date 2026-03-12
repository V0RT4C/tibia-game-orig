#ifndef TIBIA_CRYPTO_H_
#define TIBIA_CRYPTO_H_ 1
#include "crypto/xtea/xtea.h"
#include "crypto/rsa/rsa.h"
// Preserve old names as aliases for backward compatibility during migration
using TRSAPrivateKey = RsaPrivateKey;
using TXTEASymmetricKey = XteaSymmetricKey;
#endif
