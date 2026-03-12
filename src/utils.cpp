#include "common.h"

static TErrorFunction *ErrorFunction;
static TPrintFunction *PrintFunction;
static char StandardLogFile[4096];

void SetErrorFunction(TErrorFunction *Function){
	ErrorFunction = Function;
}

void SetPrintFunction(TPrintFunction *Function){
	PrintFunction = Function;
}

void SetStandardLogFile(const char *FileName){
	strcpy(StandardLogFile, FileName);
}

void SilentHandler(const char *Text){
	// no-op
}

void SilentHandler(int Level, const char *Text){
	// no-op
}

void LogFileHandler(const char *Text){
	if(StandardLogFile[0] == 0){
		return;
	}

	FILE *File = fopen(StandardLogFile, "at");
	if(File != NULL){
		fprintf(File, "%s", Text);
		if(fclose(File) != 0){
			error("LogfileHandler: Error %d closing the file.\n", errno);
		}
	}
}

void LogFileHandler(int Level, const char *Text){
	LogFileHandler(Text);
}

void error(const char *Text, ...){
	char s[1024];

	va_list ap;
	va_start(ap, Text);
	vsnprintf(s, sizeof(s), Text, ap);
	va_end(ap);

	if(ErrorFunction){
		ErrorFunction(s);
	}else{
		printf("%s", s);
	}
}

void print(int Level, const char *Text, ...){
	char s[1024];

	va_list ap;
	va_start(ap, Text);
	vsnprintf(s, sizeof(s), Text, ap);
	va_end(ap);

	if(PrintFunction){
		PrintFunction(Level, s);
	}else{
		printf("%s", s);
	}
}

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

void TWriteStream::writeString(const char *String){
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
