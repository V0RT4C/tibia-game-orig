#ifndef TIBIA_SCRIPT_H_
#define TIBIA_SCRIPT_H_ 1

#include "common.h"
#include "common/binary_file/binary_file.h"
#include "enums.h"

#define MAX_IDENT_LENGTH 30

struct ReadScriptFile {
	ReadScriptFile(void);
	~ReadScriptFile(void);
	void open(const char *FileName);
	void close(void);
	void error(const char *Text);
	void next_token(void);
	char *get_identifier(void);
	int get_number(void);
	char *get_string(void);
	uint8 *get_bytesequence(void);
	void get_coordinate(int *x, int *y, int *z);
	char get_special(void);

	char *read_identifier(void) {
		this->next_token();
		return this->get_identifier();
	}

	int read_number(void) {
		this->next_token();

		int Sign = 1;
		if (this->Token == SPECIAL && this->Special == '-') {
			Sign = -1;
			this->next_token();
		}

		return Sign * this->get_number();
	}

	char *read_string(void) {
		this->next_token();
		return this->get_string();
	}

	uint8 *read_bytesequence(void) {
		this->next_token();
		return this->get_bytesequence();
	}

	void read_coordinate(int *x, int *y, int *z) {
		this->next_token();
		this->get_coordinate(x, y, z);
	}

	char read_special(void) {
		this->next_token();
		return this->get_special();
	}

	void read_symbol(char Symbol) {
		if (this->read_special() != Symbol) {
			this->error("symbol mismatch");
		}
	}

	// DATA
	// =================
	TOKEN Token;
	FILE *File[3];
	char Filename[3][4096];
	int Line[3];
	char String[4000];
	int RecursionDepth;
	uint8 *Bytes;
	int Number;
	int CoordX;
	int CoordY;
	int CoordZ;
	char Special;
};

struct WriteScriptFile {
	WriteScriptFile(void);
	~WriteScriptFile(void);
	void open(const char *FileName);
	void close(void);
	void error(const char *Text);
	void write_ln(void);
	void write_text(const char *Text);
	void write_number(int Number);
	void write_string(const char *Text);
	void write_coordinate(int x, int y, int z);
	void write_bytesequence(const uint8 *Sequence, int Length);

	// DATA
	// =================
	FILE *File;
	char Filename[4096];
	int Line;
};

#endif // TIBIA_SCRIPT_H_
