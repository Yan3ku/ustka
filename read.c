/*** The Mighty Sexp Reader ***/
#include "aux.h"
#include "types/vec.h"
#include "types/cell.h"
#include "types/arena.h"
#include "read.h"


typedef enum {
	OK,
	EOF_ERR,
	DOT_MANY_FOLLOW_ERR,
	TO_MANY_DOTS_ERR,
	NOTHING_BEFORE_DOT_ERR,
	NOTHING_AFTER_DOT_ERR,
	DOT_CONTEXT_ERR,
	UNMATCHED_BRA_ERR,
	UNMATCHED_KET_ERR,
	UNMATCHED_SBRA_ERR,
	UNMATCHED_SKET_ERR,
} ReadErr;


const char *READ_ERR[] = {
	[OK]                  	 = "READER SAYS OK",
	[EOF_ERR]             	 = "unexpected end of file",
	[DOT_MANY_FOLLOW_ERR]    = "more than one object follows . in list",
	[TO_MANY_DOTS_ERR]       = "to many dots",
	[NOTHING_BEFORE_DOT_ERR] = "nothing appears before . in list.",
	[NOTHING_AFTER_DOT_ERR]  = "nothing appears after . in list.",
	[DOT_CONTEXT_ERR]        = "dot context error",
	[UNMATCHED_BRA_ERR]      = "unmatched opening parenthesis",
	[UNMATCHED_KET_ERR]      = "unmatched close parenthesis",
	[UNMATCHED_SBRA_ERR]     = "unmatched opening square bracket",
	[UNMATCHED_SKET_ERR]     = "unmatched close square bracket",
};

/* 'nextitem` either returns token from this enum or Cell *
   the NIL is reserved for returning NULL/nil in error case */
/* BRA is opening, KET is closing parenthesis */
typedef enum {
	NIL,
	BRA,
	KET,
	SBRA,
	SKET,
	HASH,
	DOT,
	QUOTE,
	BSTICK,
} Token;


/* -- TOKENIZER -- */
struct Reader {
	FILE *input;
	size_t cursor;
	ReadErr err;
};

const char *
readerr(Reader *reader)
{
	return reader->err ? READ_ERR[reader->err] : NULL;
}

Reader *
ropen(FILE *input)
{
	Reader *reader = calloc(1, sizeof(Reader));
	reader->input = input;
	return reader;
}

void
rclose(Reader *reader)
{
	fclose(reader->input);
	free(reader);
}

static int			/* is character terminating sexp */
isterm(char chr)
{
	return (strchr(")(`'.#", chr) || isspace(chr) || chr == EOF);
}

static Cell *			/* todo split string parsing routines */
nextitem(Arena *arena, Reader *reader)
{
	char chr;
	while (isspace(chr = fgetc(reader->input)) && chr != EOF);
	switch (chr) {
	case EOF:  reader->err = EOF_ERR; return (Cell *)EOF; return nil;
        case '(':  return (Cell *)BRA;
	case ')':  return (Cell *)KET;
	case '[':  return (Cell *)SBRA;
        case ']':  return (Cell *)SKET;
        case '\'': return (Cell *)QUOTE;
        case '`':  return (Cell *)BSTICK;
        case '.':  return (Cell *)DOT;
        case '#':  return (Cell *)HASH;
	case '"': {		/* todo: test string */
		Cell *cell;
		cell = cellof(arena, A_ATOM);
		cell->type = A_STR;
		cell->string = nil;
		int esc = 0;
		VEC(char, str);
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
			return nil;
		}
		vec_push(str, '\0');
		cell->string = new(arena, strlen(str) + 1);
		strcpy(cell->string, str);
		vec_free(str);
		return cell;
	} default: {		/* todo: add double */
		  Cell *cell;
		  ungetc(chr, reader->input);
		  cell = cellof(arena, A_ATOM);
		  VEC(char, str);
		  char *endptr = str;
		  while (!isterm(chr = fgetc(reader->input))) {
			  vec_push(str, chr);
		  }
		  ungetc(chr, reader->input);
		  vec_push(str, '\0');
		  cell->integer = strtol(str, &endptr, 10);
		  if (*endptr == '\0') {
			  vec_free(str);
			  cell->type = A_INT;
			  return cell;

		  }
		  cell->type = A_SYM;
		  cell->string = new(arena, strlen(str) + 1);
		  strcpy(cell->string, str);
		  vec_free(str);
		  return cell;
	  }
	}
}


