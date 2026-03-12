#ifndef GAMESERVER_TIME_UTILS_H
#define GAMESERVER_TIME_UTILS_H

#include "common/types/types.h"
#include <ctime>

// NOTE: Filename is time_utils.h (not time.h) to avoid conflict with
// system <time.h>. This deviates from the spec's target directory
// structure which shows time/time.h — the spec will be updated.

// IMPORTANT(fusion): `RoundNr` is just the number of seconds since startup which
// is why `GetRoundAtTime` and `GetRoundForNextMinute` straight up uses it as
// seconds. It is incremented every 1 second inside `AdvanceGame`.
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

#endif // GAMESERVER_TIME_UTILS_H
