#ifndef GAMESERVER_COMMON_STRINGS_H
#define GAMESERVER_COMMON_STRINGS_H

#include "common/types/types.h"

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
void ChangeWildcards(char *String);
char *Capitals(char *Text);

#endif // GAMESERVER_COMMON_STRINGS_H
