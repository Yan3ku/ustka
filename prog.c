#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "vec.h"


/* -- NAN BOXING --
   double is: [sign] 11*[exponent] 52*[mantisa] = 64 bits
   NAN is all exponent bits set + most significat bit in mantysa
   NAN = 0x7ff8000000000000
   the trick is to use the free NAN bits for storing different values */
typedef union {
	uint64_t as_uint;
	double as_double;
} Atom;				/* 8 bytes long total */

#define NANISH      0x7ffc000000000000 /* distinguish "our" NAN with one additional bit */
#define NANISH_MASK 0xffff000000000000 /* [SIGN/PTR_TAG] + 11*[EXP] + 2*[NANISH] + 2*[TAG] */

#define BOOL_MASK   0x7ffe000000000002 /* 2 ms + and 2 ls */
#define NULL_VALUE  0x7ffe000000000000 /* 0b*00 */
#define TRUE_VALUE  (BOOL_MASK | 3)    /* 0b*11 */
#define FALSE_VALUE (BOOL_MASK | 2)    /* 0b*10 */

#define INT_MASK    0x7ffc000000000000 /* use all of mantisa bits for integer */
#define SYM_MASK    0xfffc000000000000 /* pointers have sign bit set */
#define STR_MASK    0xfffe000000000000 /* on x86-64 ptr* is at max 48 bits long */
#define OBJ_MASK    0xfffd000000000000
#define PTR_MASK    0xf000000000000000

/* predicates */
#define DOUBLP(v) ((v.as_uint & NANISH) != NANISH)
#define NULLP(v)  ((v.as_uint == NULL_VALUE)
#define BOOLP(v)  ((v.as_uint & BOOL_MASK) == BOOL_MASK)
#define PTRP(v)   ((v.as_uint & PTR_MASK) == PTR_MASK)
#define INTP(v)   ((v.as_uint & NANISH_MASK) == INT_MASK)
#define STRP(v)   ((v.as_uint & NANISH_MASK) == STR_MASK)
#define SYMP(v)   ((v.as_uint & NANISH_MASK) == SYM_MASK)
#define OBJP(v)   ((v.as_uint & NANISH_MASK) == BOJ_MASK)

/* dereference value */
#define AS_DOUBL(v) (v.as_double)
#define AS_BOOL(v)  ((char)(v.as_uint & 0x1))
#define AS_INT(v)   ((int32_t)(v.as_uint))
#define AS_PTR(v)   ((char*)((v).as_uint & 0xFFFFFFFFFFFF))

/* add tag mask to malloced pointer */
#define TO_VEC(p) ((uint64_t)(p) | VEC_MASK)
#define TO_STR(p) ((uint64_t)(p) | STR_MASK)
#define TO_SYM(p) ((uint64_t)(p) | SYM_MASK)
#define TO_MAP(p) ((uint64_t)(p) | MAP_MASK)
#define TO_SET(p) ((uint64_t)(p) | SET_MASK)
#define TO_INT(i) ((uint64_t)(i) | INT_MASK)


/* -- CONS CELLS --
   Cell is holding either the Atom or Cons (construction)
   Cons is link between Cells.
*/
typedef struct Cell Cell;

typedef struct Cons {
	struct Cell *car;
	struct Cell *cdr;
} Cons;

typedef enum {
	A_CONS,
	A_ATOM,
} CellType;

struct Cell {
	union {
		Atom atom;
		Cons cons;
	} as;
	CellType type;
};

/* macros allows being lvaues */
#define cdr(c)       ((c)->as.cons.cdr)
#define car(c)       ((c)->as.cons.car)
#define cadr(c)      car(cdr(c))
#define cddr(c)      cdr(cdr(c))
#define consp(c)     ((c)->type == A_CONS)
#define atomp(c)     ((c)->type == A_ATOM)
#define as_cons(c)   ((c)->as.cons)
#define as_atom(c)   (((c)->as.atom))



/* -- TOKENIZER -- */
typedef enum {
	OK,
	UNMATCHED_RPAREN_ERR,
	DOT_FOLLOW_ERR,
	EOF_ERR,
} ReadErr;

const char *READ_ERR2STR[] = {
	[OK] = "OK",
	[UNMATCHED_RPAREN_ERR] = "unmatched )",
	[DOT_FOLLOW_ERR] = "more than one object follows . in list",
	[EOF_ERR] = "unexpected end of file",
};

#define ERR(expr) do { ReadErr err; if ((err = (expr))) return err; } while (0)


typedef struct {
	union {
		char *string;
		double doubl;
		int integer;
	} as;
	enum {
		T_LPAREN,
		T_RPAREN,
		T_RBRACKET,
		T_LBRACKET,
		T_HASH,
		T_DOT,
		T_QUOTE,
		T_BSTICK,
		T_SYMBOL,
		T_STRING,
		T_DOUBLE,
		T_INTEGER,
	} type;
} Token;

typedef struct {
	FILE *input;
	FILE *trace;
	size_t cursor;
	Token tok;
	Token unget;
	ReadErr err;
	int dounget;
} Reader;



Reader *
reader_make(FILE *input)
{
	Reader *reader = calloc(1, sizeof(Reader));
	reader->input = input;
	return reader;
}

int
reader_isterm(char chr)
{
	return (strchr(")(`'.#", chr) || isspace(chr));
}

