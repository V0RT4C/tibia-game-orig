#include "common/write_stream/write_stream.h"
#include "common/assert/assert.h"
#include "logging/logging.h"

#include <string.h>

// TWriteStream
// =============================================================================
void TWriteStream::writeFlag(bool Flag){
	this->writeByte((uint8)Flag);
}

void TWriteStream::writeWord(uint16 Word){
	this->writeByte((uint8)(Word));
	this->writeByte((uint8)(Word >> 8));
}

void TWriteStream::writeQuad(uint32 Quad){
	this->writeByte((uint8)(Quad));
	this->writeByte((uint8)(Quad >> 8));
	this->writeByte((uint8)(Quad >> 16));
	this->writeByte((uint8)(Quad >> 24));
}

void TWriteStream::write_string(const char *String){
	if(String == NULL){
		this->writeWord(0);
		return;
	}

	int StringLength = (int)strlen(String);
	ASSERT(StringLength >= 0);
	if(StringLength < 0xFFFF){
		this->writeWord((uint16)StringLength);
	}else{
		this->writeWord(0xFFFF);
		this->writeQuad((uint32)StringLength);
	}

	if(StringLength > 0){
		this->writeBytes((const uint8*)String, StringLength);
	}
}

void TWriteStream::writeBytes(const uint8 *Buffer, int Count){
	if(Buffer == NULL){
		error("TWriteStream::writeBytes: Provided buffer does not exist.\n");
		throw "internal error";
	}

	for(int i = 0; i < Count; i += 1){
		this->writeByte(Buffer[i]);
	}
}

// TWriteBuffer
// =============================================================================
TWriteBuffer::TWriteBuffer(uint8 *Data, int Size){
	if(Data == NULL){
		error("TWriteBuffer::TWriteBuffer: data is NULL.\n");
		Size = 0;
	}else if(Size < 0){
		error("TWriteBuffer::TWriteBuffer: Invalid data size %d.\n", Size);
		Size = 0;
	}

	this->Data = Data;
	this->Size = Size;
	this->Position = 0;
}

void TWriteBuffer::writeByte(uint8 Byte){
	if((this->Size - this->Position) < 1){
		throw "buffer full";
	}

	this->Data[this->Position] = Byte;
	this->Position += 1;
}

void TWriteBuffer::writeWord(uint16 Word){
	if((this->Size - this->Position) < 2){
		throw "buffer full";
	}

	this->Data[this->Position] = (uint8)(Word);
	this->Data[this->Position + 1] = (uint8)(Word >> 8);
	this->Position += 2;
}

void TWriteBuffer::writeQuad(uint32 Quad){
	if((this->Size - this->Position) < 4){
		throw "buffer full";
	}

	this->Data[this->Position] = (uint8)(Quad);
	this->Data[this->Position + 1] = (uint8)(Quad >> 8);
	this->Data[this->Position + 2] = (uint8)(Quad >> 16);
	this->Data[this->Position + 3] = (uint8)(Quad >> 24);
	this->Position += 4;
}

void TWriteBuffer::writeBytes(const uint8 *Buffer, int Count){
	if((this->Size - this->Position) < Count){
		throw "buffer full";
	}

	memcpy(&this->Data[this->Position], Buffer, Count);
	this->Position += Count;
}
