#include "objects.h"

#include <stdio.h>
#include "jsonp.h"
#include "core.h"

void
visit_print(visit_t *v) {
    printf("Visit[%p]:\n", v);
    printf("  .id = %u\n", v->id);
    printf("  .location = %u\n", v->location);
    printf("  .user = %u\n", v->user);
    printf("  .visited_at = %u\n", v->visited_at);
    printf("  .mark = %u\n", v->mark);
}

char*
visit_parse(visit_t *o, char *p, const char *e)
{
#define OBJECT_FIELDS(XX) \
    XX(id,         num)   \
    XX(location,   num)   \
    XX(user,       num)   \
    XX(visited_at, num)   \
    XX(mark,       num)

    PARSE_OBJECT(OBJECT_FIELDS, p, e);
    return next;
}

location_t* locations_hash_get(uint32_t id);

void
visit_update_json(visit_t *v)
{
    unsigned a = 0;
    char buf[4096];
    APPEND_CONST_STRING(buf, a, "{\"id\": ");
    APPEND_INT(buf, a, v->id);
    APPEND_CONST_STRING(buf, a, ", \"location\": ");
    APPEND_INT(buf, a, v->location);
    APPEND_CONST_STRING(buf, a, ", \"user\": ");
    APPEND_INT(buf, a, v->user);
    APPEND_CONST_STRING(buf, a, ", \"visited_at\": ");
    APPEND_INT(buf, a, v->visited_at);
    APPEND_CONST_STRING(buf, a, ", \"mark\": ");
    APPEND_INT(buf, a, v->mark);
    APPEND_CONST_STRING(buf, a, "}\n");

    if (v->len_json < a) {
        if (v->json)
            free(v->json);
        v->json = malloc(a);
    }
    memcpy(v->json, buf, a);

    v->len_json = a;
}

void
visit_update_uv_json(visit_t *v)
{
#if 0
    int a = 0;
    char *p = v->uv_json;
    APPEND_CONST_STRING(p, a, "{\"mark\": ");
    APPEND_INT(p, a, v->mark);
    APPEND_CONST_STRING(p, a, ",\"visited_at\": ");
    APPEND_INT(p, a, v->visited_at);
    APPEND_CONST_STRING(p, a, ",\"place\": \"");

    location_t *loc = v->location_obj;
    assert(loc != NULL);
    /* if (loc == NULL) { */
    /*     loc = locations_hash_get(v->location); */
    /*     v->location_obj = loc; */
    /* } */
    APPEND_BUF(p, a, loc->place, loc->len_place);
    APPEND_CONST_STRING(p, a, "\"},");

    v->uv_len_json = a;
#endif
}
