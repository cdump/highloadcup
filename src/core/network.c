#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "network.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include "queue.h"

#include "app.h"
#include "core.h"
#include "http.h"
#include "memalloc.h"

/* #define ENABLE_STAT_SOCK 1 */

#define IOVS(name, ...) struct iovec name[] = {  __VA_ARGS__ }

#ifdef ENABLE_STAT_SOCK
static int stat_socket;
static struct sockaddr_in stat_addr;
static __thread char stat_start[4];
static __thread char stat_stop[4];
#endif

static volatile int32_t live_conn_cnt = 0;
static __thread int epoll_fd = 0;
static int sock_transfer_pipe[APP_THREADS_CNT][2];

static int real_write(int sock, write_task_t *wt);
static void conn_destroy(conn_t *cn);
static void conn_stop_write_watcher(conn_t *cn);
static inline int find_header(char *data, unsigned len, http_method_t *method, uint64_t *content_length);
static inline void process_write(conn_t *cn);
static inline void process_read(conn_t *cn);
static void loop(int listen_socket);

// write task to socket and update task
// Return values:
// 0 - all writeen
// 1 - NOT all written, need to retry write same task object later
// 2 - nothing was written (EAGAIN from writev)
// -1 - disconnect
// -2 - write error
static int
real_write(int sock, write_task_t *wt)
{
    int r;
	do {
        r = writev(sock, wt->iov, wt->iovcnt);
	} while (r == -1 && errno == EINTR);

    if (r > 0) {
        while (wt->iovcnt && r >= (int)wt->iov[0].iov_len) {
            r -= wt->iov[0].iov_len;
            wt->iov++;
            wt->iovcnt--;
        }
        if (r) {
            wt->iov[0].iov_len -= r;
            wt->iov[0].iov_base += r; // GCC Arithmetic on void threats sizeof(void) == 1
        }
        if (wt->iovcnt == 0)
            return 0;
    } else if (r == 0) {
        //disconnect
        return -2;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 2;
        return -2;
    }
    return -2;
}

void
http_reply_and_free(http_req_t *req, uint16_t status, char *body, unsigned bodylen, void *free_on_done)
{
    /* static char hdr_ka[] = "HTTP/1.1 200 X\r\nContent-Length:         \r\nContent-Type: application/json\r\nConnection: keep-alive\r\n\r\n"; */
    /* static char hdr_cl[] = "HTTP/1.1 200 X\r\nContent-Length:         \r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"; */

    static char hdr_ka[] = "HTTP/1.1 200 X\r\nContent-Length:         \r\nConnection: keep-alive\r\n\r\n";
    static char hdr_cl[] = "HTTP/1.1 200 X\r\nContent-Length:         \r\nConnection: close\r\n\r\n";


	int hlen;
    write_task_t task;
    if (req->keep_alive) {
        hlen = sizeof(hdr_ka) - 1;
        memcpy(task.hdrbuf, hdr_ka, hlen);
    } else {
        hlen = sizeof(hdr_cl) - 1;
        memcpy(task.hdrbuf, hdr_cl, hlen);
    }

    int off = sizeof("HTTP/1.1 200 X\r\nContent-Length: ") - 1;
    APPEND_INT(task.hdrbuf, off, bodylen);
    task.hdrbuf[off] = ' ';

    switch(status) {
    case 200:
        break;
    case 400:
        task.hdrbuf[9] = '4';
        break;
    case 404:
        task.hdrbuf[9] = '4';
        task.hdrbuf[11] = '4';
        break;
    default:
        task.hdrbuf[9] = '5';
        break;
    }

    IOVS(iov,
            {task.hdrbuf, hlen},
            {body, bodylen}
        );

    task.iov = iov;
    task.iovcnt = COUNTOF(iov);

    conn_t *cn = req->cn;

    /* real_write(cn->sock, &task); */
    /* conn_read_t *rs = &cn->read; */
    /* #<{(| rs->data_size = rs->data_off = rs->hdr_found = 0; |)}># */
    /* #<{(| rs->hdr_found = true; |)}># */
    /* return; */

    /* If write queue is empty, we can try write right now, without epoll */
    if (TAILQ_EMPTY(&cn->write_tasks)) {
        int r = real_write(cn->sock, &task);
        if (r == 0) {
            memalloc_free(free_on_done);
            if (req->keep_alive) {
                conn_read_t *rs = &cn->read;
                rs->data_size = rs->data_off = rs->hdr_found = 0;
            } else {
                conn_destroy(cn);
            }
            return;
        } else if (r < 0) {
            //error or discon
            memalloc_free(free_on_done);
            conn_destroy(cn);
            return;
        }
    }

    // If we reach this point, it's time to start epoll watcher

    write_task_t *t = (write_task_t*)memalloc_alloc(sizeof(*t));
    t->free_on_done = free_on_done;
    t->close_on_done = !req->keep_alive;

    // Copy stack-allocated iov to heap-allocated mem
    t->iovcnt = COUNTOF(iov);
    t->iov = t->iov_buf;

    // Copy stack-allocated header to heap-allocated mem
    memcpy(t->hdrbuf, iov[0].iov_base, iov[0].iov_len);
    t->iov[0].iov_base = t->hdrbuf;
    t->iov[0].iov_len = iov[0].iov_len;

    for (size_t i = 1; i < COUNTOF(iov); i++) {
        t->iov[i].iov_base = iov[i].iov_base;
        t->iov[i].iov_len = iov[i].iov_len;
    }
    TAILQ_INSERT_TAIL(&cn->write_tasks, t, link);

    conn_start_write_watcher(cn);
}


