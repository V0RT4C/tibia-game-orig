#ifndef TIBIA_COMMON_H_
#define TIBIA_COMMON_H_ 1

#include "common/types/types.h"
#include "common/assert/assert.h"
#include "logging/logging.h"
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

// Stream classes (extracted to common/ modules)
#include "common/read_stream/read_stream.h"
#include "common/write_stream/write_stream.h"
#include "common/dynamic_buffer/dynamic_buffer.h"

#endif //TIBIA_COMMON_H_
