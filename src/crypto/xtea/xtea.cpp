#include "crypto/xtea/xtea.h"

// XteaSymmetricKey
// =============================================================================
void XteaSymmetricKey::init(TReadBuffer *Buffer){
	m_SymmetricKey[0] = Buffer->readQuad();
	m_SymmetricKey[1] = Buffer->readQuad();
	m_SymmetricKey[2] = Buffer->readQuad();
	m_SymmetricKey[3] = Buffer->readQuad();
}

void XteaSymmetricKey::encrypt(uint8 *Data){
	// TODO(fusion): This assumes both data endpoints have the same byte order.
	// It's unlikely that there is anything other than little-endian but we
	// should use a few helping functions to ensure compatibility.
	uint32 Sum = 0x00000000UL;
	uint32 Delta = 0x9E3779B9UL;
	uint32 V0 = *(uint32*)(&Data[0]);
	uint32 V1 = *(uint32*)(&Data[4]);
	for(int i = 0; i < 32; i += 1){
		V0 += (((V1 << 4) ^ (V1 >> 5)) + V1) ^ (Sum + m_SymmetricKey[Sum & 3]);
		Sum += Delta;
		V1 += (((V0 << 4) ^ (V0 >> 5)) + V0) ^ (Sum + m_SymmetricKey[(Sum >> 11) & 3]);
	}
	*(uint32*)(&Data[0]) = V0;
	*(uint32*)(&Data[4]) = V1;
}

void XteaSymmetricKey::decrypt(uint8 *Data){
	// TODO(fusion): Same as above.
	uint32 Sum = 0xC6EF3720UL;
	uint32 Delta = 0x9E3779B9UL;
	uint32 V0 = *(uint32*)(&Data[0]);
	uint32 V1 = *(uint32*)(&Data[4]);
	for(int i = 0; i < 32; i += 1){
		V1 -= (((V0 << 4) ^ (V0 >> 5)) + V0) ^ (Sum + m_SymmetricKey[(Sum >> 11) & 3]);
		Sum -= Delta;
		V0 -= (((V1 << 4) ^ (V1 >> 5)) + V1) ^ (Sum + m_SymmetricKey[Sum & 3]);
	}
	*(uint32*)(&Data[0]) = V0;
	*(uint32*)(&Data[4]) = V1;
}
