#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/strings/strings.h"
#include <cstring>

TEST_CASE("AddStaticString returns stable pointer") {
    InitStrings();
    const char *p1 = AddStaticString("hello");
    const char *p2 = AddStaticString("world");
    CHECK(strcmp(p1, "hello") == 0);
    CHECK(strcmp(p2, "world") == 0);
    CHECK(p1 != p2);
}

TEST_CASE("AddStaticString empty string returns empty") {
    const char *p = AddStaticString("");
    CHECK(strcmp(p, "") == 0);
}

TEST_CASE("Plural with count 1 returns original") {
    CHECK(strcmp(Plural("a sword", 1), "a sword") == 0);
}

TEST_CASE("Plural with count > 1 pluralizes") {
    const char *result = Plural("a sword", 3);
    CHECK(strcmp(result, "3 swords") == 0);
}

TEST_CASE("MatchString exact match") {
    CHECK(MatchString("hello", "hello"));
    CHECK_FALSE(MatchString("hello", "world"));
}

TEST_CASE("MatchString wildcard") {
    CHECK(MatchString("h*o", "hello"));
    CHECK(MatchString("?ello", "hello"));
}

TEST_CASE("Trim removes whitespace") {
    char buf[] = "  hello  ";
    Trim(buf);
    CHECK(strcmp(buf, "hello") == 0);
}

TEST_CASE("Capitals capitalizes words") {
    char buf[] = "hello world";
    Capitals(buf);
    CHECK(strcmp(buf, "Hello World") == 0);
}

TEST_CASE("AddSlashes escapes special chars") {
    char dest[64];
    AddSlashes(dest, "it's a \"test\"");
    CHECK(strcmp(dest, "it\\'s a \\\"test\\\"") == 0);
}

TEST_CASE("SearchForWord finds word") {
    CHECK(SearchForWord("world", "hello world") != nullptr);
    CHECK(SearchForWord("xyz", "hello world") == nullptr);
}
