#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int suite_count = 0;
static int suite_pass = 0;
static int test_count = 0;
static int test_pass = 0;
static int expect_count = 0;
static int expect_pass = 0;

#define COLOR_RED    "\033[0;31m"
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_RESET  "\033[0m"

#define SET_COLOR(c) printf("%s", COLOR_##c)
#define RESET_COLOR() SET_COLOR(RESET)

#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do { \
        expect_count++; \
        if (equality) { \
            expect_pass++; \
        } else { \
            fprintf(stderr, \
                    COLOR_RED "%s:%d:" " expect %s=" format " actual %s=" format COLOR_RESET "\n", \
                    __FILE__, __LINE__, #expect, expect,     #actual, actual); \
        } \
    } while (0)

#define EXPECT_EQ_CHAR(expect, actual) \
    EXPECT_EQ_BASE((expect) == (actual), expect, actual, "'%c'")

#define EXPECT_EQ_INT(expect, actual) \
    EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")

#define EXPECT_EQ_UINT(expect, actual) \
    EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%u")

#define EXPECT_EQ_LONG(expect, actual) \
    EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%ld")

#define EXPECT_EQ_ULONG(expect, actual) \
    EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%lu")

#define EXPECT_EQ_DOUBLE(expect, actual) \
    EXPECT_EQ_BASE(abs((expect) - (actual)) < 1e-18, expect, actual, "%.17g")

#define EXPECT_EQ_STRING(expect, actual) \
    EXPECT_EQ_BASE(strcmp(expect, actual) == 0, expect, actual, "\"%s\"")

#define TEST(sname, tname) static void sname##_##tname()

#define RUN_TEST(sname, tname) \
    ++test_count; \
    expect_count = 0; \
    expect_pass = 0; \
    sname##_##tname(); \
    if (expect_pass == expect_count) { \
        ++test_pass; \
        SET_COLOR(GREEN); \
    } else SET_COLOR(RED); \
    printf("%s: ", #tname); \
    RESET_COLOR(); \
    printf("%d/%d ", expect_pass, expect_count); \
    printf("(%3.2f%%)\n", expect_pass * 100.0 / expect_count);

#define SUITE_BEG(sname) \
    ++suite_count; \
    test_count = 0; \
    test_pass = 0; \
    SET_COLOR(YELLOW); \
    printf("[%s]\n", #sname); \
    RESET_COLOR();

#define SUITE_END(sname) \
    if (test_pass == test_count) { \
        ++suite_pass; \
    } \
    printf("\n");

#define MAIN_BEG int main() {

#define MAIN_END \
        if (suite_pass == suite_count) SET_COLOR(GREEN); \
        else if (suite_pass == 0) SET_COLOR(RED); \
        else SET_COLOR(YELLOW); \
        printf("%d/%d ", suite_pass, suite_count); \
        printf("(%3.2f%%)\n", suite_pass * 100.0 / suite_count); \
        RESET_COLOR(); \
        return suite_pass == suite_count ? 0 : 1; \
    }