/* We always want EPOLLIN and sometimes EPOLLOUT */
void
conn_start_write_watcher(conn_t *cn)
{
    struct epoll_event ev;
    ev.events = (EPOLLIN | EPOLLOUT);
    ev.data.ptr = cn;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, cn->sock, &ev);
}

/* We always want EPOLLIN and sometimes EPOLLOUT */
static void
conn_stop_write_watcher(conn_t *cn)
{
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = cn;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, cn->sock, &ev);
}

static void
conn_destroy(conn_t *cn)
{
    __sync_sub_and_fetch(&live_conn_cnt, 1);
    close(cn->sock);
    memalloc_free(cn);
}


// -1 = error
// 0 = need more data
// > 0 = offset to body
static inline int
find_header(char *data, unsigned len, http_method_t *method, uint64_t *content_length)
{
#define CL "Content-Length:"
    if (len < 8)
        return 0;

    char *end = data + len - 4;
    if (memcmp(end, "\r\n\r\n", 4) != 0) {
        end = (char*)memmem(data, len, "\r\n\r\n", 4);
        if (end == NULL)
            return 0; /* not found, need more data */
    }

    //find content-length only if POST
    if (IS_POST(data)) {
        *method = HTTP_POST;
        end[0] = '\0';
        char *c = strcasestr(data, CL);
        if (c) {
            c += sizeof(CL) - 1;
            *content_length = atoll(c);
        }
    } else {
        *method = HTTP_GET;
    }

    /* if (content_length > size_limit) */
    /*     return -1; */

	return (end - data) + 4;
#undef CL
}

static inline void
process_write(conn_t *cn)
{
    int r;
    do {
        write_task_t *wt = TAILQ_FIRST(&cn->write_tasks);
        if (wt == NULL) {
            conn_stop_write_watcher(cn);
            break;
        }
        r = real_write(cn->sock, wt);

        if (r == 0) {
            memalloc_free(wt->free_on_done);
            TAILQ_REMOVE(&cn->write_tasks, wt, link);
            if (!wt->close_on_done) {
                conn_read_t *rs = &cn->read;
                rs->data_size = rs->data_off = rs->hdr_found = 0;
            } else {
                conn_destroy(cn);
            }
            memalloc_free(wt);
        } else if (r == 2) {
            //EAGAIN
            break;
        } else if (r < 0) {
            //error or disconnect
            memalloc_free(wt->free_on_done);
            conn_destroy(cn);
            memalloc_free(wt);
            break;
        }
    } while (1);
}

