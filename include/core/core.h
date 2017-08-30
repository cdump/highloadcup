#pragma once

#include "memalloc.h"

extern __thread int gthread_id;

#define clock_diff(start, stop) \
	((stop.tv_nsec < start.tv_nsec) \
	? ((stop.tv_sec - start.tv_sec - 1) * 1000000 + (1000000000 + stop.tv_nsec - start.tv_nsec) / 1000) \
	: ((stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define COUNTOF(v) (sizeof(v) / sizeof(*v))


//return len
static inline int
long2str(char *buf, long n)
{
    char tmp[22];/* This is a stack.
                    64/Log_2[10] = 19.3, so this is enough forever...*/
    unsigned long u;
    char *bufptr=buf, *tmpptr=tmp+1;

    int len = 0;
    if (n < 0){
        /*INT_MIN workaround:*/
        u = ((unsigned long)(-(1+n))) + 1;
        *bufptr++='-';
        len = 1;
    } else {
        u = n;
    }

    *tmp='\0';/*terminator*/

    /*Fill up the stack:*/
    do {
        *tmpptr++ = '0' + (u % 10);
    } while (u/=10);
    len += tmpptr - tmp - 1;

    /*Copy the stack to the output buffer:*/
    while((*bufptr++ = *--tmpptr) != '\0');
    return len;
}

#include <inttypes.h>
// #include <stdio.h>
static inline void
float2str(uint32_t num, uint32_t denum, char *buf)
{
    // printf("float = %f\n", 1.0 * num / denum);
    uint64_t a = num * 1000000 / denum;
    uint64_t i = a / 1000000;
    uint64_t d = (a % 1000000 + 5) / 10;
    int l = long2str(buf, i);
    buf[l++] = '.';
    if (d == 0) {
        buf[l] = '0';
    } else  {
        for (uint64_t j = d; j < 10000; j *= 10)
            buf[l++] = '0';
        l += long2str(&buf[l], d);
        buf[l] = ' ';
    }
    // printf("%lu : %lu . %lu\n", a, i, d);
}

#define APPEND_CONST_STRING(BUF, OFF, STR) do { \
        memcpy(BUF + OFF, STR, sizeof(STR) - 1);    \
        OFF += sizeof(STR) - 1;                     \
} while(0)

#define APPEND_BUF(BUF, OFF, DATA, LEN) do { \
    memcpy(BUF + OFF, DATA, LEN);            \
    OFF += LEN;                              \
} while(0)

#define APPEND_STRING(BUF, OFF, STR) do { \
    char *p = STR;                        \
    while (*p != '\0') {                  \
        (BUF)[OFF] = *p;                  \
        (OFF)++;                          \
        p++;                              \
    }                                     \
} while(0)

#define APPEND_INT(BUF, OFF, NUM) do { \
    (OFF) += long2str(BUF + OFF, NUM); \
} while(0)
