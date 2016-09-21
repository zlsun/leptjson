
#include "leptjson.h"
#include "test.h"

TEST(simple, null) {
    lept_value v;
    v.type = LEPT_UNKNOWN;
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "null"));
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));
    lept_free_value_on_stack(&v);
}

TEST(simple, true) {
    lept_value v;
    v.type = LEPT_UNKNOWN;
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "true"));
    EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(&v));
    lept_free_value_on_stack(&v);
}

TEST(simple, false) {
    lept_value v;
    v.type = LEPT_UNKNOWN;
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "false"));
    EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(&v));
    lept_free_value_on_stack(&v);
}

#define TEST_NUMBER(json, expect)                           \
    do {                                                    \
        lept_value v;                                       \
        v.type = LEPT_UNKNOWN;                              \
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json)); \
        EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(&v));      \
        EXPECT_EQ_DOUBLE(expect, lept_get_number(&v));      \
        lept_free_value_on_stack(&v);                       \
    } while (0)

TEST(simple, number) {
    TEST_NUMBER("0", 0.0);
    TEST_NUMBER("-0", 0.0);
    TEST_NUMBER("-0.0", 0.0);
    TEST_NUMBER("1", 1.0);
    TEST_NUMBER("-1", -1.0);
    TEST_NUMBER("1.5", 1.5);
    TEST_NUMBER("-1.5", -1.5);
    TEST_NUMBER("3.1416", 3.1416);
    TEST_NUMBER("1E10", 1E10);
    TEST_NUMBER("1e10", 1e10);
    TEST_NUMBER("1E+10", 1E+10);
    TEST_NUMBER("1E-10", 1E-10);
    TEST_NUMBER("-1E10", -1E10);
    TEST_NUMBER("-1e10", -1e10);
    TEST_NUMBER("-1E+10", -1E+10);
    TEST_NUMBER("-1E-10", -1E-10);
    TEST_NUMBER("1.234E+10", 1.234E+10);
    TEST_NUMBER("1.234E-10", 1.234E-10);
    TEST_NUMBER("1e-10000", 0.0); /* must underflow */

    TEST_NUMBER("1.0000000000000002"       ,  1.0000000000000002); /* the smallest number > 1 */
    TEST_NUMBER("4.9406564584124654e-324"  ,  4.9406564584124654e-324); /* minimum denormal */
    TEST_NUMBER("-4.9406564584124654e-324" , -4.9406564584124654e-324);
    TEST_NUMBER("2.2250738585072009e-308"  ,  2.2250738585072009e-308);  /* Max subnormal double */
    TEST_NUMBER("-2.2250738585072009e-308" , -2.2250738585072009e-308);
    TEST_NUMBER("2.2250738585072014e-308"  ,  2.2250738585072014e-308);  /* Min normal positive double */
    TEST_NUMBER("-2.2250738585072014e-308" , -2.2250738585072014e-308);
    TEST_NUMBER("1.7976931348623157e+308"  ,  1.7976931348623157e+308);  /* Max double */
    TEST_NUMBER("-1.7976931348623157e+308" , -1.7976931348623157e+308);
}

#define TEST_STRING(json, expect)                           \
    do {                                                    \
        lept_value v;                                       \
        lept_string* s;                                     \
        v.type = LEPT_UNKNOWN;                              \
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json)); \
        EXPECT_EQ_INT(LEPT_STRING, lept_get_type(&v));      \
        s = lept_get_string(&v);                            \
        EXPECT_EQ_ULONG(strlen(expect), s->len);            \
        EXPECT_EQ_STRING(expect, s->str);                   \
        lept_free_value_on_stack(&v);                       \
    } while (0)

TEST(simple, string) {
    TEST_STRING("\"\"", "");
    TEST_STRING("\"abc 0-9\"", "abc 0-9");
    TEST_STRING("\"'+-*/@\"", "'+-*/@");
    TEST_STRING("\"\\b\"", "\b");
    TEST_STRING("\"\\f\"", "\f");
    TEST_STRING("\"\\n\"", "\n");
    TEST_STRING("\"\\r\"", "\r");
    TEST_STRING("\"\\t\"", "\t");
    TEST_STRING("\"\\\"\"", "\"");
}

