#ifndef GAMESERVER_COMMON_BINARY_FILE_H
#define GAMESERVER_COMMON_BINARY_FILE_H

#include "common/read_stream/read_stream.h"
#include "common/write_stream/write_stream.h"
#include <cstdio>

struct TReadBinaryFile: TReadStream {
	TReadBinaryFile(void);
	bool open(const char *FileName);
	void close(void);
	void error(const char *Text);
	int getPosition(void);
	int getSize(void);
	void seek(int Offset);

	// VIRTUAL FUNCTIONS
	// =================
	uint8 readByte(void) override;
	void readBytes(uint8 *Buffer, int Count) override;
	bool eof(void) override;
	void skip(int Count) override;

	// TODO(fusion): Appended virtual functions. These are not in the base class
	// VTABLE which can be problematic if we intend to use polymorphism, although
	// that doesn't seem to be case.
	virtual ~TReadBinaryFile(void);														// VTABLE[8]
	// Duplicate destructor that also calls operator delete.							// VTABLE[9]

	// DATA
	// =================
	FILE *File;
	char Filename[4096];
	int FileSize;
};

struct TWriteBinaryFile: TWriteStream {
	TWriteBinaryFile(void);
	bool open(const char *FileName);
	void close(void);
	void error(const char *Text);

	// VIRTUAL FUNCTIONS
	// =================
	void writeByte(uint8 Byte) override;
	void writeBytes(const uint8 *Buffer, int Count) override;

	// TODO(fusion): Appended virtual functions. These are not in the base class
	// VTABLE which can be problematic if we intend to use polymorphism, although
	// that doesn't seem to be case.
	virtual ~TWriteBinaryFile(void);

	// DATA
	// =================
	FILE *File;
	char Filename[4096];
};

#endif // GAMESERVER_COMMON_BINARY_FILE_H
