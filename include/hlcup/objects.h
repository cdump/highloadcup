#pragma once
#include <inttypes.h>
#include "pstr.h"
#include <stdbool.h>
#include "tommyhashdyn.h"

struct visit_struct;
struct user_struct;
struct location_struct;

// id - уникальный внешний id посещения. Устанавливается тестирующей системой. 32-разрядное целое число.
// location - id достопримечательности. 32-разрядное целое число.
// user - id путешественника. 32-разрядное целое число.
// visited_at - дата посещения, timestamp с ограничениями: снизу 01.01.2000, а сверху 01.01.2015.
// mark - оценка посещения от 0 до 5 включительно. Целое число.
typedef struct visit_struct {
    tommy_node node; /* makes this structure hashable */

    struct location_struct *location_obj;
    struct user_struct *user_obj;

    char *json;

    uint32_t id;
    uint32_t location;
    uint32_t user;
    uint32_t visited_at;

    uint8_t mark;
    uint8_t len_json;
} visit_t;


int my_tree_compare(struct visit_struct *a, struct visit_struct *b);
#define BPS_TREE_NAME visits_tree
#define BPS_TREE_BLOCK_SIZE 64
#define BPS_TREE_EXTENT_SIZE 128
#define BPS_TREE_COMPARE(a, b, arg) my_tree_compare(a, b)
#define BPS_TREE_COMPARE_KEY(a, b, arg) ((a)->visited_at < (b) ? -1 : (a)->visited_at > (b))
#define bps_tree_elem_t struct visit_struct*
#define bps_tree_key_t uint32_t
#include "bps_tree.h"

/* id - уникальный внешний id достопримечательности. Устанавливается тестирующей системой. 32-разрядное целое число. */
/* place - описание достопримечательности. Текстовое поле неограниченной длины. */
/* country - название страны расположения. unicode-строка длиной до 50 символов. */
/* city - название города расположения. unicode-строка длиной до 50 символов. */
/* distance - расстояние от города по прямой в километрах. 32-разрядное целое число. */
typedef struct location_struct {
    tommy_node node; /* makes this structure hashable */

    uint32_t id;
    uint32_t distance;
    uint32_t len_place;
    uint32_t len_country;
    uint32_t len_city;
    uint8_t len_json;

    uint16_t simple_avg_cnt;
    uint16_t simple_avg_sum;

    char *place;
    char *country;
    char *city;

    char *json;

    //visits sorted by visited_at
    struct visits_tree vtree;
} location_t;



// id - уникальный внешний идентификатор пользователя. Устанавливается тестирующей системой и используется затем, для проверки ответов сервера. 32-разрядное целое число.
// email - адрес электронной почты пользователя. Тип - unicode-строка длиной до 100 символов. Гарантируется уникальность.
// first_name и last_name - имя и фамилия соответственно. Тип - unicode-строки длиной до 50 символов.
// gender - unicode-строка "m" означает мужской пол, а "f" - женский.
// birth_date - дата рождения, записанная как число секунд от начала UNIX-эпохи по UTC (другими словами - это timestamp). Ограничено снизу 01.01.1930 и сверху 01.01.1999-ым.
typedef struct user_struct {
    //32 bytes
    tommy_node node; /* makes this structure hashable */

    uint32_t id;
    int32_t birth_date;

    uint32_t len_email;
    uint32_t len_first_name;
    uint32_t len_last_name;
    uint32_t len_json;

    char *email;
    char *first_name;
    char *last_name;
    char *json;

    char gender;

    //visits sorted by visited_at
    //144 bytes
    struct visits_tree vtree;
} user_t;

char* user_parse(user_t *u, char *buf, const char *e);
void user_print(user_t *u);
void user_update_json(user_t *u);

char* location_parse(location_t *l, char *buf, const char *e);
void location_print(location_t *l);
void location_update_json(location_t *l);

char* visit_parse(visit_t *u, char *buf, const char *e);
void visit_print(visit_t *u);
void visit_update_json(visit_t *v);
void visit_update_uv_json(visit_t *v);


#if 0
    if (o->len_##NAME < int_j_obj.values[i].len + 1) { \
        if (o->NAME) \
            free(o->NAME); \
        o->NAME = malloc(int_j_obj.values[i].len + 1); \
    } \

#endif


#include "jsonp.h"
#define FILL_str(NAME) do {                                                         \
    /* all strings are null terminated, so copy NULL byte too */                    \
    memcpy(o->NAME, int_j_obj.values[i].data, int_j_obj.values[i].len + 1/*NULL*/); \
    o->len_##NAME = int_j_obj.values[i].len ;                                       \
} while(0)

#define FILL_dynstr(NAME) do {                                                      \
    /* all strings are null terminated, so copy NULL byte too */                    \
    if (o->len_##NAME < int_j_obj.values[i].len) {                                  \
        if (o->NAME) {                                                              \
            /*printf("realloc str %s, was |%.*s| new |%.*s|\n",                       \
                #NAME, o->len_##NAME, o->NAME, int_j_obj.values[i].len, int_j_obj.values[i].data); */ \
            free(o->NAME);                                                          \
        }                                                                           \
        o->NAME = malloc(int_j_obj.values[i].len + 1);                              \
    }                                                                               \
    FILL_str(NAME);                                                                 \
} while(0)

#define FILL_num(NAME) do {                                          \
    char *eptr = int_j_obj.values[i].data + int_j_obj.values[i].len; \
    o->NAME = strtoll(int_j_obj.values[i].data, &eptr, 10);          \
} while(0)

#define FILL_gender(NAME) do {                                              \
    o->NAME = int_j_obj.values[i].data[0];                                  \
    if (int_j_obj.values[i].len != 1 || (o->NAME != 'f' && o->NAME != 'm')) \
        return NULL;                                                        \
} while(0)

#define PARSE_OBJECT_FILL_HELPER(NAME, TYPE)      \
        if (PSTR_CMP(int_j_obj.keys[i], #NAME)) { \
            FILL_##TYPE(NAME);                    \
        } else

#define PARSE_OBJECT(FIELDS, P, E)                        \
    json_t int_j_obj;                                     \
    char *next = parse_json_object((P), (E), &int_j_obj); \
    if (!next)                                            \
        return NULL;                                      \
    for (unsigned i = 0; i < int_j_obj.cnt; i++) {        \
        FIELDS(PARSE_OBJECT_FILL_HELPER) {}               \
    }

