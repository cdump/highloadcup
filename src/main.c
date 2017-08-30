#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/mman.h>

#include "app.h"

#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench.h"
#include "log.h"
#include "core.h"

#include "api.h"
#include "httpapi.h"
#include "network.h"

__thread int gthread_id = 0;

static void*
thread_routine(void *arg)
{
    gthread_id = (uint64_t)arg;
#if 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(gthread_id, &cpuset);
    int s = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        printf("pthread_setaffinity_np failed\n");
    }
#endif

    net_start(APP_HTTP_LISTEN_PORT);
    return NULL;
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
static void
test_http()
{
    conn_t cn;
    TAILQ_INIT(&cn.write_tasks);

    int a = open("/dev/null", O_RDWR);

    cn.sock = a;
#define A "POST /visits/1 HTTP/1.1\r\nContent-Length: 13\r\n\r\n{\"user\": \"1\"}"

    int cnt = 1000000;
    BENCH_BEGIN();
    for (int i = 0; i < cnt; i++) {
    /* while(1) { */
        cn.read.method = HTTP_POST;
        cn.read.data_size = sizeof(A) - 1;
        cn.read.data_off = 0;
        cn.read.header_size = sizeof("POST /visits/1 HTTP/1.1\r\nContent-Length: 13\r\n\r\n") - 1;
        memcpy(cn.read.data, A, sizeof(A));
        if (i%2) {
            cn.read.data[cn.read.header_size + 10] = '2';
        }
        process_http_request(&cn);
    }
    BENCH_END();
}


int
main(int argc, char **argv)
{
    int mr = mlockall(MCL_CURRENT | MCL_FUTURE);
    fprintf(stderr, "mlockall res = %d : %d (%s)\n", mr, errno, strerror(errno));

	struct rlimit l;
	l.rlim_cur = 4096;
    l.rlim_max = 4096;
	if (setrlimit(RLIMIT_NOFILE, &l) < 0) {
        fprintf(stderr, "setrlimit failed: %d (%s)\n", errno, strerror(errno));
    }

    signal(SIGPIPE, SIG_IGN);

    memalloc_init();

    /* int nr = nice(-20); */
    /* fprintf(stderr, "nice res = %d\n", nr); */

    /* struct sched_param sp; */
    /* sp.sched_priority = 99; */
    /* int rs = sched_setscheduler(0, SCHED_RR, &sp); */
    /* fprintf(stderr, "sched = %d\n", rs); */

    api_init();

    /* test_http(); exit(0); */

    net_global_init();

    pthread_t threads[APP_THREADS_CNT];
    for (int i = 1; i < APP_THREADS_CNT; i++) {
        if (pthread_create(&threads[i], NULL, thread_routine, (void *)(uint64_t)i)) {
            fprintf(stderr, "can't create thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
    thread_routine(0);

    return 0;
}
