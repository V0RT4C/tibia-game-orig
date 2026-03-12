#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/utils/utils.h"

TEST_CASE("random returns value in range") {
    for (int i = 0; i < 100; i++) {
        int val = random(5, 10);
        CHECK(val >= 5);
        CHECK(val <= 10);
    }
}

TEST_CASE("random with equal min/max returns that value") {
    CHECK(random(7, 7) == 7);
}

TEST_CASE("isSpace identifies whitespace") {
    CHECK(isSpace(' '));
    CHECK(isSpace('\t'));
    CHECK(isSpace('\n'));
    CHECK(isSpace('\r'));
    CHECK_FALSE(isSpace('A'));
    CHECK_FALSE(isSpace('0'));
}

TEST_CASE("isAlpha identifies Latin-1 letters") {
    CHECK(isAlpha('A'));
    CHECK(isAlpha('z'));
    CHECK_FALSE(isAlpha('0'));
    CHECK_FALSE(isAlpha(' '));
}

TEST_CASE("isEngAlpha identifies English letters only") {
    CHECK(isEngAlpha('A'));
    CHECK(isEngAlpha('z'));
    CHECK_FALSE(isEngAlpha('0'));
}

TEST_CASE("isDigit identifies digits") {
    CHECK(isDigit('0'));
    CHECK(isDigit('9'));
    CHECK_FALSE(isDigit('A'));
}

TEST_CASE("toLower/toUpper") {
    CHECK(toLower('A') == 'a');
    CHECK(toLower('z') == 'z');
    CHECK(toUpper('a') == 'A');
    CHECK(toUpper('Z') == 'Z');
}

TEST_CASE("stricmp case-insensitive comparison") {
    CHECK(stricmp("hello", "HELLO") == 0);
    CHECK(stricmp("abc", "abd") < 0);
    CHECK(stricmp("abd", "abc") > 0);
    CHECK(stricmp("hello world", "hello", 5) == 0);
}

TEST_CASE("CheckBit/SetBit/ClearBit") {
    uint8 bits[2] = {0, 0};
    CHECK_FALSE(CheckBit(bits, 0));
    SetBit(bits, 0);
    CHECK(CheckBit(bits, 0));
    SetBit(bits, 15);
    CHECK(CheckBit(bits, 15));
    ClearBit(bits, 0);
    CHECK_FALSE(CheckBit(bits, 0));
    CHECK(CheckBit(bits, 15));
}

TEST_CASE("CheckBitIndex boundary") {
    CHECK(CheckBitIndex(2, 0));
    CHECK(CheckBitIndex(2, 15));
    CHECK_FALSE(CheckBitIndex(2, 16));
    CHECK_FALSE(CheckBitIndex(2, -1));
}

TEST_CASE("findFirst/findLast") {
    char str[] = "hello world hello";
    CHECK(findFirst(str, 'o') == &str[4]);
    CHECK(findLast(str, 'o') == &str[16]);
    CHECK(findFirst(str, 'z') == nullptr);
    CHECK(findLast(str, 'z') == nullptr);
}

// Stub for error() — used by FileExists
#include <cstdio>
#include <cstdarg>
void error(const char *Text, ...) {
    va_list ap;
    va_start(ap, Text);
    vfprintf(stderr, Text, ap);
    va_end(ap);
}
