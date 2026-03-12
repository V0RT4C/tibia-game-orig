#include "logging/logging.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cerrno>

static ErrorFunction *CurrentErrorFunction;
static PrintFunction *CurrentPrintFunction;
static char StandardLogFile[4096];

void SetErrorFunction(ErrorFunction *Function){
	CurrentErrorFunction = Function;
}

void SetPrintFunction(PrintFunction *Function){
	CurrentPrintFunction = Function;
}

void SetStandardLogFile(const char *FileName){
	strcpy(StandardLogFile, FileName);
}

void SilentHandler(const char *Text){
	// no-op
}

void SilentHandler(int Level, const char *Text){
	// no-op
}

void LogFileHandler(const char *Text){
	if(StandardLogFile[0] == 0){
		return;
	}

	FILE *File = fopen(StandardLogFile, "at");
	if(File != NULL){
		fprintf(File, "%s", Text);
		if(fclose(File) != 0){
			error("LogfileHandler: Error %d closing the file.\n", errno);
		}
	}
}

void LogFileHandler(int Level, const char *Text){
	LogFileHandler(Text);
}

void error(const char *Text, ...){
	char s[1024];

	va_list ap;
	va_start(ap, Text);
	vsnprintf(s, sizeof(s), Text, ap);
	va_end(ap);

	if(CurrentErrorFunction){
		CurrentErrorFunction(s);
	}else{
		printf("%s", s);
	}
}

void print(int Level, const char *Text, ...){
	char s[1024];

	va_list ap;
	va_start(ap, Text);
	vsnprintf(s, sizeof(s), Text, ap);
	va_end(ap);

	if(CurrentPrintFunction){
		CurrentPrintFunction(Level, s);
	}else{
		printf("%s", s);
	}
}
