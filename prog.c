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

int loglevel = 1;

/* macros allows being lvaues */
#define cdr(c)  ((c)->get.cell.cdr)
#define car(c)    ((c)->get.cell.car)
#define cadr(c)   car(cdr(c))
#define cddr(c)   cdr(cdr(c))
#define get(c, t) ((c)->get.atom.get.t)
#define islist(c) (car(c)->type == A_CELL)


/* show trace how read is parsing sexps */
#define NDEBUG
#ifdef NDEBUG
#define LOC_() fprintf(stderr, "[%s@%-10s:%-3d] ", __FILE__, __func__, __LINE__)
#define INFO2(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#define INFOLN2(...) do { INFO2(__VA_ARGS__); INFO2("\n"); } while (0)
#define INFO(...)  do { LOC_(); INFO2(__VA_ARGS__) } while (0)
#define INFOLN(...) do { INFO(__VA_ARGS__); INFO2("\n"); } while (0)
#define INFODEPTH(depth, ...) do {					\
		LOC_();                                                 \
		for (int i = 0; i < depth; i++) fputc('>', stderr);	\
		fputc(' ', stderr);					\
		INFO2(__VA_ARGS__);                                     \
} while (0)
#define INFODEPTHLN(depth, ...) do { INFODEPTH(depth, __VA_ARGS__); INFO2("\n"); } while (0)
#else
#define INFO(...)
#define INFOLN(...)
#define INFODEPTH(...)
#endif




static void readcons(FILE *input, Cons **cons, int depth); /* read construct conses */
static Cons *readatom(FILE *input);
static Cons *read_(FILE *input, int depth);			/* main entry to read */

Cons *
alloccons(ConsType type)
{
	Cons *cons = malloc(sizeof(Cons));
	cons->type = type;
	return cons;
}

void
readcons(FILE *input, Cons **cons, int depth)
{
	char ch;
       	while (isblank(ch = fgetc(input)) && ch != EOF);
	if (feof(input)) return; /* todo: signal error */
	switch (ch) {
        case '(': {
		INFODEPTHLN(depth, "OPENING");
		(*cons) = alloccons(A_CELL);
		readcons(input, &car(*cons), depth + 1);
		readcons(input, &cdr(*cons), depth);
		break;
	} case ')':
		INFODEPTHLN(depth, "CLOSING");
		(*cons) = NULL;
		break;
	default: {
		ungetc(ch, input);
		if (ch != '.') {
			INFODEPTH(depth, "ATOM :: ");
			(*cons) = alloccons(A_CELL);
			Cons *atom = readatom(input);
			INFOLN2("%s", get(atom, sym));
			car(*cons) = atom;
			readcons(input, &cdr(*cons), depth);
		} else {
			INFODEPTHLN(depth, ":: DOT ::");
			fgetc(input);
			*cons = read_(input, depth);
			while (isblank(ch = fgetc(input)) && ch != EOF);
			if (ch != ')') {
				INFODEPTHLN(depth, "ERROR: expected left paren");
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
read_(FILE *input, int depth)
{
	char ch;
	while (isblank(ch = fgetc(input)) && ch != EOF);
	switch (ch) {
	case '(': {
		INFODEPTHLN(depth, "OPENING");
		Cons *cons = alloccons(A_CELL);
		readcons(input, &cons, depth + 1);
		INFODEPTHLN(depth, "CLOSING");
		return cons;
	}
	default:{
		ungetc(ch, input);
		INFODEPTH(depth, "ATOM :: ");
		Cons* atom = readatom(input);
		INFOLN2("%s", get(atom, sym));
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
		if (islist(cons)) printf("(");
		print_aux(car(cons), !cdr(cons));
		if (islist(cons)) {
			printf(")");
			if (cdr(cons)) putchar(' ');
		}
		if (cdr(cons) && cdr(cons)->type == A_ATOM) {
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
	print(read_(file, 0));
}
