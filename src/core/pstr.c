#include "pstr.h"
#include <ctype.h>

bool
pstr_to_u64(pstr_t *v, uint64_t *out)
{
	unsigned i;
	//it's enough
	if (v->len > 19)
		return false;

	*out = 0;
	for (i = 0; i < v->len; i++) {
		if (!isdigit(v->data[i]))
			return false;
		*out = *out * 10 + (v->data[i] - '0');
	}
	return true;
}

bool
pstr_to_u32(pstr_t *v, uint32_t *out)
{
	uint64_t vv;
	if (!pstr_to_u64(v, &vv) || vv > UINT32_MAX)
		return false;
	*out = vv;
	return true;
}

bool
pstr_to_i32(pstr_t *v, int32_t *out)
{
    if (v->len < 1)
        return false;

    int sign = 1;
    if (v->data[0] == '-') {
        v->data++;
        v->len--;
        sign = -1;
    }

	uint64_t vv;
	if (!pstr_to_u64(v, &vv) || vv > UINT32_MAX)
		return false;
	*out = sign * vv;
	return true;
}

bool
pstr_to_u16(pstr_t *v, uint16_t *out)
{
	uint64_t vv;
	if (!pstr_to_u64(v, &vv) || vv > UINT16_MAX)
		return false;
	*out = vv;
	return true;
}
