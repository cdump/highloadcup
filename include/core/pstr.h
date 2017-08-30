#pragma once

#include <inttypes.h>
#include <stdbool.h>
typedef struct {
	uint32_t len;
	char *data;
} pstr_t;

#define pstrf(s) (s)->len, (s)->data
#define pstr_cinit(cstr) {sizeof(cstr) - 1, cstr}

bool pstr_to_u64(pstr_t *v, uint64_t *out);
bool pstr_to_u32(pstr_t *v, uint32_t *out);
bool pstr_to_i32(pstr_t *v, int32_t *out);
bool pstr_to_u16(pstr_t *v, uint16_t *out);

#include <string.h>
#define PSTR_CMP(PSTR, CSTR) ((PSTR).len == sizeof(CSTR) - 1 && memcmp((PSTR).data, CSTR, sizeof(CSTR) - 1) == 0)
