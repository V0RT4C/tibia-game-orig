#include "common/binary_file/binary_file.h"
#include "logging/logging.h"
#include <cerrno>
#include <cstdio>
#include <cstring>

// NOTE(fusion): Throwing a character array on the stack as an exception won't
// work because it implicitly decays into a character pointer which would then
// point to invalid data when actually parsed. It is the reason `ErrorString` is
// declared statically, or else you'd need to throw some heap allocated string
// which then becomes ambiguous whether you should free it or not.
static char ErrorString[100];

// Helper Functions
// =============================================================================
static void SaveFile(const char *Filename){
	if(Filename == NULL){
		error("SaveFile: Filename is NULL.\n");
		return;
	}

	char BackupFilename[4096];

	strcpy(BackupFilename, Filename);
	strcat(BackupFilename, "#");

	FILE *Source = fopen(Filename, "rb");
	if(Source == NULL){
		error("SaveFile: Source file %s does not exist.\n", Filename);
		return;
	}

	FILE *Dest = fopen(BackupFilename, "wb");
	if(Dest == NULL){
		error("SaveFile: Cannot create destination file %s.\n", BackupFilename);
		fclose(Source);
		return;
	}

	char Buffer[16384];
	while(true){
		size_t n = fread(Buffer, 1, sizeof(Buffer), Source);
		if(n == 0){
			if(ferror(Source)){
				error("SaveFile: Error reading source file %s.\n", Filename);
			}
			break;
		}

		if(fwrite(Buffer, 1, n, Dest) != n){
			error("SaveFile: Error writing destination file %s.\n", BackupFilename);
			break;
		}
	}

	if(fclose(Source) != 0){
		error("SaveFile: Error %d closing source file.\n", errno);
	}

	if(fclose(Dest) != 0){
		error("SaveFile: Error %d closing destination file.\n", errno);
	}
}

// TReadBinaryFile REGULAR FUNCTIONS
//==============================================================================
TReadBinaryFile::TReadBinaryFile(void){
	this->File = NULL;
}

bool TReadBinaryFile::open(const char *FileName){
	if(this->File != NULL){
		this->error("File still open");
	}

	this->File = fopen(FileName, "rb");
	if(this->File == NULL){
		return false;
	}

	strcpy(this->Filename, FileName);
	this->FileSize = -1;
	return true;
}

void TReadBinaryFile::close(void){
	// TODO(fusion): Check if file is NULL?
	if(fclose(this->File) != 0){
		int ErrCode = errno;
		::error("TReadBinaryFile::close: Error closing file.\n");
		::error("# File: %s, error code: %d (%s)\n",
				this->Filename, ErrCode, strerror(ErrCode));
	}
	this->File = NULL;
}

void TReadBinaryFile::error(const char *Text){
	if(this->File != NULL){
		if(fclose(this->File) != 0){
			::error("TReadBinaryFile::error: Error %d closing file.\n", errno);
		}
		this->File = NULL;
	}

	snprintf(ErrorString, sizeof(ErrorString),
			"error in binary-file \"%s\": %s.",
			this->Filename, Text);

	throw ErrorString;
}

int TReadBinaryFile::getPosition(void){
	// TODO(fusion): Check if file is NULL?
	return (int)ftell(this->File);
}

int TReadBinaryFile::getSize(void){
	// TODO(fusion): Check if file is NULL?
	int Size = this->FileSize;
	if(Size == -1){
		long Position = ftell(this->File);
		fseek(this->File, 0, SEEK_END);
		Size = (int)ftell(this->File);
		fseek(this->File, Position, SEEK_SET);
		this->FileSize = Size;
	}
	return Size;
}

void TReadBinaryFile::seek(int Offset){
	if(this->File == NULL){
		this->error("File not open for seek");
	}

	if(Offset < 0){
		this->error("Negative offset for seek");
	}

	fseek(this->File, (long)Offset, 0);
}

// TReadBinaryFile VIRTUAL FUNCTIONS
//==============================================================================
uint8 TReadBinaryFile::readByte(void){
	uint8 Byte;
	int Result = (int)fread(&Byte, 1, 1, this->File);
	if(Result != 1){
		int ErrCode = errno;
		int Position = this->getPosition();
		::error("TReadBinaryFile::readByte: Error reading a byte\n");
		::error("# File: %s, position: %d, return value: %d, error code: %d (%s)\n",
				this->Filename, Position, Result, ErrCode, strerror(ErrCode));

		// NOTE(fusion): Close file and make a backup, possibly for further inspection.
		if(fclose(this->File) != 0){
			::error("TReadBinaryFile::readByte: Error %d closing file.\n", errno);
		}
		this->File = NULL;
		SaveFile(this->Filename);

		this->error("Error while reading byte");
	}
	return Byte;
}

