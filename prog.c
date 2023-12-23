#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>

#define SYM_LEN 24


#define ATOM_TYPES(T) T##_SYMBOL, T##_KEYWORD, T##_STRING, T##_INTEGER, T##_FLOAT, \
                      T##_REGEX, T##_CHAR, T##_BOOL, T##_VEC, T##_MAP, T##_SET

typedef struct {
	union {
		char sym[SYM_LEN];
		char *string;
		int integer;
		double floating;
		char chr;
		int bool;
	} as;
	enum { ATOM_TYPES(A) } type;
} Atom;

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

typedef enum {
	C_OK,
	C_UNMATCHED_LPAREN,
	C_DOT_FOLLOW,
} ConsErr;

typedef struct {
	FILE *input;
	size_t cursor;
	ConsErr err;
} Reader;

typedef enum {
	ATOM_TYPES(T),
	T_LPAREN,
	T_RPAREN,
	T_RBRACKET,
	T_LBRACKET,
	T_QUOTE,
	T_BSTICK,
	T_HASH,
} Token;

/* macros allows being lvaues */
#define cdr(c)       ((c)->as.cons.cdr)
#define car(c)       ((c)->as.cons.car)
#define cadr(c)      car(cdr(c))
#define cddr(c)      cdr(cdr(c))
#define consp(c)     ((c)->type == A_CONS)
#define atomp(c)     ((c)->type == A_ATOM)
#define asatom(c, t) ((c)->as.atom.as.t)



void
reader_next()
{

}

Token
reader_peek()
{

}

Token
reader_unget()
{

}

static void readcells(FILE *input, Cell **cell); /* read construct conses */
static Cell *readatom(FILE *input);
static Cell *read(FILE *input);			/* main entry to read */

Cell *
alloccell(CellType type)
{
	Cell *cell = malloc(sizeof(Cell));
	cell->type = type;
	return cell;
}

void
readcells(FILE *input, Cell **cell)
{
	char ch;
       	while (isblank(ch = fgetc(input)) && ch != EOF);
	if (feof(input)) return; /* todo: signal error */
	switch (ch) {
        case '(': {
		(*cell) = alloccell(A_CONS);
		readcells(input, &car(*cell));
		readcells(input, &cdr(*cell));
		break;
	} case ')':
		(*cell) = NULL;
		break;
	default: {
		ungetc(ch, input);
		if (ch != '.') {
			(*cell) = alloccell(A_CONS);
			Cell *atom = readatom(input);
			car(*cell) = atom;
			readcells(input, &cdr(*cell));
		} else {
			fgetc(input);
			*cell = read(input);
			while (isblank(ch = fgetc(input)) && ch != EOF);
			if (ch != ')') {
				/* todo error */
			}
		}
		break;
	}
	}
}

void
readsym(FILE *input, char *sym) {
	char ch, *p;
	for (p = sym; p - sym < SYM_LEN && !isblank(ch = fgetc(input)) &&
		     ch != ')' && ch != EOF; p++) {
		(*p) = ch;
	}
	if (p - sym == 0) return; /* todo: signal error */
	(*p) = 0;
	ungetc(ch, input);
}

Cell *
readatom(FILE *input)
{
	Cell *new = alloccell(A_ATOM);
	readsym(input, asatom(new, sym)); /* for now the only atoms are symbols */
	return new;
}

Cell *
read(FILE *input)
{
	char ch;
	while (isblank(ch = fgetc(input)) && ch != EOF);
	switch (ch) {
	case '(': {
		Cell *cell = alloccell(A_CONS);
		readcells(input, &cell);
		return cell;
	}
	default:{
		ungetc(ch, input);
		Cell* atom = readatom(input);
		return atom;
	}
	}
}

void
print_aux(Cell *cell, int last)
{
	if (!cell) return;
	switch (cell->type) {
	case A_ATOM:
		fputs(asatom(cell, sym), stdout);
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
	}}
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
	FILE *file = fopen(argv[1], "r");
	char ch[SYM_LEN];
	if (!file) {
		printf("doesnt work file");
		exit(1);
	}
	print(read(file));
}
