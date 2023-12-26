#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include "vec.h"


/* -- CONS CELLS --
   Cell is holding either the Atom or Cons (construction)
   Cons is link between Cells.
*/
#define nil (void *)0

typedef struct {
	enum {
		A_SYMBOL,
		A_STRING,
		A_DOUBLE,
		A_INTEGER,
	} type;
	union {
		char *string;
		double doubl;
		int integer;
	} as;
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

#define cdr(c)       ((c)->as.cons.cdr)
#define car(c)       ((c)->as.cons.car)
#define cadr(c)      car(cdr(c))
#define cddr(c)      cdr(cdr(c))
#define consp(c)     ((c)->type == A_CONS)
#define atomp(c)     ((c)->type == A_ATOM)
#define nilp(c)      (!(c))
#define lisp(c)      (nilp(c) || consp(c))

#define consof(c)    ((c)->as.cons)
#define atomof(c)    ((c)->as.atom)

#define intp(c)      (atomp(c) && atomof(c).type == A_INTEGER)
#define doublep(c)   (atomp(c) && atomof(c).type == A_DOUBLE)
#define strp(c)      (atomp(c) && atomof(c).type == A_STRING)
#define symp(c)      (atomp(c) && atomof(c).type == A_SYMBOL)

#define intof(c)     (atomof(c).as.integer)
#define doublof(c)   (atomof(c).as.doubl)
#define symof(c)     (atomof(c).as.string)
#define strof(c)     (atomof(c).as.string)


/* -- TOKENIZER -- */
typedef enum {
	OK,
	EOF_ERR,
	DOT_FOLLOW_ERR,
	TO_MANY_DOTS_ERR,
	UNMATCHED_RPAREN_ERR,
} ReadErr;

const char *READ_ERR2STR[] = {
	[OK]                   = "OK",
	[EOF_ERR]              = "unexpected end of file",
	[DOT_FOLLOW_ERR]       = "more than one object follows . in list",
	[TO_MANY_DOTS_ERR]     = "to many dots",
	[UNMATCHED_RPAREN_ERR] = "unmatched close parenthesis",
};

typedef struct {
	int heapval;
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
	size_t cursor;
	Token tok;
	ReadErr err;
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
	reader->tok.heapval = 0;
	reader->err = OK;
	while (isspace(chr = fgetc(reader->input)) && chr != EOF);
	switch (chr) {
	case EOF:  reader->err = EOF_ERR;         return reader->err;
	case ')':  reader->tok.type = T_RPAREN;   return OK;
        case '(':  reader->tok.type = T_LPAREN;   return OK;
	case '[':  reader->tok.type = T_RBRACKET; return OK;
        case ']':  reader->tok.type = T_LBRACKET; return OK;
        case '\'': reader->tok.type = T_QUOTE;    return OK;
        case '`':  reader->tok.type = T_BSTICK;   return OK;
        case '.':  reader->tok.type = T_DOT;      return OK;
        case '#':  reader->tok.type = T_HASH;     return OK;
	case '"': {		/* todo: test string */
		int esc = 0;
		Vec(char) str;
		reader->tok.type = T_STRING;
		vec_ini(str);
		while ((chr = fgetc(reader->input)) != EOF) {
			if (esc) {
				esc = 0;
				switch (chr) {
				case '\\': vec_push(str, chr); continue;
				case '"': vec_push(str, chr);  continue;
				case 'n': vec_push(str, '\n'); continue;
				case 't': vec_push(str, '\t'); continue;
				}
			} else if (chr == '"') break;
			if ((esc = (chr == '\\'))) continue;
			vec_push(str, chr);
		}
		if (chr == EOF) {
			vec_free(str);
			reader->err = EOF_ERR;
			return reader->err;
		}
		vec_push(str, '\0');
		reader->tok.heapval = 1;
		reader->tok.as.string = strdup(str);
		vec_free(str);
		return OK;
	} default: {		/* todo: add double */
		ungetc(chr, reader->input);
		Vec(char) str;
		vec_ini(str);
		char *endptr = str;
		while (((chr = fgetc(reader->input)) != EOF) && !reader_isterm(chr)) {
			vec_push(str, chr);
		}
		ungetc(chr, reader->input);
		vec_push(str, '\0');
		reader->tok.as.integer = strtol(str, &endptr, 10);
		if (*endptr == '\0') {
			vec_free(str);
			reader->tok.type = T_INTEGER;
			return OK;

		}
		reader->tok.type = T_SYMBOL;
		reader->tok.as.string = strdup(str);
		reader->tok.heapval = 1;
		vec_free(str);
		return OK;
	}
	}
}


/* -- READ SEXP -- */
static ReadErr readcells(Reader *reader, Cell **cell); /* read construct cells */
static Cell *read(Reader *reader);                     /* main entry to read */

Cell *
cellalloc(CellType type)
{
	Cell *cell = malloc(sizeof(Cell));
	cell->type = type;
	if (type == A_CONS) car(cell) = cdr(cell) = nil;
	return cell;
}

Cell *
cellcopy(Cell *cell)
{
	Cell *copy = malloc(sizeof(Cell));
	memcpy(copy, cell, sizeof(Cell));
	if (!intp(cell) && ((strp(cell)) || symp(cell))) {
		strof(copy) = malloc(strlen(strof(cell)) + 1);
		strcpy(strof(copy), strof(cell));
	}
	return copy;
}

void
freecell(Cell **cell) {
	if (nilp(*cell)) return;
	switch ((*cell)->type) {
	case A_CONS:
		freecell(&car(*cell));
		freecell(&cdr(*cell));
		break;
	case A_ATOM:
		if (strp(*cell) || symp(*cell)) free(strof(*cell));
	}
	free(*cell);
	*cell = nil;
}

ReadErr
readcells(Reader *reader, Cell **cell) /* todo: implement all tokens */
{
	ReadErr err;
	if ((err = reader_next(reader))) return err;
	switch (reader->tok.type) {
        case T_LPAREN: {
		fprintf(stderr, "T_LAPREN readcells\n");
		*cell = cellalloc(A_CONS);
	        if ((err = readcells(reader, &car(*cell)))) goto exit;
		if ((err = readcells(reader, &cdr(*cell)))) goto exit;
		return OK;
	} case T_RPAREN:
		fprintf(stderr, "T_PAREN readcells\n");
		*cell = nil;
		return OK;
	case T_DOT:
		fprintf(stderr, "T_DOT readcells\n");
		if (!(*cell = read(reader))) return reader->err;
		if ((err = reader_next(reader))) goto exit;
		if (reader->tok.type != T_RPAREN) {
			if (reader->tok.heapval) free(reader->tok.as.string);
			reader->err = DOT_FOLLOW_ERR;
			return DOT_FOLLOW_ERR;
		}
		return OK;
	case T_INTEGER: {
		fprintf(stderr, "T_INTEGER readcells\n");
		*cell = cellalloc(A_CONS);
		car(*cell) = cellalloc(A_ATOM);
		intof(car(*cell)) = reader->tok.as.integer;
		atomof(car(*cell)).type = A_INTEGER;
		if ((err = readcells(reader, &cdr(*cell)))) goto exit;
		return OK;
	}
	case T_SYMBOL: {
		fprintf(stderr, "T_SYM readcells\n");
		*cell = cellalloc(A_CONS);
		car(*cell) = cellalloc(A_ATOM);
		strof(car(*cell)) = reader->tok.as.string;
		atomof(car(*cell)).type = A_SYMBOL;
		fprintf(stderr, "%s\n", strof(car(*cell)));
		if ((err = readcells(reader, &cdr(*cell)))) goto exit;
		return OK;
	}
	case T_STRING: {
		fprintf(stderr, "T_SYM readcells\n");
		*cell = cellalloc(A_CONS);
		car(*cell) = cellalloc(A_ATOM);
		strof(car(*cell)) = reader->tok.as.string;
		atomof(car(*cell)).type = A_STRING;
		fprintf(stderr, "%s\n", strof(car(*cell)));
		if ((err = readcells(reader, &cdr(*cell)))) goto exit;
		return OK;
	}
	}
exit:
	freecell(cell);
	*cell = nil;
	return err;
}


Cell *
read(Reader *reader)
{
	Cell *cell;
        if (reader_next(reader)) return nil;
	switch (reader->tok.type) { /* todo: handle all cases */
	case T_LPAREN: {
		fprintf(stderr, "T_LPAREN read\n");
		cell = nil;
		readcells(reader, &cell);
		return cell;
	}
	case T_SYMBOL: {
		fprintf(stderr, "T_SYM read\n");
		cell = cellalloc(A_ATOM);
		symof(cell) = reader->tok.as.string;
		atomof(cell).type = A_SYMBOL;
		return cell;
	}
	case T_STRING: {
		fprintf(stderr, "T_STRING read\n");
		cell = cellalloc(A_ATOM);
		strof(cell) = reader->tok.as.string;
		atomof(cell).type = A_STRING;
		return cell;
	}
	case T_INTEGER: {
		fprintf(stderr, "T_INTEGER number\n");
		cell = cellalloc(A_ATOM);
		intof(cell) = reader->tok.as.integer;
		atomof(cell).type = A_INTEGER;
		return cell;
	}
	case T_DOUBLE: {
		fprintf(stderr, "T_DOUBL number\n");
		cell = cellalloc(A_ATOM);
		doublof(cell) = reader->tok.as.doubl;
		atomof(cell).type = A_DOUBLE;
		return cell;
	}
	case T_RPAREN: reader->err = UNMATCHED_RPAREN_ERR; return nil;
	case T_DOT:    reader->err = TO_MANY_DOTS_ERR;     return nil;
	default: return nil;
	}
}



/* -- PRINTER -- */
void
print_aux(Cell *cell, int last)
{
	if (!cell) return;
	switch (cell->type) {
	case A_ATOM:
		if (strp(cell)) {
			putchar('"');
		}
		if (strp(cell) || symp(cell))
			fputs(strof(cell), stdout);
		else if (intp(cell))
			printf("%d", intof(cell));
		else fprintf(stderr, "print_aux: type not recognized");
		if (strp(cell)) {
			putchar('"');
		}
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
	if (nilp(cell)) return;
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


/* -- EVAL -- */
int
add(Cell *cell)			/* todo: errors */
{
	int acc = 0;
	while  (!nilp(cell)) {
		if (!intp(car(cell))) {
			fprintf(stderr, "add: intp error");
			return 0;
		} else if (!lisp(cdr(cell))) {
			fprintf(stderr, "add: lisp error");
			return 0;
		}
		acc += intof(car(cell));
		cell = cdr(cell);
	}
	return acc;
}

Cell *eval(Cell *cell);


Cell *
eval_cdr(Cell *cell)
{
	if (nilp(cell)) return nil;
	if (atomp(cell)) return cellcopy(cell);
	assert(consp(cell));
	Cell *ret = cellalloc(A_CONS);
	car(ret) = eval(car(cell));
	cdr(ret) = eval_cdr(cdr(cell));
	return ret;
}

Cell *
eval(Cell *cell)		/* eval car */
{
	if (nilp(cell))  return nil; /* fix memory */
	if (atomp(cell)) return cellcopy(cell);
	assert(consp(cell));
	Cell *hd = eval(car(cell));
	if (!nilp(hd) && symp(hd)) {
		if (strcmp(symof(hd), "+") == 0) {
			freecell(&hd);
			Cell *ret = cellalloc(A_ATOM);
			Cell *tl = eval_cdr(cdr(cell));
			intof(ret) = add(tl);
			atomof(ret).type = A_INTEGER;
			freecell(&tl);
			return ret;
		}
	}
	fprintf(stderr, "illegal function call\n");
	freecell(&hd);
	return nil;
}

int
main(int argc, char *argv[])
{
	(void)argc, (void)argv;
	#if 0
	FILE *file = fopen(argv[1], "r");
	char ch[SYM_LEN];
	if (!file) {
		printf("doesnt work file");
		exit(1);
	}
	#endif
	Reader *reader;
	Cell *sexp;
	Cell *res;
	print(res = eval(sexp = read(reader = reader_make(stdin))));
	fflush(stdout);
	freecell(&sexp);
	freecell(&res);
	if (reader->err) {
		fprintf(stderr, "%s\n", READ_ERR2STR[reader->err]);
	}
	free(reader);
}
