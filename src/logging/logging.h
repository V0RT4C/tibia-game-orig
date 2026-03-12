#ifndef GAMESERVER_LOGGING_H
#define GAMESERVER_LOGGING_H

#include "common/assert/assert.h"

typedef void ErrorFunction(const char *Text);
typedef void PrintFunction(int Level, const char *Text);

void SetErrorFunction(ErrorFunction *Function);
void SetPrintFunction(PrintFunction *Function);
void SetStandardLogFile(const char *FileName);
void SilentHandler(const char *Text);
void SilentHandler(int Level, const char *Text);
void LogFileHandler(const char *Text);
void LogFileHandler(int Level, const char *Text);
void error(const char *Text, ...) ATTR_PRINTF(1, 2);
void print(int Level, const char *Text, ...) ATTR_PRINTF(2, 3);

#endif // GAMESERVER_LOGGING_H
