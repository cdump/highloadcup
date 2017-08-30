#define _GNU_SOURCE
#include <string.h>

#include "http.h"
#include "network.h"

#include <string.h>
#include <ctype.h>
#include "log.h"
#include <assert.h>
#include <string.h>

#include "app.h"

static void decode(pstr_t *val, bool decode_plus);

static inline int
split_kv(char *p, char *e, http_arg_t args[], unsigned max_args, tommy_hashtable *tommy_head)
{
	unsigned pos = 0;
	while (p < e) {
		http_arg_t *arg = &args[pos];
		char *s = p;

		arg->k.data = p;
		while (p < e && *p != '=')
			p++;
		arg->k.len = p - s;
		p++;

		if (p >= e)
			break;

		s = p;
		arg->v.data = p;
		while (p < e && *p != '&')
			p++;
		arg->v.len = p - s;
		p++;

		if (arg->k.len && arg->v.len)
			pos++;

		/* decode(&arg->k, decode_plus); */

        //XXX: hlcup hack, decode only country and city
        if (arg->k.data[0] == 'c') {
            decode(&arg->v, true);
        }

        tommy_hashtable_insert(tommy_head, &arg->node, arg, tommy_hash_u64(0, arg->k.data, arg->k.len));

		if (pos >= max_args)
			break;
	}
	return pos;
}

void
process_http_request(struct conn *cn)
{
    http_req_t req;
	char *e, *path;
    size_t path_len;

    conn_read_t *rs = &cn->read;
    char *data = rs->data;
    unsigned data_len = rs->data_size;
    unsigned header_len = rs->header_size;

	req.cn = cn;

    req.method = rs->method;
    //XXX: hack for HLCUP
    req.keep_alive = (req.method == HTTP_GET);

    //Remove me! only for http_test() in main!!!
    req.keep_alive = true;

    path = data + (req.method == HTTP_GET ? sizeof("GET ") : sizeof("POST ")) - 1;


    char *pe = (char*)memchr(data, '\r', data_len) - sizeof("HTTP/1.X");
    //assert pe != NULL
    pe[0] = '\0';
    /* req->minor_version = pe[8] == '0' ? 0 : 1; */
    /* req->keep_alive = (req->minor_version == 1); */
    /* req.keep_alive = true; */

    path_len = pe - path;
	req.path.data = path;

	e = strchr(path, '?');
	req.path.len = e ? (e - path) : path_len;

    // assume that only query string can contain encoded data. It's not true of course
    // and we should uncomment that line
    /* decode(&req.path, false); */

    // lazy parse in http_get_args
    req.args.decoded = false;
    if (e != NULL) {
        req.args.ptr_begin = e + 1;
        req.args.ptr_end = path + path_len;
    } else {
        req.args.ptr_begin = NULL;
        req.args.ptr_end = NULL;
    }


	req.body.data = data + header_len;
	req.body.len = data_len - header_len;

    APP_HTTP_ON_NEW_REQUEST(&req);
}

static void
decode(pstr_t *val, bool decode_plus)
{
	char *p = val->data;
    char *e = val->data + val->len;
    char *v = p;
#define h2i(c) (isdigit(c) ? (c - '0') : 10 + (isupper(c) ? c - 'A' : c - 'a'))
	while (p < e) {
		if (p[0] == '%' && e - p >= 3 && isxdigit(p[1]) && isxdigit(p[2])) {
			*v = h2i(p[1]) << 4 | h2i(p[2]);
			p += 3;
		} else {
			if (p[0] == '+' && decode_plus) {
				*v = ' ';
			} else {
				if (p != v)
					*v = *p;
			}
			p++;
		}
		v++;
	}
#undef h2i
	val->len = v - val->data;
}


static int
http_arg_compare(const void* arg, const void* obj)
{
    const pstr_t *v = (const pstr_t*)arg;
    const http_arg_t *o = (const http_arg_t*)obj;

    /* printf("cmp |%.*s| vs |%.*s|\n", pstrf(v), pstrf(&o->k)); */
    return (v->len != o->k.len || memcmp(v->data, o->k.data, v->len) != 0);
}

bool
http_get_arg(http_req_t *r, const char *name, pstr_t *v)
{
	http_arg_t *res = NULL;
    if (!r->args.decoded) {
        if (r->args.ptr_begin == NULL)
            return false;
        tommy_hashtable_init(&r->args.tommy, 8);
		r->args.cnt = split_kv(r->args.ptr_begin, r->args.ptr_end, r->args.data, COUNTOF(r->args.data), &r->args.tommy);
        r->args.decoded = true;
    }

    pstr_t tf;
    tf.data = (char*)name;
    tf.len = strlen(name);
    res = tommy_hashtable_search(&r->args.tommy, http_arg_compare, &tf, tommy_hash_u64(0, tf.data, tf.len));

    /* printf("arg |%s| = %p\n", name, res); */
	if (res == NULL)
		return false;
	v->data = res->v.data;
	v->len = res->v.len;
	return true;
}

bool
http_get_arg_u64(http_req_t *r, const char *name, uint64_t *v)
{
	pstr_t sv;
	if (!http_get_arg(r, name, &sv))
		return false;
	return pstr_to_u64(&sv, v);
}

bool
http_get_arg_u32(http_req_t *r, const char *name, uint32_t *v)
{
	pstr_t sv;
	if (!http_get_arg(r, name, &sv))
		return false;
	return pstr_to_u32(&sv, v);
}

bool
http_get_arg_i32(http_req_t *r, const char *name, int32_t *v)
{
	pstr_t sv;
	if (!http_get_arg(r, name, &sv))
		return false;
	return pstr_to_i32(&sv, v);
}
