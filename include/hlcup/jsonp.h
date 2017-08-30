#pragma once
#include <inttypes.h>
#include "pstr.h"

typedef struct {
    uint32_t cnt;
    pstr_t keys[8];
    pstr_t values[8];
} json_t;

void json_dump(json_t *j);
char *parse_json_object(char *buf, const char *e, json_t *j);
void json_test();
