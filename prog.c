#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define SYM_LEN 24

typedef struct {
	union {
		char sym[SYM_LEN];
	} get;
	enum {
		A_SYMBOL,
	} type;
} Atom;

typedef struct Cons Cons;

typedef struct Cell {
	struct Cons *car;
	struct Cons *cdr;
} Cell;

struct Cons {
	union {
		Atom atom;
		Cell cell;
	} get;
	enum {
		A_CELL,
		A_ATOM,
	} type;
};

inline static Cons *cdr(Cons *cons) { return cons->get.cell.cdr; }
inline static Cons *car(Cons *cons) { return cons->get.cell.car; }
inline static Cons *cadr(Cons *cons) { return car(cdr(cons)); }
inline static Cons *cddr(Cons *cons) { return cdr(cdr(cons)); }

static void readcons(FILE *input, Cell *cell); /* read construct conses */
static Cons *readatom(FILE *input);
static Cons *read(FILE *input);			/* main entry to read */

void
readcons(FILE *input, Cell *cell)
{
	char ch;
       	while (isblank(ch = fgetc(input)) && ch != EOF);
	if (feof(input)) return; /* todo: signal error */
	switch (ch) {
        case '(': {		/* not really sure what im suuposed to do with car */
		printf("entering list\n");
		cell->car = malloc(sizeof(Cons));
		cell->cdr = malloc(sizeof(Cons));
		cell->car->type = A_CELL;
		readcons(input, &cell->car->get.cell);
		readcons(input, &cell->cdr->get.cell);
		break;
	} case ')':
		printf("closing list\n");
		cell->car = cell->cdr = NULL;
		break;
	default: {
		ungetc(ch, input);
		if (ch != '.') {
			printf("reading atom: ");
			Cons *atom = readatom(input);
			printf("%s\n", atom->get.atom.get.sym);
			cell->car = atom;
			Cons *new = malloc(sizeof(Cons));
			cell->cdr = new;
			readcons(input, &cell->cdr->get.cell);
		} else {
			fgetc(input);
			printf("reading cdr list:\n ");
			cell->
			cell->cdr = read(input);
			while (isblank(ch = fgetc(input)) && ch != EOF);
			if (ch != ')') {
				printf("error: expected )\n");
			}
		}
		break;
	}
	}
}

void				/* this function werks fine, tested on input */
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
	Cons *new = malloc(sizeof(Cons));
	new->type = A_ATOM;
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
		printf("entering list\n");
		Cons *cons = malloc(sizeof(Cons));
		cons->type = A_CELL;
		readcons(input, &cons->get.cell);
		return cons;
	}
	default:{
		ungetc(ch, input);
		printf("reading atom:");
		Cons* atom = readatom(input);
		printf("%s\n", atom->get.atom.get.sym);
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
		fputs(cons->get.atom.get.sym, stdout);
		if (!last) putchar(' ');
		break;
	case A_CELL: {
		if (car(cons)) {
			int islist = (car(cons)->type == A_CELL);
			if (islist) printf("(");
			print_aux(car(cons), !cadr(cons));
			if (islist) {
				printf(")");
				if (cadr(cons)) putchar(' ');
			}
		}
		if (cdr(cons) && cdr(cons)->type == A_ATOM) {
			fputs(". ", stdout);
			last = 1;
		} else last = 0;
		print_aux(cdr(cons), last);
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
