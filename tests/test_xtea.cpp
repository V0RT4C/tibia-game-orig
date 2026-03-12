#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "crypto/xtea/xtea.h"
#include <cstring>

TEST_CASE("XTEA encrypt-decrypt roundtrip") {
	XteaSymmetricKey key;
	key.m_SymmetricKey[0] = 0x01234567;
	key.m_SymmetricKey[1] = 0x89ABCDEF;
	key.m_SymmetricKey[2] = 0xFEDCBA98;
	key.m_SymmetricKey[3] = 0x76543210;

	uint8 original[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
	uint8 data[8];
	memcpy(data, original, 8);

	key.encrypt(data);
	CHECK(memcmp(data, original, 8) != 0);

	key.decrypt(data);
	CHECK(memcmp(data, original, 8) == 0);
}

TEST_CASE("XTEA all-zero block roundtrip") {
	XteaSymmetricKey key;
	key.m_SymmetricKey[0] = 0;
	key.m_SymmetricKey[1] = 0;
	key.m_SymmetricKey[2] = 0;
	key.m_SymmetricKey[3] = 0;

	uint8 data[8] = {0};
	uint8 original[8] = {0};

	key.encrypt(data);
	key.decrypt(data);
	CHECK(memcmp(data, original, 8) == 0);
}