static inline void
process_read(conn_t *cn)
{
    conn_read_t *rs = &cn->read;
    int r;
    char *data_ptr = rs->data;
    uint32_t data_off = rs->data_off;
    uint32_t want_read = READ_BUF_SIZE - data_off;
    do {
        r = read(cn->sock, data_ptr + data_off, want_read);
    } while (r == -1 && errno == EINTR);

    if (r > 0) {
#if 0
#define D "HTTP/1.1 200 Ok\r\nConnection: keep-alive\r\nContent-Length: 2\r\n\r\n1\n"
        write(cn->sock, D, sizeof(D) - 1);
#else
        data_off += r;
        rs->data_off = data_off;

        //find header
        if (!rs->hdr_found) {
            uint64_t clen = 0;
            int h = find_header(data_ptr, data_off, &rs->method, &clen);
            if (h < 0) {
                if (h == 0) {
                    if (data_off == READ_BUF_SIZE) {
                        fprintf(stderr, "header find error: not found\n");
                        conn_destroy(cn);
                    }
                    //not found yet
                    return;
                } else {
                    //h == -1
                    fprintf(stderr, "header find error\n");
                    conn_destroy(cn);
                    return;
                }
            }

            rs->hdr_found = true;
            rs->data_size = h + clen;
            rs->header_size = h;

            if (h + clen > data_off) {
                //not all data read
                return;
            }
        } else {
            if (rs->data_size > data_off) {
                //not all data read
                return;
            }
        }

        assert(rs->data_size <= data_off);
        /* printf("data = |%.*s|\n", cn->read.data_size, cn->read.data); */

/* #define D "HTTP/1.1 200 Ok\r\nConnection: keep-alive\r\nContent-Length: 2\r\n\r\n1\n" */
/*         write(cn->sock, D, sizeof(D) - 1); */
/*         rs->hdr_found = false; */

#ifdef ENABLE_STAT_SOCK
        sendto(stat_socket, stat_start, 3, 0, &stat_addr, sizeof(stat_addr));
#endif

        process_http_request(cn);

#ifdef ENABLE_STAT_SOCK
        sendto(stat_socket, stat_stop, 3, 0, &stat_addr, sizeof(stat_addr));
#endif

#endif
    } else {
        //disconnect or real error
        if (!(r == -1 && errno == EAGAIN)) {
            conn_destroy(cn);
            return;
        }
    }
}

static void
loop(int listen_socket)
{
    struct epoll_event ev, events[64];
    int nfds, yes = 1;
    while (1) {
#if 1
        /* We don't need 2 pthread_setcanceltype() calls inside glibc epoll_wait(), so use syscall() */
        nfds = syscall(SYS_epoll_wait, epoll_fd, events, COUNTOF(events), 0);
#else
        nfds = epoll_wait(epoll_fd, events, COUNTOF(events), 0);
#endif
        for (int n = 0; n < nfds; n++) {
            if (events[n].data.ptr != NULL) {
                if (events[n].events & EPOLLIN)
                    process_read((conn_t*)events[n].data.ptr);
                if (events[n].events & EPOLLOUT)
                    process_write((conn_t*)events[n].data.ptr);
            } else {
                int s = -1;

#ifdef APP_LISTEN_IN_ONE_THREAD
                if (gthread_id == 0) {
                    s = accept4(listen_socket, NULL, NULL, SOCK_NONBLOCK);
                    if (s == -1) {
                        fprintf(stderr, "accept thr %u res = %d : %d (%s)\n", gthread_id, s, errno, strerror(errno));
                    } else {
                        int32_t conn_cnt = __sync_fetch_and_add(&live_conn_cnt, 1);
                        int t = (int)(conn_cnt / (2000 / APP_THREADS_CNT)) % APP_THREADS_CNT;
                        /* printf("%d - %d\n", conn_cnt, t); */
                        t = APP_THREADS_CNT - 1 - t;
                        if (t != 0) {
                            /* printf("transfer %d to %d\n", s, t); */
                            write(sock_transfer_pipe[t][1], &s, sizeof(s));
                            s = -1;
                        }
                    }
                } else {
                    //actually, it's not socket, it's pipe
                    read(listen_socket, &s, sizeof(s));
                    /* printf("got %d in %d\n", s, gthread_id); */
                }
#else
                s = accept4(listen_socket, NULL, NULL, SOCK_NONBLOCK);
#endif

                if (s > 0) {
                    conn_t *c = (conn_t*)memalloc_alloc(sizeof(*c));
                    /* printf("Accept %p %d\n", c, s); */
                    memset(c, 0, sizeof(*c));
                    TAILQ_INIT(&c->write_tasks);
                    c->sock = s;

                    setsockopt(s, SOL_TCP, TCP_NODELAY, &yes, sizeof(yes));
                    setsockopt(s, SOL_TCP, TCP_QUICKACK, &yes, sizeof(yes));
                    /* struct linger so_linger = {.l_onoff = 1, .l_linger = 0}; */
                    /* setsockopt(s, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)); */
                    /* int v = 256; */
                    /* setsockopt(s, SOL_SOCKET, SO_RCVBUF, &v, sizeof(v)); */

                    ev.events = EPOLLIN;
                    ev.data.ptr = c;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, s, &ev);

                    process_read(c);
                }
            }
        }
    }
}


