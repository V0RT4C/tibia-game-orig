#include "common/utils/utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

int random(int Min, int Max){
	int Range = (Max - Min) + 1;
	int Result = Min;
	if(Range > 0){
		Result += rand() % Range;
	}
	return Result;
}

bool FileExists(const char *FileName){
	struct stat Buffer;
	bool Result = true;
	if(stat(FileName, &Buffer) != 0){
		if(errno != ENOENT){
			error("FileExists: Unexpected error code %d.\n", errno);
		}
		Result = false;
	}
	return Result;
}

// String Utility
// =============================================================================
bool isSpace(int c){
	return c == ' '
		|| c == '\t'
		|| c == '\n'
		|| c == '\r'
		|| c == '\v'
		|| c == '\f';
}

bool isAlpha(int c){
	// TODO(fusion): This is most likely wrong! We're assuming a direct conversion
	// from `char` to `int` which will cause sign extension for negative values. This
	// wouldn't be a problem if we expected to parse only streams of `char[]` but can
	// be problematic for the output of `getc` which returns bytes as `unsigned char`
	// converted to `int`.
	//	TLDR: The parameter `c` should be `uint8`.
	return ('A' <= c && c <= 'Z')
		|| ('a' <= c && c <= 'z')
		|| c == -0x1C	// E4 => ä
		|| c == -0x0A	// F6 => ö
		|| c == -0x04	// FC => ü
		|| c == -0x3C	// C4 => Ä
		|| c == -0x2A	// D6 => Ö
		|| c == -0x24	// DC => Ü
		|| c == -0x21;	// DF => ß
}

bool isEngAlpha(int c){
	return ('A' <= c && c <= 'Z')
		|| ('a' <= c && c <= 'z');
}

bool isDigit(int c){
	return ('0' <= c && c <= '9');
}

int toLower(int c){
	// TODO(fusion): Same problem as `isAlpha`.
	if(('A' <= c && c <= 'Z') || (0xC0 <= c && c <= 0xDE && c != 0xD7)){
		c += 32;
	}
	return c;
}

int toUpper(int c){
	// TODO(fusion): Same problem as `isAlpha`.
	if(('a' <= c && c <= 'z') || (0xE0 <= c && c <= 0xFE && c != 0xF7)){
		c -= 32;
	}
	return c;
}

char *strLower(char *s){
	for(int i = 0; s[i] != 0; i += 1){
		s[i] = (char)toLower(s[i]);
	}
	return s;
}

char *strUpper(char *s){
	for(int i = 0; s[i] != 0; i += 1){
		s[i] = (char)toUpper(s[i]);
	}
	return s;
}

int stricmp(const char *s1, const char *s2, int Max /*= INT_MAX*/){
	for(int i = 0; i < Max; i += 1){
		int c1 = toLower(s1[i]);
		int c2 = toLower(s2[i]);
		if(c1 > c2){
			return 1;
		}else if(c1 < c2){
			return -1;
		}else{
			ASSERT(c1 == c2);
			if(c1 == 0){
				break;
			}
		}
	}
	return 0;
}

char *findFirst(char *s, char c){
	return strchr(s, (int)c);
}

char *findLast(char *s, char c){
	char *Current = s;
	char *Last = NULL;
	while(true){
		Current = strchr(Current, (int)c);
		if(Current == NULL)
			break;
		Last = Current;
		Current += 1; // skip character
	}
	return Last;
}

// BitSet Utility
// =============================================================================
bool CheckBitIndex(int BitSetBytes, int Index){
	return Index >= 0 && Index < (BitSetBytes * 8);
}

bool CheckBit(uint8 *BitSet, int Index){
	int ByteIndex = (int)(Index / 8);
	uint8 BitMask = (uint8)(1 << (Index % 8));
	return (BitSet[ByteIndex] & BitMask) != 0;
}

void SetBit(uint8 *BitSet, int Index){
	int ByteIndex = (int)(Index / 8);
	uint8 BitMask = (uint8)(1 << (Index % 8));
	BitSet[ByteIndex] |= BitMask;
}

void ClearBit(uint8 *BitSet, int Index){
	int ByteIndex = (int)(Index / 8);
	uint8 BitMask = (uint8)(1 << (Index % 8));
	BitSet[ByteIndex] &= ~BitMask;
}
