#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

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
	} get;
	enum { ATOM_TYPES(A) } type;
} Atom;

typedef struct Cons Cons;

typedef struct Cell {
	struct Cons *car;
	struct Cons *cdr;
} Cell;

typedef enum {
	A_CELL,
	A_ATOM,
} ConsType;


struct Cons {
	union {
		Atom atom;
		Cell cell;
	} get;
	ConsType type;
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
#define cdr(c)    ((c)->get.cell.cdr)
#define car(c)    ((c)->get.cell.car)
#define cadr(c)   car(cdr(c))
#define cddr(c)   cdr(cdr(c))
#define get(c, t) ((c)->get.atom.get.t)
#define listp(c)  ((c)->type == A_CELL)
#define atomp(c)  ((c)->type == A_ATOM)


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

static void readcons(FILE *input, Cons **cons); /* read construct conses */
static Cons *readatom(FILE *input);
static Cons *read(FILE *input);			/* main entry to read */

Cons *
alloccons(ConsType type)
{
	Cons *cons = malloc(sizeof(Cons));
	cons->type = type;
	return cons;
}

void
readcons(FILE *input, Cons **cons)
{
	char ch;
       	while (isblank(ch = fgetc(input)) && ch != EOF);
	if (feof(input)) return; /* todo: signal error */
	switch (ch) {
        case '(': {
		(*cons) = alloccons(A_CELL);
		readcons(input, &car(*cons));
		readcons(input, &cdr(*cons));
		break;
	} case ')':
		(*cons) = NULL;
		break;
	default: {
		ungetc(ch, input);
		if (ch != '.') {
			(*cons) = alloccons(A_CELL);
			Cons *atom = readatom(input);
			car(*cons) = atom;
			readcons(input, &cdr(*cons));
		} else {
			fgetc(input);
			*cons = read(input);
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

Cons *
readatom(FILE *input)
{
	Cons *new = alloccons(A_ATOM);
	readsym(input, new->get.atom.get.sym); /* for now the only atoms are symbols */
	return new;
}

Cons *
read(FILE *input)
{
	char ch;
	while (isblank(ch = fgetc(input)) && ch != EOF);
	switch (ch) {
	case '(': {
		Cons *cons = alloccons(A_CELL);
		readcons(input, &cons);
		return cons;
	}
	default:{
		ungetc(ch, input);
		Cons* atom = readatom(input);
		return atom;
	}
	}
}

void
print_aux(Cons *cons, int last)
{
	if (!cons) return;
	switch (cons->type) {
	case A_ATOM:
		fputs(get(cons, sym), stdout);
		if (!last) putchar(' ');
		break;
	case A_CELL: {
		if (listp(car(cons))) printf("(");
		print_aux(car(cons), !cdr(cons));
		if (listp(car(cons))) {
			printf(")");
			if (cdr(cons)) putchar(' ');
		}
		if (cdr(cons) && atomp(cdr(cons))) {
			fputs(". ", stdout);
		}
		print_aux(cdr(cons), !!cdr(cons));
	}}
}

void
print(Cons *cons) {
	if (!cons) return;
	switch (cons->type) {
	case A_CELL:
		putchar('(');
		print_aux(cons, 0);
		putchar(')');
		break;
	case A_ATOM:
		print_aux(cons, 0);
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