#define TEST_ARRAY(json, ...)                                   \
    do {                                                        \
        double array[100] = __VA_ARGS__;                        \
        double* p = array;                                      \
        lept_value v;                                           \
        lept_array* a;                                          \
        lept_array_item* i;                                     \
        v.type = LEPT_UNKNOWN;                                  \
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json));     \
        EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(&v));           \
        a = lept_get_array(&v);                                 \
        for (i = a->items; i != NULL; i = i->next) {            \
            EXPECT_EQ_DOUBLE(lept_get_number(i->value), *p++);  \
        }                                                       \
        lept_free_value_on_stack(&v);                           \
    } while (0)

TEST(simple, array) {
    TEST_ARRAY("[]", {0});
    TEST_ARRAY("[0]", {0});
    TEST_ARRAY("[0,1]", {0, 1});
    TEST_ARRAY("[ 0, 1, 2 ]", {0, 1, 2});
}

#define TEST_OBJECT(json, keys, ...)                            \
    do {                                                        \
        const char* k = keys;                                   \
        double values[100] = __VA_ARGS__;                       \
        double* p = values;                                     \
        lept_value v;                                           \
        lept_object* o;                                         \
        lept_object_node* n;                                    \
        v.type = LEPT_UNKNOWN;                                  \
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json));     \
        EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(&v));          \
        o = lept_get_object(&v);                                \
        for (n = o->nodes; n != NULL; n = n->next) {            \
            EXPECT_EQ_CHAR(n->key->str[0], *k++);               \
            EXPECT_EQ_DOUBLE(lept_get_number(n->value), *p++);  \
        }                                                       \
        lept_free_value_on_stack(&v);                           \
    } while (0)

TEST(simple, object) {
    TEST_OBJECT("{}", "", {0});
    TEST_OBJECT("{\"a\":0}", "a", {0});
    TEST_OBJECT("{\"a\":0,\"b\":1}", "ab", {0,1});
}

#define TEST_ERROR(error, json)                         \
    do {                                                \
        lept_value v;                                   \
        v.type = LEPT_UNKNOWN;                          \
        EXPECT_EQ_INT(error, lept_parse(&v, json));     \
        lept_free_value_on_stack(&v);                   \
    } while (0)

TEST(error, expect_value) {
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, " ");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "]");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "}");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "[");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "[1,");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "{\"a\":");
}

TEST(error, invalid_value) {
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "?");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "{1");
    /* invalid number */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "+0");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "+1");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, ".123"); /* at least one digit before '.' */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "1.");   /* at least one digit after '.' */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "INF");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "inf");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "NAN");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "nan");
}

TEST(error, unclosed_brackets) {
    TEST_ERROR(LEPT_PARSE_UNCLOSED_BRACKETS, "[1");
    TEST_ERROR(LEPT_PARSE_UNCLOSED_BRACKETS, "[1}");
    TEST_ERROR(LEPT_PARSE_UNCLOSED_BRACKETS, "[1,2");
    TEST_ERROR(LEPT_PARSE_UNCLOSED_BRACKETS, "[1,2}");
}

TEST(error, unclosed_quotes) {
    TEST_ERROR(LEPT_PARSE_UNCLOSED_QUOTES, "\"");
    TEST_ERROR(LEPT_PARSE_UNCLOSED_QUOTES, "\"1");
    TEST_ERROR(LEPT_PARSE_UNCLOSED_QUOTES, "\"1\\\"");
}

TEST(error, root_not_singular) {
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "null x");
    /* invalid number */
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0123"); /* after zero should be '.' or nothing */
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0x0");
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0x123");
}

TEST(error, number_too_big) {
    TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, "1e309");
    /* TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, "-1e309"); */
}

#define EXPECT_OBJECT(length)                                   \
    EXPECT_EQ_INT(LEPT_OBJECT, lept_get_type(v));               \
    o = lept_get_object(v);                                     \
    EXPECT_EQ_ULONG(length, o->len);                            \
    n = o->nodes;
#define EXPECT_ARRAY(length)                                    \
    EXPECT_EQ_INT(LEPT_ARRAY, lept_get_type(v));                \
    a = lept_get_array(v);                                      \
    EXPECT_EQ_ULONG(length, o->len);                            \
    i = a->items;