ReadErr
reader_next(Reader *reader)
{
	char chr;
	ReadErr err = OK;
	while (isspace(chr = fgetc(reader->input)) && chr != EOF);
	switch (chr) {
	case EOF:  err = EOF_ERR; goto ERR;
	case ')':  reader->tok.type = T_RPAREN; goto OK;
        case '(':  reader->tok.type = T_LPAREN; goto OK;
        case '\'': reader->tok.type = T_QUOTE;  goto OK;
        case '`':  reader->tok.type = T_BSTICK; goto OK;
        case '.':  reader->tok.type = T_DOT;    goto OK;
        case '#':  reader->tok.type = T_HASH;   goto OK;
	case '"': {		/* todo: test string */
		int esc = 0;
		Vec(char) str;
		reader->tok.type = T_STRING;
		vec_ini(str);
		while ((chr = fgetc(reader->input)) != EOF) {
			if (chr == '\\' && !esc) break;
			esc = (chr == '\\');
			vec_push(str, chr);
		}
		if (chr == EOF) {
			vec_free(str);
			err = EOF_ERR;
			goto ERR;
		}
		vec_push(str, '\0');
		reader->tok.as.string = strdup(str);
		vec_free(str);
		goto OK;
	} default:		/* todo: add numbers */
		ungetc(chr, reader->input);
		reader->tok.type = T_SYMBOL;
		Vec(char) str;
		vec_ini(str);
		while (((chr = fgetc(reader->input)) != EOF) && !reader_isterm(chr)) {
			vec_push(str, chr);
		}
		ungetc(chr, reader->input);
		vec_push(str, '\0');
		reader->tok.as.string = strdup(str);
		vec_free(str);
		goto OK;
	}
ERR:
	reader->err = err;
	return err;
OK:
	reader->err = OK;
	return OK;
}


/* -- READ SEXP -- */
static ReadErr readcells(Reader *reader, Cell **cell); /* read construct cells */
static Cell *read(Reader *reader);                     /* main entry to read */

Cell *
alloccell(CellType type)
{
	Cell *cell = malloc(sizeof(Cell));
	cell->type = type;
	return cell;
}

void
freecell(Cell *cell) {
	if (!cell) return;
	switch (cell->type) {
	case A_CONS:
		freecell(car(cell));
		freecell(cdr(cell));
		break;
	case A_ATOM:
		if (PTRP(as_atom(cell))) free(AS_PTR(as_atom(cell)));
	}
	free(cell);
}

ReadErr
readcells(Reader *reader, Cell **cell) /* todo: implement objects and special symbols */
{
	ERR(reader_next(reader));
	switch (reader->tok.type) {
        case T_LPAREN: {
		printf("T_LAPREN readcells\n");
		*cell = alloccell(A_CONS);
	        ERR(readcells(reader, &car(*cell)));
		ERR(readcells(reader, &cdr(*cell)));
		return OK;
	} case T_RPAREN:
		printf("T_PAREN readcells\n");
		*cell = NULL;
		return OK;
	case T_DOT:
		printf("T_DOT readcells\n");
		*cell = read(reader);
		ERR(reader_next(reader));
		if (reader->tok.type != T_RPAREN) {
			reader->err = DOT_FOLLOW_ERR;
			return DOT_FOLLOW_ERR;
		}
		return OK;
	default: {
		printf("T_SYM readcells\n");
		*cell = alloccell(A_CONS);
		car(*cell) = alloccell(A_ATOM);
		as_atom(car(*cell)).as_uint = TO_SYM(reader->tok.as.string);
		printf("%s\n", AS_PTR(as_atom(car(*cell))));
		ERR(readcells(reader, &cdr(*cell)));
		return OK;
	}
	}
}


Cell *
read(Reader *reader)
{
        if (reader_next(reader)) return NULL;
	switch (reader->tok.type) { /* todo: handle all cases */
	case T_LPAREN: {
		fprintf(stderr, "T_LPAREN read\n");
		Cell *cell;
		readcells(reader, &cell);
		return cell;
	}
	case T_SYMBOL:
	case T_STRING: {
		fprintf(stderr, "T_ATOM read\n");
		Cell* cell = alloccell(A_ATOM);
		as_atom(cell).as_uint = TO_STR(reader->tok.as.string);
		return cell;
	}
	case T_RPAREN:
		reader->err = UNMATCHED_RPAREN_ERR;
		return NULL;
	}
	return NULL;
}



/* -- PRINTER -- */
void
print_aux(Cell *cell, int last)
{
	if (!cell) return;
	switch (cell->type) {
	case A_ATOM:
		fputs(AS_PTR(as_atom(cell)), stdout);
		if (!last) putchar(' ');
		break;
	case A_CONS: {
		if (consp(car(cell))) printf("(");
		print_aux(car(cell), !cdr(cell));
		if (consp(car(cell))) {
			printf(")");
			if (cdr(cell)) putchar(' ');
		}
		if (cdr(cell) && atomp(cdr(cell))) {
			fputs(". ", stdout);
		}
		print_aux(cdr(cell), !!cdr(cell));
	}
	}
}

void
print(Cell *cell) {
	if (!cell) return;
	switch (cell->type) {
	case A_CONS:
		putchar('(');
		print_aux(cell, 0);
		putchar(')');
		break;
	case A_ATOM:
		print_aux(cell, 0);
	}
}

int
main(int argc, char *argv[])
{
	#if 0
	FILE *file = fopen(argv[1], "r");
	char ch[SYM_LEN];
	if (!file) {
		printf("doesnt work file");
		exit(1);
	}
	#endif
	Reader *reader;
	Cell *cell;
	print(cell = read(reader = reader_make(stdin)));
	freecell(cell);
	if (reader->err) {
		puts(READ_ERR2STR[reader->err]);
	}
	free(reader);
}
