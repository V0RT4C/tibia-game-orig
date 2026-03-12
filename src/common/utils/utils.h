#ifndef GAMESERVER_COMMON_UTILS_H
#define GAMESERVER_COMMON_UTILS_H

#include "common/types/types.h"
#include "common/assert/assert.h"
#include "logging/logging.h"

#include <algorithm>
#include <climits>

int random(int Min, int Max);
bool FileExists(const char *FileName);

bool isSpace(int c);
bool isAlpha(int c);
bool isEngAlpha(int c);
bool isDigit(int c);
int toLower(int c);
int toUpper(int c);
char *strLower(char *s);
char *strUpper(char *s);
int stricmp(const char *s1, const char *s2, int Max = INT_MAX);
char *findFirst(char *s, char c);
char *findLast(char *s, char c);

bool CheckBitIndex(int BitSetBytes, int Index);
bool CheckBit(uint8 *BitSet, int Index);
void SetBit(uint8 *BitSet, int Index);
void ClearBit(uint8 *BitSet, int Index);

template<typename T>
void RandomShuffle(T *Buffer, int Size){
	if(Buffer == NULL){
		error("RandomShuffle: Buffer is NULL.\n");
		return;
	}

	int Max = Size - 1;
	for(int Min = 0; Min < Max; Min += 1){
		int Swap = random(Min, Max);
		if(Swap != Min){
			std::swap(Buffer[Min], Buffer[Swap]);
		}
	}
}

#endif // GAMESERVER_COMMON_UTILS_H