#define EXPECT_KEY(keystr)                                      \
    EXPECT_EQ_STRING(keystr, n->key->str);
#define EXPECT_VALUE_TRUE()                                     \
    EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(n->value));          \
    n = n->next;
#define EXPECT_VALUE_FALSE()                                    \
    EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(n->value));         \
    n = n->next;
#define EXPECT_VALUE_NULL()                                     \
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(n->value));          \
    n = n->next;
#define EXPECT_VALUE_NUMBER(number)                             \
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(n->value));        \
    EXPECT_EQ_DOUBLE(number, lept_get_number(n->value));        \
    n = n->next;
#define EXPECT_VALUE_STRING(string)                             \
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(n->value));        \
    EXPECT_EQ_STRING(string, lept_get_string(n->value)->str);   \
    n = n->next;
#define EXPECT_VALUE_OBJECT(length)                             \
    v = n->value;                                               \
    EXPECT_OBJECT(length);
#define EXPECT_VALUE_ARRAY(length)                              \
    v = n->value;                                               \
    EXPECT_ARRAY(length);
#define EXPECT_ITEM_STRING(string)                              \
    EXPECT_EQ_INT(LEPT_STRING, lept_get_type(i->value));        \
    EXPECT_EQ_STRING(string, lept_get_string(i->value)->str);   \
    i = i->next;

TEST(complex, mix) {
    const char json[] = "{"
    "  \"show\": true,"
    "  \"log\": false,"
    "  \"time\": -12345.67890,"
    "  \"command\": \"ls\","
    "  \"argument\": {"
    "    \"argc\": 1,"
    "    \"argv\": ["
    "      \"ls\","
    "      \"-al\","
    "    ]"
    "  }"
    "}";
    lept_value* value = NEW(lept_value);
    lept_value* v = value;
    lept_object* o;
    lept_object_node* n;
    lept_array* a;
    lept_array_item* i;

    v->type = LEPT_UNKNOWN;

    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(v, json));

    EXPECT_OBJECT(5ul);
    EXPECT_KEY("show"); EXPECT_VALUE_TRUE();
    EXPECT_KEY("log"); EXPECT_VALUE_FALSE();
    EXPECT_KEY("time"); EXPECT_VALUE_NUMBER(-12345.67890);
    EXPECT_KEY("command"); EXPECT_VALUE_STRING("ls");
    EXPECT_KEY("argument"); EXPECT_VALUE_OBJECT(2ul);
    {
        EXPECT_KEY("argc"); EXPECT_VALUE_NUMBER(1.0);
        EXPECT_KEY("argv"); EXPECT_VALUE_ARRAY(2ul);
        {
            EXPECT_ITEM_STRING("ls");
            EXPECT_ITEM_STRING("-al");
        }
    }

    lept_free_value(value);
}

#define TEST_FILE(expect, file)                             \
    do {                                                    \
        lept_value v;                                       \
        v.type = LEPT_UNKNOWN;                              \
        EXPECT_EQ_INT(expect, lept_parse_file(&v, file));   \
        lept_free_value_on_stack(&v);                       \
    } while (0)

TEST(file, ok) {
    TEST_FILE(LEPT_PARSE_OK, "test/good/1.json");
    TEST_FILE(LEPT_PARSE_OK, "test/good/2.json");
}

MAIN_BEG
    SUITE_BEG(simple)
        RUN_TEST(simple, null)
        RUN_TEST(simple, true)
        RUN_TEST(simple, false)
        RUN_TEST(simple, number)
        RUN_TEST(simple, string)
        RUN_TEST(simple, array)
        RUN_TEST(simple, object)
    SUITE_END(simple)
    SUITE_BEG(error)
        RUN_TEST(error, expect_value)
        RUN_TEST(error, invalid_value)
        RUN_TEST(error, unclosed_brackets)
        RUN_TEST(error, unclosed_quotes)
        RUN_TEST(error, root_not_singular)
        RUN_TEST(error, number_too_big)
    SUITE_END(error)
    SUITE_BEG(complex)
        RUN_TEST(complex, mix)
    SUITE_END(complex)
    SUITE_BEG(file)
        RUN_TEST(file, ok)
    SUITE_END(file)
MAIN_END
