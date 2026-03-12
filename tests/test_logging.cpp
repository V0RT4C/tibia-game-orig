#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "logging/logging.h"

#include <cstring>

static char last_error[1024];
static char last_print[1024];
static int last_print_level;

static void TestErrorHandler(const char *Text) {
    strncpy(last_error, Text, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = 0;
}

static void TestPrintHandler(int Level, const char *Text) {
    last_print_level = Level;
    strncpy(last_print, Text, sizeof(last_print) - 1);
    last_print[sizeof(last_print) - 1] = 0;
}

TEST_CASE("error() calls pluggable handler") {
    last_error[0] = 0;
    SetErrorFunction(TestErrorHandler);
    error("test %d", 42);
    CHECK(strcmp(last_error, "test 42") == 0);
    SetErrorFunction(nullptr);
}

TEST_CASE("print() calls pluggable handler with level") {
    last_print[0] = 0;
    last_print_level = -1;
    SetPrintFunction(TestPrintHandler);
    print(3, "hello %s", "world");
    CHECK(strcmp(last_print, "hello world") == 0);
    CHECK(last_print_level == 3);
    SetPrintFunction(nullptr);
}

TEST_CASE("SilentHandler does not crash") {
    SilentHandler("test");
    SilentHandler(1, "test");
}

TEST_CASE("error() without handler does not crash") {
    SetErrorFunction(nullptr);
    error("no handler test");
}

TEST_CASE("print() without handler does not crash") {
    SetPrintFunction(nullptr);
    print(1, "no handler test");
}
