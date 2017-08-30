#include "objects.h"
#include "jsonp.h"
#include "core.h"

#include <stdio.h>

void
user_print(user_t *u) {
    printf("User[%p]:\n", u);
    printf("  .id = %u\n", u->id);
    printf("  .email = %s\n", u->email);
    printf("  .first_name = %s\n", u->first_name);
    printf("  .last_name = %s\n", u->last_name);
    printf("  .gender = %c\n", u->gender);
    printf("  .birth_date = %d\n", u->birth_date);
}

char*
user_parse(user_t *o, char *p, const char *e)
{
#define OBJECT_FIELDS(XX)  \
    XX(first_name, dynstr) \
    XX(last_name,  dynstr) \
    XX(birth_date, num)    \
    XX(gender,     gender) \
    XX(id,         num)    \
    XX(email,      dynstr)

    PARSE_OBJECT(OBJECT_FIELDS, p, e);
    return next;
}


void
user_update_json(user_t *u)
{
    unsigned a = 0;
    char buf[4096];

    APPEND_CONST_STRING(buf, a, "{\"id\": ");
    APPEND_INT(buf, a, u->id);
    APPEND_CONST_STRING(buf, a, ", \"email\": \"");
    APPEND_BUF(buf, a, u->email, u->len_email);
    APPEND_CONST_STRING(buf, a, "\", \"first_name\": \"");
    APPEND_BUF(buf, a, u->first_name, u->len_first_name);
    APPEND_CONST_STRING(buf, a, "\", \"last_name\": \"");
    APPEND_BUF(buf, a, u->last_name, u->len_last_name);
    APPEND_CONST_STRING(buf, a, "\", \"gender\": \"");
    buf[a] = u->gender;
    a++;
    APPEND_CONST_STRING(buf, a, "\", \"birth_date\": ");
    APPEND_INT(buf, a, u->birth_date);
    APPEND_CONST_STRING(buf, a, "}\n");

    if (u->len_json < a) {
        if (u->json)
            free(u->json);
        u->json = malloc(a);
    }
    memcpy(u->json, buf, a);

    u->len_json = a;
}
