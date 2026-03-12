#ifndef GAMESERVER_COMMON_DYNAMIC_BUFFER_H
#define GAMESERVER_COMMON_DYNAMIC_BUFFER_H

#include "common/write_stream/write_stream.h"

struct TDynamicWriteBuffer: TWriteBuffer {
	TDynamicWriteBuffer(int InitialSize);
	void resizeBuffer(void);

	// VIRTUAL FUNCTIONS
	// =================
	void writeByte(uint8 Byte) override;
	void writeWord(uint16 Word) override;
	void writeQuad(uint32 Quad) override;
	void writeBytes(const uint8 *Buffer, int Count) override;

	// TODO(fusion): Appended virtual functions. These are not in the base class
	// VTABLE which can be problematic if we intend to use polymorphism, although
	// that doesn't seem to be case.
	virtual ~TDynamicWriteBuffer(void);													// VTABLE[6]
	// Duplicate destructor that also calls operator delete.							// VTABLE[7]
};

#endif // GAMESERVER_COMMON_DYNAMIC_BUFFER_H