/* -- READ SEXP -- */
/* -es stands for (e)S-expression */
static Cell * reades_(Arena *arena, Reader *reader);

static void
discardsexp(Arena *arena, Reader *reader) {
	Cell *item;
	ReadErr err = reader->err;
	int depth = 0;
	/* because NOTHING_AFTER_DOT_ERR consumes KET synchronization can be omited */
	if (reader->err == EOF_ERR || reader->err == NOTHING_AFTER_DOT_ERR) return;
	while ((item = nextitem(arena, reader)) != (Cell *)EOF) {
		if (item == (Cell *)BRA) depth++;
		else if (item == (Cell *)KET) depth--;
		if (depth < 0) break;
	}
	reader->err = err;
}

Cell * /* todo: implement all tokens / make area size fixed? */
readrest(Arena *arena, Reader *reader)
{
	Cell *tl = nil;
	Cell *hd = nil;
	Cell *item = nextitem(arena, reader);
	while (item != (Cell *)KET) {
		switch ((uint64_t)item) {
		case EOF: return nil;
		case BRA:
			item = readrest(arena, reader);
			if (reader->err) { /* synchronize to the next sexp */
				discardsexp(arena, reader);
				return nil;
			}
			break;
		case DOT:
			if (!tl) {
				reader->err = NOTHING_BEFORE_DOT_ERR;
				return nil;
			}
			CDR(tl) = reades_(arena, reader);
			if (reader->err) {
				if (reader->err == UNMATCHED_KET_ERR)
					reader->err = NOTHING_AFTER_DOT_ERR;
				return nil;
			}
			if (readrest(arena, reader) != nil || reader->err) {
				reader->err = DOT_MANY_FOLLOW_ERR;
				return nil;
			}
			return hd;
		}
		Cell *cell = cons(arena, item, nil);
		if (!hd) hd = cell;
		else CDR(tl) = cell;
		tl = cell;
		item = nextitem(arena, reader);
	}
	return hd;
}

Cell *
reades_(Arena *arena, Reader *reader)
{
	Cell *item = nextitem(arena, reader);
	switch ((uint64_t)item) {
	case 0:    return nil;
	case BRA: {
		Cell *cell = readrest(arena, reader);
		if (reader->err) discardsexp(arena, reader);
		return cell;
	}
	case KET:  reader->err = UNMATCHED_KET_ERR;  return nil;
	case SKET: reader->err = UNMATCHED_SBRA_ERR; return nil;
	case DOT:  reader->err = DOT_CONTEXT_ERR;    return nil;
	default:   return item;
	}
}

Cell *
reades(Arena *arena, Reader *reader)
{
	reader->err = OK;
	return reades_(arena, reader);
}



/* -- PRINTER -- */
void
printes(Cell *cell) {
	if (!cell) { printf("nil"); return; }
	if (CONSP(cell)) {
		putchar('(');
		printes(CAR(cell));
		while (CONSP(CDR(cell)))  {
			putchar(' ');
			cell = CDR(cell);
			printes(CAR(cell));
		};
		if (ATOMP(CDR(cell))) {
			fputs(" . ", stdout);
			printes(CDR(cell));
		}
		putchar(')');
		return;
	}
	if (ATOMP(cell)) {
		switch (cell->type) {
		case A_STR: printf("\"%s\"", cell->string); break;
		case A_SYM: printf("%s", cell->string);     break;
		case A_INT: printf("%d", cell->integer);    break;
		case A_DOUBL: printf("%f", cell->doubl);    break;
		case A_VEC: assert(0 && "unimplemented");   break;
		}
		return;
	}
	assert(0 && "unreachable");
}
