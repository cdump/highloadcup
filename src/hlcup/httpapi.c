#include <assert.h>

#include <sys/epoll.h>
#include "network.h"
#include "log.h"
#include "http.h"
#include "core.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "api.h"

void
httpapi_on_new_request(http_req_t *req)
{
    /* log_w("Got req: %s %.*s", req->method == HTTP_POST ? "POST" : "GET", pstrf(&req->path)); */
    /* http_reply_str(req, HTTP_OK, "1\n"); */
    /* return; */

/* #define D "HTTP/1.1 200 Ok\r\nConnection: keep-alive\r\nContent-Length: 2\r\n\r\n1\n" */
/*     write(req->cn->sock, D, sizeof(D) - 1); */
/*     return; */

    pstr_t path = req->path;

    pstr_t str_prefix = {0, NULL};
    pstr_t str_id = {0, NULL};
    pstr_t str_action = {0, NULL};
    char *f_s = strchr(path.data + 1, '/');
    str_prefix.data = path.data + 1;
    if (!f_s) {
        goto _404;
    }

    str_prefix.len = f_s - path.data - 1;
    str_id.data = f_s + 1;
    char *f_i = strchr(f_s + 1, '/');
    if (!f_i) {
        str_id.len = path.data + path.len - f_s - 1;
    } else {
        str_id.len = f_i - f_s - 1;
        str_action.data = f_i + 1;
        char *f_a = strchr(f_i + 1, '?');
        if (!f_a) {
            str_action.len = path.data + path.len - f_i - 1;
        } else {
            str_action.len = f_a - f_i - 1;
        }
    }
    if (str_id.len == 0)
        goto _404;

    uint32_t id;
    if (req->method == HTTP_POST) {
        if (str_action.len != 0)
            goto _404;
        char *p = req->body.data;
        const char *e = req->body.data + req->body.len;
        int code;
        if (PSTR_CMP(str_id, "new")) {
            //create
            if (PSTR_CMP(str_prefix, "users")) {
                code = users_new(&p, e);
                http_reply_str(req, code, "{}\n");
                return;
            }
            if (PSTR_CMP(str_prefix, "locations")) {
                code = locations_new(&p, e);
                http_reply_str(req, code, "{}\n");
                return;
            }
            if (PSTR_CMP(str_prefix, "visits")) {
                code = visits_new(&p, e);
                http_reply_str(req, code, "{}\n");
                return;
            }
        } else if (pstr_to_u32(&str_id, &id)) {
            //modify
            if (PSTR_CMP(str_prefix, "users")) {
                code = users_update(id, p, e);
                http_reply_str(req, code, "{}\n");
                return;
            }
            if (PSTR_CMP(str_prefix, "locations")) {
                code = locations_update(id, p, e);
                http_reply_str(req, code, "{}\n");
                return;
            }
            if (PSTR_CMP(str_prefix, "visits")) {
                code = visits_update(id, p, e);
                http_reply_str(req, code, "{}\n");
                return;
            }
        }
    } else {
        //HTTP GET
        if (!pstr_to_u32(&str_id, &id))
            goto _404;

        if (PSTR_CMP(str_prefix, "users")) {
            if (str_action.len == 0) {
                //users/ID
                user_t *u;
                u = users_get(id);
                if (u == NULL) {
                    http_reply_str(req, HTTP_NOT_FOUND, "{}\n");
                } else {
                    http_reply_and_free(req, HTTP_OK, u->json, u->len_json, NULL);
                }
                return;
            } else if (PSTR_CMP(str_action, "visits")) {
                //users/ID/visits

                uint32_t from_date = 0;
                uint32_t to_date = UINT32_MAX;
                uint32_t to_distance = UINT32_MAX;
                const char *country = NULL;
                pstr_t p;
                char country_buf[4 * 50 + 1];
                if (http_get_arg(req, "fromDate", &p)) {
                    if (!pstr_to_u32(&p, &from_date)) {
                        http_reply_str(req, HTTP_BAD_REQUEST, "{}\n");
                        return;
                    }
                    //parsed ok
                }
                if (http_get_arg(req, "toDate", &p)) {
                    if (!pstr_to_u32(&p, &to_date)) {
                        http_reply_str(req, HTTP_BAD_REQUEST, "{}\n");
                        return;
                    }
                    //parsed ok
                }
                if (http_get_arg(req, "country", &p)) {
                    country = country_buf;
                    assert(p.len < sizeof(country_buf));
                    memcpy(country_buf, p.data, p.len);
                    country_buf[p.len] = 0;
                }
                if (http_get_arg(req, "toDistance", &p)) {
                    if (!pstr_to_u32(&p, &to_distance)) {
                        http_reply_str(req, HTTP_BAD_REQUEST, "{}\n");
                        return;
                    }
                    //parsed ok
                }

                char *b = memalloc_alloc(1024*1024);
                int off = 0;
                APPEND_CONST_STRING(b, off, "{\"visits\":[");
                char *pos = b + off;
                int code;
                code = users_visits(id, from_date, to_date, country, to_distance, &pos);
                off = 0;
                APPEND_CONST_STRING(pos, off,"]}\n\0");
                http_reply_and_free(req, code == 404 ? HTTP_NOT_FOUND : HTTP_OK, b, strlen(b), b);
                return;
            }
        } else if (PSTR_CMP(str_prefix, "locations")) {
            if (str_action.len == 0) {
                //locations/ID
                location_t *l;
                l = locations_get(id);
                if (l == NULL) {
                    http_reply_str(req, HTTP_NOT_FOUND, "{}\n");
                } else {
                    http_reply_and_free(req, HTTP_OK, l->json, l->len_json, NULL);
                }
                return;
            } else if (PSTR_CMP(str_action, "avg")) {
                //locations/ID/avg

                uint32_t from_date = 0;
                uint32_t to_date = UINT32_MAX;
                uint32_t from_age = 0;
                uint32_t to_age = UINT32_MAX;
                char gender = '\0';
                pstr_t p;
                if (http_get_arg(req, "fromDate", &p)) {
                    if (!pstr_to_u32(&p, &from_date)) {
                        http_reply_str(req, HTTP_BAD_REQUEST, "{}\n");
                        return;
                    }
                    //parsed ok
                }
                if (http_get_arg(req, "toDate", &p)) {
                    if (!pstr_to_u32(&p, &to_date)) {
                        http_reply_str(req, HTTP_BAD_REQUEST, "{}\n");
                        return;
                    }
                    //parsed ok
                }
                if (http_get_arg(req, "fromAge", &p)) {
                    if (!pstr_to_u32(&p, &from_age)) {
                        http_reply_str(req, HTTP_BAD_REQUEST, "{}\n");
                        return;
                    }
                    //parsed ok
                }
                if (http_get_arg(req, "toAge", &p)) {
                    if (!pstr_to_u32(&p, &to_age)) {
                        http_reply_str(req, HTTP_BAD_REQUEST, "{}\n");
                        return;
                    }
                    //parsed ok
                }
                if (http_get_arg(req, "gender", &p)) {
                    if (p.len != 1 || (p.data[0] != 'f' && p.data[0] != 'm')) {
                        http_reply_str(req, HTTP_BAD_REQUEST, "{}\n");
                        return;
                    }
                    gender = p.data[0];
                    //parsed ok
                }
                int code;
                char *buf = memalloc_alloc(32);
                int blen = 0;
                APPEND_CONST_STRING(buf, blen, "{\"avg\":         }");

                code = locations_avg(id, from_date, to_date, from_age, to_age, gender, &buf[8]);
                if (code == 200) {
                    http_reply_and_free(req, HTTP_OK, buf, blen, buf);
                } else {
                    memalloc_free(buf);
                    http_reply_str(req, HTTP_NOT_FOUND, "{}\n");
                }
                return;
            }
        } else if (PSTR_CMP(str_prefix, "visits") && str_action.len == 0) {
            //visits/ID
            visit_t *v;
            v = visits_get(id);
            if (v == NULL) {
                http_reply_str(req, HTTP_NOT_FOUND, "{}\n");
            } else {
                http_reply_and_free(req, HTTP_OK, v->json, v->len_json, NULL);
            }
            return;
        }
    }

_404:
    http_reply_str(req, HTTP_NOT_FOUND, "404\n");
}
