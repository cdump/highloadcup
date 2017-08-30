#pragma once
#include "http.h"

/* Application config */

/* arg - http_request_t* */
void httpapi_on_new_request(http_req_t *req);
#define APP_HTTP_ON_NEW_REQUEST(REQ) httpapi_on_new_request(REQ)

#define APP_HTTP_LISTEN_PORT 80

#define APP_THREADS_CNT 4

// #define APP_LISTEN_IN_ONE_THREAD 1
