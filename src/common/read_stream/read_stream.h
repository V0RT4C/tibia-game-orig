#ifndef GAMESERVER_COMMON_READ_STREAM_H
#define GAMESERVER_COMMON_READ_STREAM_H

#include "common/types/types.h"

struct TReadStream {
	// VIRTUAL FUNCTIONS
	// =================
	virtual bool readFlag(void);														// VTABLE[0]
	virtual uint8 readByte(void) = 0;													// VTABLE[1]
	virtual uint16 readWord(void);														// VTABLE[2]
	virtual uint32 readQuad(void);														// VTABLE[3]
	virtual void read_string(char *Buffer, int MaxLength);								// VTABLE[4]
	virtual void readBytes(uint8 *Buffer, int Count);									// VTABLE[5]
	virtual bool eof(void) = 0;															// VTABLE[6]
	virtual void skip(int Count) = 0;													// VTABLE[7]
};

struct TReadBuffer: TReadStream {
	TReadBuffer(const uint8 *Data, int Size);

	// VIRTUAL FUNCTIONS
	// =================
	uint8 readByte(void) override;
	uint16 readWord(void) override;
	uint32 readQuad(void) override;
	void readBytes(uint8 *Buffer, int Count) override;
	bool eof(void) override;
	void skip(int Count) override;

	// DATA
	// =================
	const uint8 *Data;
	int Size;
	int Position;
};

#endif // GAMESERVER_COMMON_READ_STREAM_H
