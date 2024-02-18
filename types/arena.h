/* Resizing arena implementation */
/*
#include "aux.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdalign.h>
*/

typedef struct Arena {
	char *reg;
	char *ptr;
	ptrdiff_t siz;
	struct Arena *cdr;
} Arena;

#define aini() ainit(4096)
static inline Arena *
ainit(size_t siz) {
	Arena *a = (Arena *)malloc(sizeof(Arena));
	a->reg = (char *)calloc(siz, 1);
	a->siz = siz;
	a->ptr = a->reg;
	a->cdr = NULL;
	return a;
}

static inline void
deinit(Arena *a)
{
	Arena *cdr, *car = a;
	do {
		cdr = car->cdr;
		free(car->reg);
		free(car);
		car = cdr;
	} while (cdr);
}

static inline void *
new(Arena *a, ptrdiff_t siz) {
	Arena **cdr = &a;
        siz = align(siz);
	do {
		if ((*cdr)->reg + (*cdr)->siz - (*cdr)->ptr >= siz) {
			(*cdr)->ptr += siz;
			return (*cdr)->ptr - siz;
		}
		cdr = &(*cdr)->cdr;
	} while (*cdr);
	*cdr = ainit(max(siz, a->siz));
	(*cdr)->ptr += siz;
	return (*cdr)->reg;
}
