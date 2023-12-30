#ifndef CELL_H
#define CELL_H
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "arena.h"

#define nil NULL

#define CELL_MASK (1UL << 63)
#define A_CONS    CELL_MASK
#define A_ATOM    0

#define CELL_CL_TAG(p) ((uint64_t)(p) & ~CELL_MASK)
#define CELL_AS_PTR(p) ((Cell*)((uint64_t)(p) & 0xFFFFFFFFFFFF))
#define CONSP(p)       (p && ((uint64_t)p & CELL_MASK) == A_CONS)
#define ATOMP(p)       (p && ((uint64_t)p & CELL_MASK) == A_ATOM)
#define TO_CONS(p)     ((void*)(CELL_CL_TAG(p) | A_CONS))
#define TO_ATOM(p)     ((void*)(CELL_CL_TAG(p) | A_ATOM))
#define CAR(p)         (CELL_AS_PTR(p)->car_)
#define CDR(p)         (CELL_AS_PTR(p)->cdr_)

typedef enum {
	A_SYM,
	A_STR,
	A_INT,
	A_DOUBL,
	A_VEC,
} AtomVar;

typedef struct Cell {
	union {
		struct {
			struct Cell *car_; /* don't use these directly */
			struct Cell *cdr_; /* they have upper bits set to type */
		};
		struct {
			AtomVar type;
			union {
				struct Cell *vec;
				char *string;
				double doubl;
				int integer;
			};
		};
	};
} Cell;

static inline Cell *
cellof(Arena *arena, uint64_t mask) {
	return (Cell *)((uint64_t)new(arena, sizeof(Cell)) | mask);
}

static inline Cell *
cons(Arena *arena, Cell *a, Cell *b)
{
	Cell *cell = cellof(arena, A_CONS);
	CAR(cell) = a;
	CDR(cell) = b;
	return cell;
}

#endif
