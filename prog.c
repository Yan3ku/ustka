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

/* macros allows being lvaues */
#define cdr(cons)  ((cons)->get.cell.cdr)
#define car(cons)  ((cons)->get.cell.car)
#define cadr(cons) car(cdr(cons))
#define cddr(cons) cdr(cdr(cons))
#define get(cons, field) ((cons)->get.atom.get.field)

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
		printf("entering list\n");
		(*cons) = alloccons(A_CELL);
		readcons(input, &car(*cons));
		readcons(input, &cdr(*cons));
		break;
	} case ')':
		printf("closing list\n");
		(*cons) = NULL;
		break;
	default: {
		ungetc(ch, input);
		if (ch != '.') {
			printf("reading atom: ");
			(*cons) = alloccons(A_CELL);
			Cons *atom = readatom(input);
			printf("%s\n", get(atom, sym));
			car(*cons) = atom;
			readcons(input, &cdr(*cons));
		} else {
			fgetc(input);
			printf("reading cdr list:\n ");
			*cons = read(input);
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
		printf("entering list\n");
		Cons *cons = alloccons(A_CELL);
		readcons(input, &cons);
		return cons;
	}
	default:{
		ungetc(ch, input);
		printf("reading atom:");
		Cons* atom = readatom(input);
		printf("%s\n", get(atom, sym));
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
		if (car(cons)) {
			int islist = (car(cons)->type == A_CELL);
			if (islist) printf("(");
			print_aux(car(cons), !cdr(cons));
			if (islist) {
				printf(")");
				if (cdr(cons)) putchar(' ');
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