void TReadBinaryFile::readBytes(uint8 *Buffer, int Count){
	int Result = (int)fread(Buffer, 1, Count, this->File);
	if(Result != Count){
		int ErrCode = errno;
		int Position = this->getPosition();
		::error("TReadBinaryFile::readBytes: Error reading %d bytes\n", Count);
		::error("# File: %s, position %d, return value: %d, error code: %d (%s)\n",
				this->Filename, Position, Result, ErrCode, strerror(ErrCode));

		// NOTE(fusion): Close file and make a backup, possibly for further inspection.
		if(fclose(this->File) != 0){
			::error("TReadBinaryFile::readBytes: Error %d closing file.\n", errno);
		}
		this->File = NULL;
		SaveFile(this->Filename);

		this->error("Error while reading bytes");
	}
}

bool TReadBinaryFile::eof(void){
	if(this->File == NULL){
		this->error("File not open for eof check");
	}

	return this->getSize() <= this->getPosition();
}

void TReadBinaryFile::skip(int Count){
	if(this->File == NULL){
		this->error("File not open for skip");
	}

	this->seek(this->getPosition() + Count);
}

TReadBinaryFile::~TReadBinaryFile(void){
	if(this->File != NULL){
		::error("TReadBinaryFile::~TReadBinaryFile: File %s is still open.\n", this->Filename);
		if(fclose(this->File) != 0){
			::error("TReadBinaryFile::~TReadBinaryFile: Error %d closing file.\n", errno);
		}
	}
}

// TWriteBinaryFile
//==============================================================================
TWriteBinaryFile::TWriteBinaryFile(void){
	this->File = NULL;
}

bool TWriteBinaryFile::open(const char *FileName){
	if(this->File != NULL){
		this->error("File still open");
	}

	this->File = fopen(FileName, "wb");
	if(this->File == NULL){
		int ErrCode = errno;
		::error("TWriteBinaryFile::open: Cannot create file %s.\n", FileName);
		::error("Error %d: %s.\n", ErrCode, strerror(ErrCode));
		return false;
	}

	strcpy(this->Filename, FileName);
	return true;
}

void TWriteBinaryFile::close(void){
	// TODO(fusion): Check if file is NULL?
	if(fclose(this->File) != 0){
		int ErrCode = errno;
		::error("TWriteBinaryFile::close: Error closing file.\n");
		::error("# File: %s, error code: %d (%s)\n",
				this->Filename, ErrCode, strerror(ErrCode));
	}
	this->File = NULL;
}

void TWriteBinaryFile::error(const char *Text){
	if(this->File != NULL){
		if(fclose(this->File) != 0){
			::error("TWriteBinaryFile::error: Error %d closing file.\n", errno);
		}
		this->File = NULL;
	}

	snprintf(ErrorString, sizeof(ErrorString),
			"error in binary-file \"%s\": %s.",
			this->Filename, Text);

	throw ErrorString;
}

void TWriteBinaryFile::writeByte(uint8 Byte){
	int Result = (int)fwrite(&Byte, 1, 1, this->File);
	if(Result != 1){
		int ErrCode = errno;
		::error("TWriteBinaryFile::writeByte: Error writing a byte\n");
		::error("# File: %s, return value: %d, error code: %d (%s)\n",
				this->Filename, Result, ErrCode, strerror(ErrCode));

		// NOTE(fusion): Close file and make a backup, possibly for further inspection.
		if(fclose(this->File) != 0){
			::error("TWriteBinaryFile::writeByte: Error %d closing file.\n", errno);
		}
		this->File = NULL;
		SaveFile(this->Filename);

		this->error("Error while writing byte");
	}
}

void TWriteBinaryFile::writeBytes(const uint8 *Buffer, int Count){
	int Result = (int)fwrite(Buffer, 1, Count, this->File);
	if(Result != Count){
		int ErrCode = errno;
		::error("TWriteBinaryFile::writeBytes: Error writing %d bytes\n", Count);
		::error("# File: %s, return value: %d, error code: %d (%s)\n",
				this->Filename, Result, ErrCode, strerror(ErrCode));

		// NOTE(fusion): Close file and make a backup, possibly for further inspection.
		if(fclose(this->File) != 0){
			::error("TWriteBinaryFile::writeBytes: Error %d closing file.\n", errno);
		}
		this->File = NULL;
		SaveFile(this->Filename);

		this->error("Error while writing bytes");
	}
}

TWriteBinaryFile::~TWriteBinaryFile(void){
	if(this->File != NULL){
		::error("TWriteBinaryFile::~TWriteBinaryFile: File %s is still open.\n", this->Filename);
		if(fclose(this->File) != 0){
			::error("TWriteBinaryFile::~TWriteBinaryFile: Error %d closing file.\n", errno);
		}
	}
}
