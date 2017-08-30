#pragma once
#include <stdlib.h>


#if 1
#define memalloc_alloc(a) real_memalloc_alloc(a)
#define memalloc_free(a) real_memalloc_free(a)
#define memalloc_init() real_memalloc_init()
#else
#define memalloc_alloc(a) malloc(a)
#define memalloc_free(a) free(a)
#define memalloc_init()
#endif

void *real_memalloc_alloc(size_t size);
void real_memalloc_free(void *ptr);
void real_memalloc_init();
