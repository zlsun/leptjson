#include "leptjson.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define CUR() (*c->json)
#define NEXT() do { ++c->json; } while (0)
#define IS(ch) (CUR() == (ch))
#define C2I(ch) \
    ((ch) > 'a' ? (ch) - 'a' + 10 : \
     (ch) > 'A' ? (ch) - 'A' + 10 : \
     (ch) - '0')

#define ISDIGIT(ch)     ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')

#define EXPECT(ch) \
    do { \
        assert(CUR() == (ch)); \
        NEXT(); \
    } while (0)

typedef struct lept_context_s {
    const char* json;
} lept_context;

static int _parse_whitespace(lept_context* c) {
    while (IS(' ') || IS('\t') || IS('\r') || IS('\n')) {
        NEXT();
    }
    return LEPT_PARSE_OK;
}

static int _parse_literal(lept_context* c, lept_value* v, const char* literal, int type) {
    int len = strlen(literal);
    if (strncmp(c->json, literal, len) != 0) {
        return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += len;
    v->type = type;
    return LEPT_PARSE_OK;
}

enum {
    BEGIN = 0,
    INTEGER, IN_INTEGER,
    FRACTION, IN_FRACTION,
    EXPONENT, IN_EXPONENT,
    END, INVALID
};

#define STATE_MACHINE(begin) \
    for (int __state = begin;;) \
        __state_switch: \
        switch (__state)
#define STATE(st) break; case st:
#define DEFAULT() default:
#define GOTO(st) \
    do { \
        __state = st; \
        goto __state_switch; \
    } while (0)

static const char* _validate_number(const char* str) {
    const char* p = str;
    STATE_MACHINE(BEGIN) {
        DEFAULT() {
            assert(0); /* should never reach */
        }
        STATE(BEGIN) {
            if (*p == '-') ++p;
            if (!ISDIGIT(*p)) GOTO(INVALID);
            else GOTO(INTEGER);
        }
        STATE(INTEGER) {
            if (*p == '0') {
                ++p;
                GOTO(FRACTION);
            } else if (ISDIGIT1TO9(*p)) {
                ++p;
                GOTO(IN_INTEGER);
            } else {
                GOTO(INVALID);
            }
        }
        STATE(IN_INTEGER) {
            while (ISDIGIT(*p)) {
                ++p;
            }
            GOTO(FRACTION);
        }
        STATE(FRACTION) {
            if (*p == '.') {
                ++p;
                GOTO(IN_FRACTION);
            } else {
                GOTO(EXPONENT);
            }
        }
        STATE(IN_FRACTION) {
            if (!ISDIGIT(*p)) {
                GOTO(INVALID);
            } else {
                do {
                    ++p;
                } while (ISDIGIT(*p));
                GOTO(EXPONENT);
            }
        }
        STATE(EXPONENT) {
            if (*p != 'e' && *p != 'E') {
                GOTO(END);
            } else {
                ++p;
                if (*p == '+' || *p == '-') {
                    ++p;
                }
                GOTO(IN_EXPONENT);
            }
        }
        STATE(IN_EXPONENT) {
            if (!ISDIGIT(*p)) {
                GOTO(INVALID);
            } else {
                do {
                    ++p;
                } while (ISDIGIT(*p));
                GOTO(END);
            }
        }
        STATE(END) {
            return p;
        }
        STATE(INVALID) {
            return str;
        }
    }
}

static int _parse_number(lept_context* c, lept_value* v) {
    const char* vend;
    char* end;
    char* tmp;
    int len;

    if ((vend = _validate_number(c->json)) == c->json) {
        return LEPT_PARSE_INVALID_VALUE;
    }

    len = vend - c->json;
    tmp = malloc((len + 1) * sizeof(char));
    memcpy(tmp, c->json, len);
    tmp[len] = '\0';

    v->value.n = strtod(tmp, &end);
    assert(end != tmp); /* should be valid */
    assert(*end == '\0'); /* should process to end of tmp */

    free(tmp);

    if (v->value.n == HUGE_VAL || v->value.n == -HUGE_VAL) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }

    c->json = vend;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int _parse_str(lept_context* c, lept_string* str) {
    char* buffer = malloc(1024 * sizeof(char));
    char* p = buffer;
    int ret;
    EXPECT('"');
    for (;;) {
        switch (CUR()) {
            case '\0':
                ret = LEPT_PARSE_UNCLOSED_QUOTES;
                goto fail;
            case '"':
                NEXT();
                goto success;
            case '\\':
                NEXT();
                switch (CUR()) {
                    case 'b' : *p++ = '\b'; NEXT(); break;
                    case 'f' : *p++ = '\f'; NEXT(); break;
                    case 'n' : *p++ = '\n'; NEXT(); break;
                    case 'r' : *p++ = '\r'; NEXT(); break;
                    case 't' : *p++ = '\t'; NEXT(); break;
                    case '\"': *p++ = '"';  NEXT(); break;
                    case '\\': *p++ = '\\'; NEXT(); break;
                    case '/' : *p++ = '/';  NEXT(); break;
                    case 'u':
                        /* \TODO unicode */
                    default:
                        ret = LEPT_PARSE_INVALID_VALUE;
                        goto fail;
                }
                break;
            default:
                *p++ = CUR();
                NEXT();
        }
    }
success:
    *p = '\0';
    str->len = strlen(buffer);
    str->str = buffer;
    return LEPT_PARSE_OK;
fail:
    free(buffer);
    return ret;
}

static int _parse_string(lept_context* c, lept_value* v) {
    lept_string* str;
    int ret;
    str = malloc(sizeof(lept_string));
    if ((ret = _parse_str(c, str)) != LEPT_PARSE_OK) {
        free(str);
        return ret;
    }
    v->value.s = str;
    v->type = LEPT_STRING;
    return LEPT_PARSE_OK;
}

static int _parse_value(lept_context* c, lept_value* v);

static int _parse_array(lept_context* c, lept_value* v) {
    lept_array* a;
    lept_array_item* head = NULL;
    lept_array_item* previous = NULL;
    lept_array_item* item = NULL;
    lept_value* value;
    unsigned long len = 0;
    int ret;
    EXPECT('[');
    for (;;) {
        _parse_whitespace(c);
        if (CUR() == ']') {
            NEXT();
            goto success;
        }
        value = malloc(sizeof(lept_value));
        if ((ret = _parse_value(c, value)) != LEPT_PARSE_OK) {
            free(value);
            goto fail;
        }
        ++len;
        item = malloc(sizeof(lept_array_item));
        item->next = NULL;
        item->value = value;
        if (head == NULL) { /* item is first item */
            head = item;
        } else {
            previous->next = item;
        }
        previous = item;
        _parse_whitespace(c);
        if (CUR() == ']') {
            NEXT();
            goto success;
        } if (CUR() == ',') {
            NEXT();
            continue;
        } else {
            ret = LEPT_PARSE_UNCLOSED_BRACKETS;
            goto fail;
        }
    }
success:
    a = malloc(sizeof(lept_array));
    a->len = len;
    a->items = head;
    v->value.a = a;
    v->type = LEPT_ARRAY;
    return LEPT_PARSE_OK;
fail:
    while (head) {
        item = head->next;
        free(head->value);
        free(head);
        head = item;
    }
    return ret;
}

static int _parse_object(lept_context* c, lept_value* v) {
    lept_object* o;
    lept_object_node* head = NULL;
    lept_object_node* previous = NULL;
    lept_object_node* node = NULL;
    lept_string* key;
    lept_value* value;
    unsigned long len = 0;
    int ret;
    EXPECT('{');
    for (;;) {
        _parse_whitespace(c);
        if (CUR() == '}') {
            NEXT();
            goto success;
        }
        if (CUR() != '\"') {
            ret = LEPT_PARSE_INVALID_VALUE;
            goto fail;
        }
        key = malloc(sizeof(lept_string));
        if ((ret = _parse_str(c, key)) != LEPT_PARSE_OK) {
            free(key);
            goto fail;
        }
        _parse_whitespace(c);
        if (CUR() != ':') {
            free(key);
            ret = LEPT_PARSE_EXPECT_VALUE;
            goto fail;
        }
        NEXT();
        _parse_whitespace(c);
        value = malloc(sizeof(lept_value));
        if ((ret = _parse_value(c, value)) != LEPT_PARSE_OK) {
            free(value);
            goto fail;
        }
        ++len;
        node = malloc(sizeof(lept_object_node));
        node->next = NULL;
        node->key = key;
        node->value = value;
        if (head == NULL) { /* node is first node */
            head = node;
        } else {
            previous->next = node;
        }
        previous = node;
        _parse_whitespace(c);
        if (CUR() == '}') {
            NEXT();
            goto success;
        } if (CUR() == ',') {
            NEXT();
            continue;
        } else {
            ret = LEPT_PARSE_UNCLOSED_BRACKETS;
            goto fail;
        }
    }
success:
    o = malloc(sizeof(lept_array));
    o->len = len;
    o->nodes = head;
    v->value.o = o;
    v->type = LEPT_OBJECT;
    return LEPT_PARSE_OK;
fail:
    while (head) {
        node = head->next;
        free(head->key);
        free(head->value);
        free(head);
        head = node;
    }
    return ret;
}

static int _parse_value(lept_context* c, lept_value* v) {
    switch (CUR()) {
        case 'n':
            return _parse_literal(c, v, "null", LEPT_NULL);
        case 't':
            return _parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':
            return _parse_literal(c, v, "false", LEPT_FALSE);
        case '"':
            return _parse_string(c, v);
        case '[':
            return _parse_array(c, v);
        case '{':
            return _parse_object(c, v);
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '-':
            return _parse_number(c, v);
        case ']': case '}': case '\0':
            return LEPT_PARSE_EXPECT_VALUE;
        default :
            return LEPT_PARSE_INVALID_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    assert(json != NULL);
    c.json = json;
    v->type = LEPT_UNKNOWN;
    _parse_whitespace(&c);
    if ((ret = _parse_value(&c, v)) == LEPT_PARSE_OK) {
        _parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_UNKNOWN;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

#define BUFFER_SIZE 1024

int lept_parse_file(lept_value* v, const char* path) {
    char* buffer;
    int ret;
    long file_size;
    long result;
    FILE* file = fopen(path, "r");
    if (file == NULL) return LEPT_FILE_CANNOT_OPEN;
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file);
    buffer = malloc(sizeof(char) * file_size);
    result = fread(buffer, 1, file_size, file);
    if (result != file_size) {
        ret = LEPT_FILE_READ_ERROR;
        goto cleanup;
    }
    ret = lept_parse(v, buffer);
cleanup:
    fclose(file);
    free(buffer);
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL);
    assert(v->type == LEPT_NUMBER);
    return v->value.n;
}

lept_string* lept_get_string(const lept_value* v) {
    assert(v != NULL);
    assert(v->type == LEPT_STRING);
    return v->value.s;
}

lept_array* lept_get_array(const lept_value* v) {
    assert(v != NULL);
    assert(v->type == LEPT_ARRAY);
    return v->value.a;
}

lept_object* lept_get_object(const lept_value* v) {
    assert(v != NULL);
    assert(v->type == LEPT_OBJECT);
    return v->value.o;
}
