#pragma once
#include <stdbool.h>
#include <inttypes.h>
#include <sys/uio.h>

#include "queue.h"
#include "http.h"

/* Reply constant string */
#define http_reply_str(R, STATUS, STR) http_reply_and_free(R, STATUS, STR, sizeof(STR) - 1, NULL)

/* Reply & free() */
void http_reply_and_free(http_req_t *r, uint16_t status, char *body, unsigned bodylen, void *free_on_done);

typedef struct write_task {
    struct iovec *iov;
    int iovcnt;

    char hdrbuf[128];
    struct iovec iov_buf[16];

    void *free_on_done;
    bool close_on_done;

    TAILQ_ENTRY(write_task) link;
} write_task_t;

#define READ_BUF_SIZE 3000

typedef struct conn_read {
    bool hdr_found;
    http_method_t method;       /* .. */
    uint32_t data_off;			/* current read position in buffer */
    uint32_t data_size;			/* real buffer size */
    uint32_t header_size;		/* size of header, which is stored in first N bytes in *data */
    char data[READ_BUF_SIZE];	/* read buffer for header */
} conn_read_t;

typedef struct conn {
    int sock;

    conn_read_t read;

    TAILQ_HEAD(write_tasks_head, write_task) write_tasks;

    TAILQ_ENTRY(llconn) link;
} conn_t;


void net_global_init();
void net_start(uint16_t port);
void conn_start_write_watcher(conn_t *cn);
