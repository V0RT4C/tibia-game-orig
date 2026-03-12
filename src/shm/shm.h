#ifndef GAMESERVER_SHM_H
#define GAMESERVER_SHM_H 1

#include "common/types/types.h"

#include <sys/types.h>

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

#endif // GAMESERVER_SHM_H
