#ifndef GAMESERVER_COMMON_WRITE_STREAM_H
#define GAMESERVER_COMMON_WRITE_STREAM_H

#include "common/types/types.h"

struct TWriteStream {
	// VIRTUAL FUNCTIONS
	// =================
	virtual void writeFlag(bool Flag);													// VTABLE[0]
	virtual void writeByte(uint8 Byte) = 0;												// VTABLE[1]
	virtual void writeWord(uint16 Word);												// VTABLE[2]
	virtual void writeQuad(uint32 Quad);												// VTABLE[3]
	virtual void write_string(const char *String);										// VTABLE[4]
	virtual void writeBytes(const uint8 *Buffer, int Count);							// VTABLE[5]
};

struct TWriteBuffer: TWriteStream {
	TWriteBuffer(uint8 *Data, int Size);

	// VIRTUAL FUNCTIONS
	// =================
	void writeByte(uint8 Byte) override;
	void writeWord(uint16 Word) override;
	void writeQuad(uint32 Quad) override;
	void writeBytes(const uint8 *Buffer, int Count) override;

	// DATA
	// =================
	uint8 *Data;
	int Size;
	int Position;
};

#endif // GAMESERVER_COMMON_WRITE_STREAM_H
