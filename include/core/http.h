#pragma once

#include <inttypes.h>
#include "core.h"

#include "tommyhashdyn.h"
#include "tommyhashtbl.h"

// #define IS_GET(BUF) (*((uint32_t*)(BUF)) == 542393671 #<{(|"GET "|)}>#)
// #define IS_POST(BUF) (*((uint32_t*)(BUF)) == 1414745936 #<{(|"POST"|)}>#)

#define IS_GET(BUF) (memcmp((BUF), "GET ", 4) == 0)
#define IS_POST(BUF) (memcmp((BUF), "POST", 4) == 0)

#include "pstr.h"
typedef struct {
	pstr_t k;			/* key: MUST be NULL terminated */
	pstr_t v;			/* value: can be not NULL terminated */

    tommy_node node; /* makes this structure hashable */
} http_arg_t;

typedef enum {
	HTTP_GET,
	HTTP_POST
} http_method_t;

typedef enum {
	HTTP_OK                    = 200,
	HTTP_BAD_REQUEST           = 400,
	HTTP_NOT_FOUND             = 404,
	HTTP_INTERNAL_SERVER_ERROR = 500,
} http_status_t;

typedef struct {
	struct conn *cn;
	// int minor_version;
	http_method_t method;

	bool keep_alive;

	//url-decoded GET args
	struct {
        char *ptr_begin;
        char *ptr_end;
        bool decoded;

        tommy_hashtable tommy;

		http_arg_t data[16];
		unsigned cnt;
	} args;

	pstr_t path; //url-decoded path
	pstr_t body;
} http_req_t;


bool http_get_arg(http_req_t *r, const char *name, pstr_t *v);
bool http_get_arg_u64(http_req_t *r, const char *name, uint64_t *v);
bool http_get_arg_u32(http_req_t *r, const char *name, uint32_t *v);
bool http_get_arg_i32(http_req_t *r, const char *name, int32_t *v);
bool http_get_arg_u16(http_req_t *r, const char *name, uint16_t *v);

void process_http_request(struct conn *cn);
