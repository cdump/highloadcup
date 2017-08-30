#include "objects.h"

#include "jsonp.h"
#include "core.h"

#include <stdio.h>

void
location_print(location_t *l) {
    printf("Location[%p]:\n", l);
    printf("  .id = %u\n", l->id);
    printf("  .place(%u) = %s\n", l->len_place, l->place);
    printf("  .country(%u) = %s\n", l->len_country, l->country);
    printf("  .city(%u) = %s\n", l->len_city, l->city);
    printf("  .distance = %u\n", l->distance);
}

char*
location_parse(location_t *o, char *p, const char *e)
{
#define OBJECT_FIELDS(XX) \
    XX(distance, num)     \
    XX(place,    dynstr)  \
    XX(id,       num)     \
    XX(city,     dynstr)  \
    XX(country,  dynstr)

    PARSE_OBJECT(OBJECT_FIELDS, p, e);
    return next;
}

void
location_update_json(location_t *l)
{
    unsigned a = 0;
    //FIXME: allocate buf on heap if place len is big
    char buf[1024];

    APPEND_CONST_STRING(buf, a, "{\"id\": ");
    APPEND_INT(buf, a, l->id);
    APPEND_CONST_STRING(buf, a, ", \"place\": \"");
    APPEND_BUF(buf, a, l->place, l->len_place);
    APPEND_CONST_STRING(buf, a, "\", \"country\": \"");
    APPEND_BUF(buf, a, l->country, l->len_country);
    APPEND_CONST_STRING(buf, a, "\", \"city\": \"");
    APPEND_BUF(buf, a, l->city, l->len_city);
    APPEND_CONST_STRING(buf, a, "\", \"distance\": ");
    APPEND_INT(buf, a, l->distance);
    APPEND_CONST_STRING(buf, a, "}\n");

    if (l->len_json < a) {
        if (l->json)
            free(l->json);
        l->json = malloc(a);
    }
    memcpy(l->json, buf, a);

    l->len_json = a;
}
