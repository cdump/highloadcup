#include "memalloc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "core.h"
#include "queue.h"


#define COUNT 4096
#define PAGESIZE 4096

#define COUNT_1mb 16


struct memelement {
    char *buf;
    LIST_ENTRY(memelement) link;
};
LIST_HEAD(memelement_head, memelement);

#define THR __thread
/* #define THR */

static THR void *global_buffer;
static THR void *global_buffer_end;
static THR void *global_buffer_1mb;
static THR void *global_buffer_1mb_end;

static THR struct memelement *elements;
static THR struct memelement *elements_1mb;

static THR struct memelement_head memelement_list;
static THR struct memelement_head memelement_1mb_list;

void
real_memalloc_init()
{
    elements = malloc(sizeof(struct memelement) * COUNT);
    elements_1mb = malloc(sizeof(struct memelement) * COUNT_1mb);

    global_buffer = mmap(NULL, COUNT * PAGESIZE, PROT_READ | PROT_WRITE,  MAP_PRIVATE  | MAP_ANONYMOUS | MAP_POPULATE, 0, 0);
    global_buffer_end = global_buffer + COUNT * PAGESIZE;

    global_buffer_1mb = mmap(NULL, COUNT_1mb * (1024*1024LLU), PROT_READ | PROT_WRITE,  MAP_PRIVATE  | MAP_ANONYMOUS | MAP_POPULATE, 0, 0);
    global_buffer_1mb_end = global_buffer_1mb + COUNT_1mb * (1024*1024LLU);

    if (global_buffer == MAP_FAILED || global_buffer_1mb == MAP_FAILED) {
        fprintf(stderr, "mmap failed\n");
        exit(1);
    }

    LIST_INIT(&memelement_list);
    LIST_INIT(&memelement_1mb_list);

	for (int i = 0; i < COUNT; i++) {
        struct memelement *el = &elements[i];
        el->buf = global_buffer + PAGESIZE * i;
        LIST_INSERT_HEAD(&memelement_list, el, link);
    }

	for (int i = 0; i < COUNT_1mb; i++) {
        struct memelement *el = &elements_1mb[i];
        el->buf = global_buffer_1mb + (1024*1024LLU) * i;
        LIST_INSERT_HEAD(&memelement_1mb_list, el, link);
    }
}

void *
real_memalloc_alloc(size_t size)
{
    if (size <= 4096) {
        struct memelement *x = memelement_list.lh_first;
        if (x) {
            void *b = x->buf;
            /* printf("alloc %lu: %p | %p from list\n", size, x, b); */
            LIST_REMOVE(x, link);
            return b;
        }
    } else if (size < (1024*1024LLU)) {
        struct memelement *x = memelement_1mb_list.lh_first;
        if (x) {
            void *b = x->buf;
            /* printf("alloc %lu: %p | %p from 1mb list\n", size, x, b); */
            LIST_REMOVE(x, link);
            return b;
        }
    }
    /* fprintf(stderr, "alloc malloc %lu\n", size); */
    /* exit(1); */

    return malloc(size);
}

void
real_memalloc_free(void *ptr)
{
    if (ptr == NULL)
        return;
    if (global_buffer <= ptr && ptr < global_buffer_end) {
        assert((ptr - global_buffer) % 4096 == 0);
        int c = (ptr - global_buffer) / 4096;
        struct memelement *el = &elements[c];
        /* printf("free %p | %p from list (%p)\n", el, el->buf, ptr); */
        LIST_INSERT_HEAD(&memelement_list, el, link);
        return;
    }
    if (global_buffer_1mb <= ptr && ptr < global_buffer_1mb_end) {
        assert((ptr - global_buffer_1mb) % (1024*1024LLU) == 0);
        int c = (ptr - global_buffer_1mb) / (1024*1024LLU);
        struct memelement *el = &elements_1mb[c];
        /* printf("free %p | %p from list 1mb (%p)\n", el, el->buf, ptr); */
        LIST_INSERT_HEAD(&memelement_1mb_list, el, link);
        return;
    }
    /* printf("free %p free\n", ptr); */
    free(ptr);
}
