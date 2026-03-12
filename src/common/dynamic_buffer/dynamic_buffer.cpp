#include "common/dynamic_buffer/dynamic_buffer.h"
#include "common/assert/assert.h"

#include <string.h>

// TDynamicWriteBuffer
// =============================================================================
TDynamicWriteBuffer::TDynamicWriteBuffer(int InitialSize)
	: TWriteBuffer(new uint8[InitialSize], InitialSize)
{
	// no-op
}

void TDynamicWriteBuffer::resizeBuffer(void){
	ASSERT(this->Size > 0);
	int Size = this->Size * 2;
	uint8 *Data = new uint8[Size];
	if(this->Data != NULL){
		memcpy(Data, this->Data, this->Size);
		delete[] this->Data;
	}

	this->Data = Data;
	this->Size = Size;
}

void TDynamicWriteBuffer::writeByte(uint8 Byte){
	while((this->Size - this->Position) < 1){
		this->resizeBuffer();
	}

	this->Data[this->Position] = Byte;
	this->Position += 1;
}

void TDynamicWriteBuffer::writeWord(uint16 Word){
	while((this->Size - this->Position) < 2){
		this->resizeBuffer();
	}

	this->Data[this->Position] = (uint8)(Word);
	this->Data[this->Position + 1] = (uint8)(Word >> 8);
	this->Position += 2;
}

void TDynamicWriteBuffer::writeQuad(uint32 Quad){
	while((this->Size - this->Position) < 4){
		this->resizeBuffer();
	}

	this->Data[this->Position] = (uint8)(Quad);
	this->Data[this->Position + 1] = (uint8)(Quad >> 8);
	this->Data[this->Position + 2] = (uint8)(Quad >> 16);
	this->Data[this->Position + 3] = (uint8)(Quad >> 24);
	this->Position += 4;
}

void TDynamicWriteBuffer::writeBytes(const uint8 *Buffer, int Count){
	while((this->Size - this->Position) < Count){
		this->resizeBuffer();
	}

	memcpy(&this->Data[this->Position], Buffer, Count);
	this->Position += Count;
}

TDynamicWriteBuffer::~TDynamicWriteBuffer(void){
	if(this->Data != NULL){
		delete[] this->Data;
	}
}
