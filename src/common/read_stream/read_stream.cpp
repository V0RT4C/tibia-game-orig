#include "common/read_stream/read_stream.h"
#include "logging/logging.h"

#include <string.h>

// TReadStream
// =============================================================================
bool TReadStream::readFlag(void){
	return this->readByte() != 0;
}

uint16 TReadStream::readWord(void){
	// NOTE(fusion): Data is encoded in little endian.
	uint8 Byte0 = this->readByte();
	uint8 Byte1 = this->readByte();
	return ((uint16)Byte1 << 8) | (uint16)Byte0;
}

uint32 TReadStream::readQuad(void){
	// NOTE(fusion): Data is encoded in little endian.
	uint8 Byte0 = this->readByte();
	uint8 Byte1 = this->readByte();
	uint8 Byte2 = this->readByte();
	uint8 Byte3 = this->readByte();
	return ((uint32)Byte3 << 24) | ((uint32)Byte2 << 16)
			| ((uint32)Byte1 << 8) | (uint32)Byte0;
}

void TReadStream::readString(char *Buffer, int MaxLength){
	if(Buffer == NULL || MaxLength == 0){
		error("TReadStream::readString: Provided buffer does not exist.\n");
		throw "internal error";
	}

	int Length = (int)this->readWord();
	if(Length == 0xFFFF){
		Length = (int)this->readQuad();
	}

	if(Length > 0){
		if(MaxLength < 0 || MaxLength > Length){
			this->readBytes((uint8*)Buffer, Length);
			Buffer[Length] = 0;
		}else{
			this->readBytes((uint8*)Buffer, MaxLength - 1);
			this->skip(Length - MaxLength + 1);
			Buffer[MaxLength - 1] = 0;
		}
	}else{
		Buffer[0] = 0;
	}
}

void TReadStream::readBytes(uint8 *Buffer, int Count){
	if(Buffer == NULL){
		error("TReadStream::readBytes: Provided buffer does not exist.\n");
		throw "internal error";
	}

	for(int i = 0; i < Count; i += 1){
		Buffer[i] = this->readByte();
	}
}

// TReadBuffer
// =============================================================================
TReadBuffer::TReadBuffer(const uint8 *Data, int Size){
	if(Data == NULL){
		error("TReadBuffer::TReadBuffer: data is NULL.\n");
		Size = 0;
	}else if(Size < 0){
		error("TReadBuffer::TReadBuffer: Invalid data size %d.\n", Size);
		Size = 0;
	}

	this->Data = Data;
	this->Size = Size;
	this->Position = 0;
}

uint8 TReadBuffer::readByte(void){
	if((this->Size - this->Position) < 1){
		throw "buffer empty";
	}

	uint8 Byte = this->Data[this->Position];
	this->Position += 1;
	return Byte;
}

uint16 TReadBuffer::readWord(void){
	if((this->Size - this->Position) < 2){
		throw "buffer empty";
	}

	uint8 Byte0 = this->Data[this->Position];
	uint8 Byte1 = this->Data[this->Position + 1];
	this->Position += 2;
	return ((uint16)Byte1 << 8) | (uint16)Byte0;
}

uint32 TReadBuffer::readQuad(void){
	if((this->Size - this->Position) < 4){
		throw "buffer empty";
	}

	uint8 Byte0 = this->Data[this->Position];
	uint8 Byte1 = this->Data[this->Position + 1];
	uint8 Byte2 = this->Data[this->Position + 2];
	uint8 Byte3 = this->Data[this->Position + 3];
	this->Position += 4;
	return ((uint32)Byte3 << 24) | ((uint32)Byte2 << 16)
			| ((uint32)Byte1 << 8) | (uint32)Byte0;
}

void TReadBuffer::readBytes(uint8 *Buffer, int Count){
	if(Buffer == NULL || Count <= 0){
		error("TReadBuffer::readBytes: Provided buffer does not exist.\n");
		throw "buffer not existing";
	}

	if((this->Size - this->Position) < Count){
		throw "buffer empty";
	}

	memcpy(Buffer, &this->Data[this->Position], Count);
	this->Position += Count;
}

bool TReadBuffer::eof(void){
	return this->Size <= this->Position;
}

void TReadBuffer::skip(int Count){
	if((this->Size - this->Position) < Count){
		throw "buffer empty";
	}

	this->Position += Count;
}
