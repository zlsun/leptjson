#pragma once

#include <stddef.h> /* size_t */

typedef enum {
    LEPT_UNKNOWN,
    LEPT_NULL,
    LEPT_FALSE,
    LEPT_TRUE,
    LEPT_NUMBER,
    LEPT_STRING,
    LEPT_ARRAY,
    LEPT_OBJECT
} lept_type;

#define STRUCT(name) struct name##_s
#define DECLARE_STRUCT(name)        \
    struct name##_s;                \
    typedef struct name##_s name;

DECLARE_STRUCT(lept_string)
DECLARE_STRUCT(lept_value)
DECLARE_STRUCT(lept_array_item)
DECLARE_STRUCT(lept_array)
DECLARE_STRUCT(lept_object_node)
DECLARE_STRUCT(lept_object)

STRUCT(lept_value) {
    lept_type type;
    union {
        double n;
        lept_string* s;
        lept_array* a;
        lept_object* o;
    } value;
};

STRUCT(lept_string) {
    size_t len;
    char* str;
};

STRUCT(lept_array_item) {
    lept_array_item* next;
    lept_value* value;
};

STRUCT(lept_array) {
    size_t len;
    lept_array_item* items;
};

STRUCT(lept_object_node) {
    lept_object_node* next;
    lept_string* key;
    lept_value* value;
};

STRUCT(lept_object) {
    size_t len;
    lept_object_node* nodes;
};

#undef DECLARE_STRUCT
#undef STRUCT

enum {
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE,
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_UNCLOSED_QUOTES,
    LEPT_PARSE_UNCLOSED_BRACKETS,
    LEPT_PARSE_ROOT_NOT_SINGULAR,
    LEPT_PARSE_NUMBER_TOO_BIG,
    LEPT_FILE_CANNOT_OPEN,
    LEPT_FILE_READ_ERROR,
};

int lept_parse(lept_value* v, const char* json);
int lept_parse_file(lept_value* v, const char* path);

lept_string* lept_new_string();
lept_array_item* lept_new_array_item();
lept_array* lept_new_array();
lept_object_node* lept_new_object_node();
lept_object* lept_new_object();
lept_value* lept_new_value();

void lept_free_string(lept_string* s);
void lept_free_array_item(lept_array_item* i);
void lept_free_array(lept_array* a);
void lept_free_object_node(lept_object_node* n);
void lept_free_object(lept_object* o);
void lept_free_value(lept_value* v);
void lept_free_value_on_stack(lept_value* v);

lept_type lept_get_type(const lept_value* v);

double lept_get_number(const lept_value* v);
lept_string* lept_get_string(const lept_value* v);
lept_array* lept_get_array(const lept_value* v);
lept_object* lept_get_object(const lept_value* v);
