#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include "read.h"
#include "vec.h"


#define nil (void *)0

typedef enum {
	OK,
	EOF_ERR,
	DOT_FOLLOW_ERR,
	TO_MANY_DOTS_ERR,
	UNMATCHED_RPAREN_ERR,
	UNMATCHED_RBRACKET_ERR,
} ReadErr;


const char *READ_ERR[] = {
	[OK]                  	 = "OK",
	[EOF_ERR]             	 = "unexpected end of file",
	[DOT_FOLLOW_ERR]      	 = "more than one object follows . in list",
	[TO_MANY_DOTS_ERR]       = "to many dots",
	[UNMATCHED_RPAREN_ERR]   = "unmatched close parenthesis",
	[UNMATCHED_RBRACKET_ERR] = "unmatched close bracket",
};


static Cell *cellalloc(CellType type);
static int reader_isterm(char ch);
static ReadErr reader_next(Reader *reader);
static ReadErr readcells(Reader *reader, Cell **cell);
static void print_aux(Cell *cell, int last);

void
atomfree(Lit *atom)
{
	if (atom->type == A_STRING || atom->type == A_SYMBOL)
		free(atom->as.string);
}


Cell *
cellalloc(CellType type)
{
	Cell *cell = malloc(sizeof(Cell));
	cell->type = type;
	if (type == A_CONS) car(cell) = cdr(cell) = nil;
	return cell;
}

void
freecell(Cell **cell)
{
	if (!*cell) return;
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


/* -- TOKENIZER -- */
struct Reader {
	FILE *input;
	size_t cursor;
	ReadErr err;
	Lit atom;
	enum {
		T_ATOM,
		T_LPAREN,
		T_RPAREN,
		T_RBRACKET,
		T_LBRACKET,
		T_HASH,
		T_DOT,
		T_QUOTE,
		T_BSTICK,
	} type;
};

const char *
readererr(Reader *reader)
{
	return reader->err ? READ_ERR[reader->err] : NULL;
}

Reader *
open(FILE *input)
{
	Reader *reader = calloc(1, sizeof(Reader));
	reader->input = input;
	return reader;
}

void
close(Reader *reader)
{
	fclose(reader->input);
	free(reader);
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
	reader->err = OK;
	while (isspace(chr = fgetc(reader->input)) && chr != EOF);
	switch (chr) {
	case EOF:  reader->err = EOF_ERR;     return reader->err;
	case ')':  reader->type = T_RPAREN;   return OK;
        case '(':  reader->type = T_LPAREN;   return OK;
	case '[':  reader->type = T_LBRACKET; return OK;
        case ']':  reader->type = T_RBRACKET; return OK;
        case '\'': reader->type = T_QUOTE;    return OK;
        case '`':  reader->type = T_BSTICK;   return OK;
        case '.':  reader->type = T_DOT;      return OK;
        case '#':  reader->type = T_HASH;     return OK;
	case '"': {		/* todo: test string */
		reader->type = T_ATOM;
		int esc = 0;
		Vec(char) str;
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
		reader->atom.as.string = strdup(str);
		vec_free(str);
		return OK;
	} default: {		/* todo: add double */
		  reader->type = T_ATOM;
		  ungetc(chr, reader->input);
		  Vec(char) str;
		  vec_ini(str);
		  char *endptr = str;
		  while (((chr = fgetc(reader->input)) != EOF) && !reader_isterm(chr)) {
			  vec_push(str, chr);
		  }
		  ungetc(chr, reader->input);
		  vec_push(str, '\0');
		  reader->atom.type = A_INTEGER;
		  reader->atom.as.integer = strtol(str, &endptr, 10);
		  if (*endptr == '\0') {
			  vec_free(str);
			  reader->atom.type = A_INTEGER;
			  return OK;

		  }
		  reader->atom.type = A_SYMBOL;
		  reader->atom.as.string = strdup(str);
		  vec_free(str);
		  return OK;
	  }
	}
}


/* -- READ SEXP -- */
ReadErr
readcells(Reader *reader, Cell **cell) /* todo: implement all tokens */
{
	ReadErr err;
	if ((err = reader_next(reader))) return err;
	switch (reader->type) {
        case T_LPAREN: {
		fprintf(stderr, "T_LAPREN readcells\n");
		*cell = cellalloc(A_CONS);
	        if ((err = readcells(reader, &car(*cell)))) return err;
		if ((err = readcells(reader, &cdr(*cell)))) return err;
		return OK;
	} case T_RPAREN:
		fprintf(stderr, "T_PAREN readcells\n");
		*cell = nil;
		return OK;
	case T_DOT:
		fprintf(stderr, "T_DOT readcells\n");
		if (!(*cell = read(reader))) return reader->err;
		if ((err = reader_next(reader))) return err;
		if (reader->type != T_RPAREN) {
			if (reader->type == T_ATOM) atomfree(&reader->atom);
			reader->err = DOT_FOLLOW_ERR;
			return reader->err;
		}
		return OK;
	case T_ATOM: {
		fprintf(stderr, "T_INTEGER readcells\n");
		*cell = cellalloc(A_CONS);
		car(*cell) = cellalloc(A_ATOM);
		memcpy(&atomof(car(*cell)), &reader->atom, sizeof(Lit));
		if ((err = readcells(reader, &cdr(*cell)))) return err;
		return OK;
	}
	case T_RBRACKET: reader->err = UNMATCHED_RBRACKET_ERR; return reader->err;
	default: reader->err = EOF_ERR; return reader->err;
	}
}


Cell *
read(Reader *reader)
{
	Cell *cell;
        if (reader_next(reader)) return nil;
	switch (reader->type) { /* todo: handle all cases */
	case T_LPAREN: {
		fprintf(stderr, "T_LPAREN read\n");
		cell = nil;
		if (readcells(reader, &cell)) freecell(&cell);
		return cell;
	}
	case T_ATOM: {
		fprintf(stderr, "T_SYM read\n");
		cell = cellalloc(A_ATOM);
		memcpy(&atomof(cell), &reader->atom, sizeof(Lit));
		return cell;
	}
	case T_RPAREN:   reader->err = UNMATCHED_RPAREN_ERR;   return nil;
	case T_RBRACKET: reader->err = UNMATCHED_RBRACKET_ERR; return nil;
	case T_DOT:      reader->err = TO_MANY_DOTS_ERR;       return nil;
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
		if (nilp(cell)) {
			printf("()");
			return;
		}
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
	if (!cell) {
		printf("nil");
		return;

	}
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
