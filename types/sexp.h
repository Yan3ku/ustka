#ifndef SEXP_H
#define SEXP_H
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "arena.h"

#define nil NULL

/* atom cells don't have special mask so they can be dereferenced normaly */
#define CELL_MASK (1UL << 63)
#define A_CONS    CELL_MASK
#define A_ATOM    0

/* probably good idea to clear tag? it will break anyway idk about that */
#define CELL_CL_TAG(p) ((uint64_t)(p) & ~CELL_MASK)
#define CELL(p)        ((Cell*)((uint64_t)(p) & 0xFFFFFFFFFFFF))
#define CONSP(p)       (p && ((uint64_t)p & CELL_MASK) == A_CONS)
#define ATOMP(p)       (p && ((uint64_t)p & CELL_MASK) == A_ATOM)
#define TO_CONS(p)     ((void*)(CELL_CL_TAG(p) | A_CONS))
#define TO_ATOM(p)     ((void*)(CELL_CL_TAG(p) | A_ATOM))
#define CELL_AT(p)     (CELL(p)->loc_.at) /* cell location in file */
#define CELL_LEN(p)    (CELL(p)->loc_.len) /* cell lenght in file */
#define CELL_LOC(p)    (CELL(p)->loc_)
#define CAR(p)         (CELL(p)->car_)
#define CDR(p)         (CELL(p)->cdr_)

#define RANGEFMT "<%lu|%lu>"
#define RANGEP(range) range.at, range.len

typedef enum {
	A_SYM,
	A_STR,
	A_INT,
	A_DOUBL,
	A_VEC,
} AtomVar;

typedef struct {
	size_t at;
	size_t len;
} Range;

typedef struct Cell {
	Range loc_;
	union {
		struct {
			struct Cell *car_; /* don't use these directly */
			struct Cell *cdr_; /* they have upper bits set to type */
		};
		struct {
			AtomVar type;
			union {	/* literals */
				struct Cell *vec;
				char *string;
				double doubl;
				int integer;
			};
		};
	};
} Cell;				/* 32 bits */

typedef struct {
	const char *fname;
	Arena *arena;
	Cell *cell;
} Sexp;

static inline Cell *
cellof(Arena *arena, uint64_t mask, size_t at) {
	Cell *cell = (Cell *)(CELL_CL_TAG(new(arena, sizeof(Cell))) | mask);
	CELL_AT(cell) = at;
	CELL_LEN(cell) = SIZE_MAX;
	return cell;
}

static inline Cell *
cons(Arena *arena, Cell *a, Cell *b, size_t at)
{
	Cell *cell = cellof(arena, A_CONS, at);
	CAR(cell) = a;
	CDR(cell) = b;
	return cell;
}

#endif
