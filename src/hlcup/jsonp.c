#include "jsonp.h"

#include "pstr.h"
#include <inttypes.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/*
 * HLCUP json parser
 * I don't know why not to use any existing parser, just because we need more bicycles!
 *
 * Only simple object with number / strings (only \u escape)
 */

void
json_dump(json_t *j) {
    printf("Json[%p]:\n", j);
    printf("  .cnt = %u\n", j->cnt);
    for (unsigned i = 0; i < j->cnt; i++) {
        printf("  .key(%d|%p) = |%.*s|  .value(%d|%p) = |%.*s|\n",
                j->keys[i].len, j->keys[i].data, j->keys[i].len, j->keys[i].data,
                j->values[i].len, j->values[i].data, j->values[i].len, j->values[i].data
              );
    }

}

char *
parse_json_object(char *buf, const char *e, json_t *j) {
    char *p = buf;

#define SKIP_SPACES() do {           \
    while (p <= e && isspace(*p))    \
        p++;                         \
    if (p > e)                       \
        return NULL;                 \
} while(0)

#define SKIP_SPACES_UNTIL(CHAR) do { \
    SKIP_SPACES();                   \
    if (*p != CHAR)                  \
        return NULL;                 \
    p++;                             \
} while(0)

    pstr_t str;

#define READ_STRING() do {                         \
    SKIP_SPACES_UNTIL('"');                        \
    str.data = p;                                  \
    while (p <= e && (*p != '"' || p[-1] == '\\')) \
        p++;                                       \
    if (p > e)                                     \
        return NULL;                               \
    assert(*p == '"');                             \
    p[0] = 0; /*make string NULL terminated */     \
    str.len = p - str.data;                        \
    p++;                                           \
} while(0)

#define READ_VALUE() do {                                    \
    SKIP_SPACES();                                           \
    if (*p == '"') {                                         \
        READ_STRING();                                       \
        char *t = str.data + str.len;                        \
        int c = 0;                                           \
        for (const char *q = str.data; q < t; q++) {         \
            if (q[0] == '\\' && q[1] == 'u') {               \
                unsigned num = (int)strtol(&q[2], NULL, 16); \
                str.data[c] = 0xC0 | ((num >> 6) & 0x1F);    \
                str.data[c+1] = 0x80 | (num & 0x3F);         \
                c+=2;                                        \
                q += 5;                                      \
            } else {                                         \
                str.data[c] = q[0];                          \
                c++;                                         \
            }                                                \
        }                                                    \
        str.len = c;                                         \
        str.data[c] = 0; /*make string NULL terminated */    \
    } else if (*p == '-' || isdigit(*p)) {                   \
        str.data = p;                                        \
        if (*p == '-')                                       \
            p++;                                             \
        while (p <= e && isdigit(*p))                        \
            p++;                                             \
        if (p > e)                                           \
            return NULL;                                     \
        str.len = p - str.data;                              \
        if (str.len == 1 && str.data[0] == '-')              \
            return NULL;                                     \
    } else {                                                 \
        return NULL;                                         \
    }                                                        \
} while(0)

    SKIP_SPACES_UNTIL('{');

    int c = 0;
    j->cnt = 0;
    while (true) {
        READ_STRING();
        j->keys[c] = str;
        /* printf("key len = %d | str = %p |%.*s|\n", str.len, str.data, str.len, str.data); */

        SKIP_SPACES_UNTIL(':');
        READ_VALUE();

        /* printf("value len = %d | str = %p |%.*s|\n", str.len, str.data, str.len, str.data); */
        j->values[c] = str;
        j->cnt = ++c;


        SKIP_SPACES();
        /* json_dump(j); */

        if (*p == ',') {
            /* printf("next field\n"); */
            p++;
        } else if (*p == '}') {
            /* printf("parse done, other = |%.*s|\n", e - p, p); */
            /* printf("parse done, this = |%.*s|\n", 256, buf); */
            return p;
        } else {
            printf("++++++++Err\n");
            return NULL;
        }
    }

    return NULL;
}

void
json_test() {
#define A "{\"distance\": 45, \"country\": \"\\u041d\\u0438\\u0434\\u0435\\u0440\\u043b\\u0430\\u043d\\u0434\\u044b\"}"
    char buf[2048];
    memset(buf, '\0', sizeof(buf));
    memcpy(buf, A, sizeof(A) - 1);
    json_t j;
    char *q = parse_json_object(buf, buf + 2048, &j);
    if (q) {
        json_dump(&j);
    }
    printf("q = %p\n", q);
}