void
net_global_init()
{
#ifdef APP_LISTEN_IN_ONE_THREAD
    for (int i = 1; i < APP_THREADS_CNT; i++) {
        if (pipe(sock_transfer_pipe[i])) {
            fprintf(stderr, "pipe create failed: %d\n", errno);
            exit(EXIT_FAILURE);
        }
    }
#endif

#ifdef ENABLE_STAT_SOCK
    stat_socket = socket(AF_INET, SOCK_DGRAM, 0);
    bzero((char *) &stat_addr, sizeof(stat_addr));
    stat_addr.sin_family = AF_INET;
    stat_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    stat_addr.sin_port = htons(7777);
#endif
}

void
net_start(uint16_t port)
{


#ifdef ENABLE_STAT_SOCK
    snprintf(stat_start, sizeof(stat_start), "+%u\n", gthread_id);
    snprintf(stat_stop, sizeof(stat_stop), "-%u\n", gthread_id);
#endif


    struct epoll_event ev;
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        fprintf(stderr, "epoll create error: %d\n", epoll_fd);
        exit(1);
    }

    ev.events = EPOLLIN;
    ev.data.ptr = NULL;

    if (gthread_id == 0
#ifndef APP_LISTEN_IN_ONE_THREAD
            || 1
#endif
        )
    {
        struct sockaddr_in listen_addr;
        bzero((char *) &listen_addr, sizeof(listen_addr));
        listen_addr.sin_family = AF_INET;
        listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        listen_addr.sin_port = htons(port);

        int listen_socket = socket(listen_addr.sin_family, (SOCK_STREAM | SOCK_NONBLOCK), 0);
        if (listen_socket == -1) {
            perror("socket");
            exit(1);
        }

        int yes = 1;
        if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
            perror("setsockopt - SO_REUSEADDR");
            exit(1);
        }

        if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) != 0) {
            perror("setsockopt - SO_REUSEPORT");
            exit(1);
        }

        /* if (setsockopt(listen_socket, SOL_TCP, TCP_DEFER_ACCEPT, &yes, sizeof(yes)) != 0) { */
        /*     perror("setsockopt - DEFER_ACCEPT"); */
        /*     exit(1); */
        /* } */

        if (bind(listen_socket, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) == -1) {
            perror("bind");
            exit(1);
        }

        if (listen(listen_socket, 4096)) {
            perror("listen");
            exit(1);
        }

        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket, &ev);
        loop(listen_socket);
    } else {
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_transfer_pipe[gthread_id][0], &ev);
        loop(sock_transfer_pipe[gthread_id][0]);
    }
}
