#include "leptjson.h"

#include <assert.h> /* assert() */
#include <errno.h>  /* errno, ERANGE */
#include <math.h>   /* HUGE_VAL */
#include <stdlib.h> /* NULL, strtod(), malloc() */
#include <stdio.h>  /* f****() */
#include <string.h> /* strlen(), strncmp() */

#define NEW(type) ((type*)malloc(sizeof(type)))
#define NEWN(n, type) ((type*)malloc(n * sizeof(type)))

#define CUR() (*c->json)
#define NEXT() do { ++c->json; } while (0)
#define IS(ch) (CUR() == (ch))
#define C2I(ch)                     \
    ((ch) > 'a' ? (ch) - 'a' + 10 : \
     (ch) > 'A' ? (ch) - 'A' + 10 : \
                  (ch) - '0')

#define ISDIGIT(ch)     ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')

#define EXPECT(ch)             \
    do {                       \
        assert(CUR() == (ch)); \
        NEXT();                \
    } while (0)

typedef struct lept_context_s {
    const char* json;
} lept_context;

static int _parse_whitespace(lept_context* c) {
    while (IS(' ') || IS('\t') || IS('\n') || IS('\r')) {
        NEXT();
    }
    return LEPT_PARSE_OK;
}

static int _parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t len = strlen(literal);
    if (strncmp(c->json, literal, len) != 0) {
        return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += len;
    v->type = type;
    return LEPT_PARSE_OK;
}

static const char* _validate_number(const char* str) {
    const char* p = str;
    if (*p == '-') ++p;
    if (*p == '0') {
        ++p;
    } else if (ISDIGIT1TO9(*p)) {
        for (++p; ISDIGIT(*p); ++p);
    } else {
        goto invalid;
    }
    if (*p == '.') {
        ++p;
        if (!ISDIGIT(*p)) goto invalid;
        for (++p; ISDIGIT(*p); ++p);
    }
    if (*p == 'e' || *p == 'E') {
        ++p;
        if (*p == '+' || *p == '-') {
            ++p;
        }
        if (!ISDIGIT(*p)) goto invalid;
        for (++p; ISDIGIT(*p); ++p);
    }
    return p;
invalid:
    return str;
}

static int _parse_number(lept_context* c, lept_value* v) {
    const char* end;
    if ((end = _validate_number(c->json)) == c->json) {
        return LEPT_PARSE_INVALID_VALUE;
    }
    errno = 0;
    v->value.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->value.n == HUGE_VAL || v->value.n == -HUGE_VAL)) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    c->json = end;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int _parse_str(lept_context* c, lept_string* str) {
    int buffer_capacity = 8;
    char* buffer = NEWN(buffer_capacity, char);
    char* p = buffer;
    char* new_buffer = NULL;
    int ret;
    EXPECT('"');
    for (;;) {
        if (p - buffer == buffer_capacity) { /* reach buffer end */
            new_buffer = NEWN(buffer_capacity * 2, char);
            memcpy(new_buffer, buffer, buffer_capacity);
            free(buffer);
            p = new_buffer + (p - buffer);
            buffer_capacity *= 2;
            buffer = new_buffer;
        }
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
    str->len = p - buffer;
    str->str = buffer;
    return LEPT_PARSE_OK;
fail:
    free(buffer);
    return ret;
}

static int _parse_string(lept_context* c, lept_value* v) {
    lept_string* str;
    int ret;
    str = lept_new_string();
    if ((ret = _parse_str(c, str)) != LEPT_PARSE_OK) {
        lept_free_string(str);
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
    size_t len = 0;
    int ret;
    EXPECT('[');
    for (;;) {
        _parse_whitespace(c);
        if (CUR() == ']') {
            NEXT();
            goto success;
        }
        value = lept_new_value();
        if ((ret = _parse_value(c, value)) != LEPT_PARSE_OK) {
            lept_free_value(value);
            goto fail;
        }
        ++len;
        item = lept_new_array_item();
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
    a = lept_new_array();
    a->len = len;
    a->items = head;
    v->value.a = a;
    v->type = LEPT_ARRAY;
    return LEPT_PARSE_OK;
fail:
    while (head) {
        item = head->next;
        lept_free_array_item(head);
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
    size_t len = 0;
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
        key = lept_new_string();
        if ((ret = _parse_str(c, key)) != LEPT_PARSE_OK) {
            lept_free_string(key);
            goto fail;
        }
        _parse_whitespace(c);
        if (CUR() != ':') {
            lept_free_string(key);
            ret = LEPT_PARSE_EXPECT_VALUE;
            goto fail;
        }
        NEXT();
        _parse_whitespace(c);
        value = lept_new_value();
        if ((ret = _parse_value(c, value)) != LEPT_PARSE_OK) {
            lept_free_string(key);
            lept_free_value(value);
            goto fail;
        }
        ++len;
        node = lept_new_object_node();
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
    o = lept_new_object();
    o->len = len;
    o->nodes = head;
    v->value.o = o;
    v->type = LEPT_OBJECT;
    return LEPT_PARSE_OK;
fail:
    while (head) {
        node = head->next;
        lept_free_object_node(head);
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
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_string* lept_new_string() {
    lept_string* s = NEW(lept_string);
    s->len = 0;
    s->str = NULL;
    return s;
}

lept_array_item* lept_new_array_item() {
    lept_array_item* i = NEW(lept_array_item);
    i->next = NULL;
    i->value = NULL;
    return i;
}

lept_array* lept_new_array() {
    lept_array* a = NEW(lept_array);
    a->len = 0;
    a->items = NULL;
    return a;
}

lept_object_node* lept_new_object_node() {
    lept_object_node* n = NEW(lept_object_node);
    n->next = NULL;
    n->key = NULL;
    n->value = NULL;
    return n;
}

lept_object* lept_new_object() {
    lept_object* o = NEW(lept_object);
    o->len = 0;
    o->nodes = NULL;
    return o;
}

lept_value* lept_new_value() {
    lept_value* v = NEW(lept_value);
    v->type = LEPT_UNKNOWN;
    return v;
}

void lept_free_string(lept_string* s) {
    assert(s != NULL);
    free(s->str);
    free(s);
}

void lept_free_array_item(lept_array_item* i) {
    assert(i != NULL);
    lept_free_value(i->value);
    free(i);
}

void lept_free_array(lept_array* a) {
    lept_array_item* item;
    lept_array_item* next;
    assert(a != NULL);
    for (item = a->items; item; item = next) {
        next = item->next;
        lept_free_array_item(item);
    }
    free(a);
}

void lept_free_object_node(lept_object_node* n) {
    assert(n != NULL);
    lept_free_string(n->key);
    lept_free_value(n->value);
    free(n);
}

void lept_free_object(lept_object* o) {
    lept_object_node* node;
    lept_object_node* next;
    assert(o != NULL);
    for (node = o->nodes; node; node = next) {
        next = node->next;
        lept_free_object_node(node);
    }
    free(o);
}

void lept_free_value_on_stack(lept_value* v) {
    assert(v != NULL);
    switch (v->type) {
        case LEPT_STRING: lept_free_string(v->value.s); break;
        case LEPT_ARRAY : lept_free_array(v->value.a);  break;
        case LEPT_OBJECT: lept_free_object(v->value.o); break;
        default: break;
    }
}

void lept_free_value(lept_value* v) {
    assert(v != NULL);
    lept_free_value_on_stack(v);
    free(v);
}

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
    buffer = NEWN(file_size + 1, char);
    result = fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
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
