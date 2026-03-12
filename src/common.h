#ifndef TIBIA_COMMON_H_
#define TIBIA_COMMON_H_ 1

#include "common/types/types.h"
#include "common/assert/assert.h"
#include "common/utils/utils.h"

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>

// Constants
// =============================================================================
// TODO(fusion): There are many constants that are hardcoded as decompilation
// artifacts. We should define them here and use when appropriate. It is not
// as simple because `std::size` is used in some cases and they're used
// essentially everywhere.

//#define MAX_NAME 30 // used with most short strings (should replace MAX_IDENT_LENGTH too)
//#define MAX_IPADDRESS 16
//#define MAX_BUDDIES 100
//#define MAX_SKILLS 25
//#define MAX_SPELLS 256
//#define MAX_QUESTS 500
#define MAX_DEPOTS 9
//#define MAX_OPEN_CONTAINERS 16
#define MAX_SPELL_SYLLABLES 10

// shm.cc
// =============================================================================
void StartGame(void);
void CloseGame(void);
void EndGame(void);
bool LoginAllowed(void);
bool GameRunning(void);
bool GameStarting(void);
bool GameEnding(void);
pid_t GetGameProcessID(void);
pid_t GetGameThreadID(void);
int GetPrintlogPosition(void);
char *GetPrintlogLine(int Line);
void IncrementObjectCounter(void);
void DecrementObjectCounter(void);
uint32 GetObjectCounter(void);
void IncrementPlayersOnline(void);
void DecrementPlayersOnline(void);
int GetPlayersOnline(void);
void IncrementNewbiesOnline(void);
void DecrementNewbiesOnline(void);
int GetNewbiesOnline(void);
void SetRoundNr(uint32 RoundNr);
uint32 GetRoundNr(void);
void SetCommand(int Command, char *Text);
int GetCommand(void);
char *GetCommandBuffer(void);
void InitSHM(bool Verbose);
void ExitSHM(void);
void InitSHMExtern(bool Verbose);
void ExitSHMExtern(void);

// strings.cc
// =============================================================================
const char *AddStaticString(const char *String);
uint32 AddDynamicString(const char *String);
const char *GetDynamicString(uint32 Number);
void DeleteDynamicString(uint32 Number);
void CleanupDynamicStrings(void);
void InitStrings(void);
void ExitStrings(void);

bool IsCountable(const char *s);
const char *Plural(const char *s, int Count);
const char *SearchForWord(const char *Pattern, const char *Text);
const char *SearchForNumber(int Count, const char *Text);
bool MatchString(const char *Pattern, const char *String);
void AddSlashes(char *Destination, const char *Source);
void Trim(char *Text);
void Trim(char *Destination, const char *Source);
char *Capitals(char *Text);

// time.cc
// =============================================================================
extern uint32 RoundNr;
extern uint32 ServerMilliseconds;
struct tm GetLocalTimeTM(time_t t);
int64 GetClockMonotonicMS(void);
void GetRealTime(int *Hour, int *Minute);
void GetTime(int *Hour, int *Minute);
void GetDate(int *Year, int *Cycle, int *Day);
void GetAmbiente(int *Brightness, int *Color);
uint32 GetRoundAtTime(int Hour, int Minute);
uint32 GetRoundForNextMinute(void);

// utils.cc
// =============================================================================
typedef void TErrorFunction(const char *Text);
typedef void TPrintFunction(int Level, const char *Text);
void SetErrorFunction(TErrorFunction *Function);
void SetPrintFunction(TPrintFunction *Function);
void SilentHandler(const char *Text);
void SilentHandler(int Level, const char *Text);
void LogFileHandler(const char *Text);
void LogFileHandler(int Level, const char *Text);
void error(const char *Text, ...) ATTR_PRINTF(1, 2);
void print(int Level, const char *Text, ...) ATTR_PRINTF(2, 3);
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

struct TReadStream {
	// VIRTUAL FUNCTIONS
	// =================
	virtual bool readFlag(void);														// VTABLE[0]
	virtual uint8 readByte(void) = 0;													// VTABLE[1]
	virtual uint16 readWord(void);														// VTABLE[2]
	virtual uint32 readQuad(void);														// VTABLE[3]
	virtual void readString(char *Buffer, int MaxLength);								// VTABLE[4]
	virtual void readBytes(uint8 *Buffer, int Count);									// VTABLE[5]
	virtual bool eof(void) = 0;															// VTABLE[6]
	virtual void skip(int Count) = 0;													// VTABLE[7]
};

struct TReadBuffer: TReadStream {
	TReadBuffer(const uint8 *Data, int Size);

	// VIRTUAL FUNCTIONS
	// =================
	uint8 readByte(void) override;
	uint16 readWord(void) override;
	uint32 readQuad(void) override;
	void readBytes(uint8 *Buffer, int Count) override;
	bool eof(void) override;
	void skip(int Count) override;

	// DATA
	// =================
	const uint8 *Data;
	int Size;
	int Position;
};

struct TWriteStream {
	// VIRTUAL FUNCTIONS
	// =================
	virtual void writeFlag(bool Flag);													// VTABLE[0]
	virtual void writeByte(uint8 Byte) = 0;												// VTABLE[1]
	virtual void writeWord(uint16 Word);												// VTABLE[2]
	virtual void writeQuad(uint32 Quad);												// VTABLE[3]
	virtual void writeString(const char *String);										// VTABLE[4]
	virtual void writeBytes(const uint8 *Buffer, int Count);							// VTABLE[5]
};

struct TWriteBuffer: TWriteStream {
	TWriteBuffer(uint8 *Data, int Size);

	// VIRTUAL FUNCTIONS
	// =================
	void writeByte(uint8 Byte) override;
	void writeWord(uint16 Word) override;
	void writeQuad(uint32 Quad) override;
	void writeBytes(const uint8 *Buffer, int Count) override;

	// DATA
	// =================
	uint8 *Data;
	int Size;
	int Position;
};

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

#endif //TIBIA_COMMON_H_
