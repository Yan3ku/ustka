/*;; The Sexp Reader For More Civilized Age ;;*/
#include "types/vec.h"
#include "types/sexp.h"
#include "types/arena.h"
#include "read.h"


typedef struct {
	enum {
		OK,
		EOF_ERR,
		DOT_MANY_FOLLOW_ERR,
		NOTHING_BEFORE_DOT_ERR,
		NOTHING_AFTER_DOT_ERR,
		DOT_CONTEXT_ERR,
		UNMATCHED_BRA_ERR,
		UNMATCHED_KET_ERR,
		UNMATCHED_SBRA_ERR,
		UNMATCHED_SKET_ERR,
	} type;
	size_t at;
} ReadErr;

const char *READ_ERR[] = {
	[OK]                  	 = "READER SAYS OK :)",
	[EOF_ERR]             	 = "unexpected end of file",
	[DOT_MANY_FOLLOW_ERR]    = "more than one object follows . in list",
	[NOTHING_BEFORE_DOT_ERR] = "nothing appears before . in list.",
	[NOTHING_AFTER_DOT_ERR]  = "nothing appears after . in list.",
	[DOT_CONTEXT_ERR]        = "dot context error",
	[UNMATCHED_BRA_ERR]      = "unmatched opening parenthesis",
	[UNMATCHED_KET_ERR]      = "unmatched close parenthesis",
	[UNMATCHED_SBRA_ERR]     = "unmatched opening square bracket",
	[UNMATCHED_SKET_ERR]     = "unmatched close square bracket",
};

/* 'nextitem` either returns token from this enum or Cell pointer
 * the NIL is reserved for returning NULL/nil in error case */
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


/* ;; TOKENIZER ;; */
struct Reader {
	FILE *input;
	const char *fname;
	size_t cursor;
	ReadErr err;
};

const char *
readerr(Reader *reader)
{
	return reader->err.type ? READ_ERR[reader->err.type] : NULL;
}

size_t
readerrat(Reader *reader)
{
	return reader->err.at;
}

Reader *
ropen(const char *fname)
{
	Reader *reader = calloc(1, sizeof(Reader));
	if (!fname) reader->input = stdin;
	else if (!(reader->input = fopen(fname, "r"))) {
		perror("ropen:");
		return nil;
	}
	reader->fname = fname;
	return reader;
}

void
rclose(Reader *reader)
{
	fclose(reader->input);
	free(reader);
}

static int
istermsexp(char chr)
{
	return (strchr(")(`'.#", chr) || isspace(chr) || chr == EOF);
}

static int
rgetc(Reader *reader)
{
	char ch = fgetc(reader->input);
	if (ch != EOF) reader->cursor++;
	return ch;
}

static void
rungetc(Reader *reader, char chr)
{
	reader->cursor--; ungetc(chr, reader->input);
}

static size_t
readstr(Reader *reader, Vec(char) str)
{
	char chr;
	int esc = 0;
	size_t pos = reader->cursor;
	while ((chr = rgetc(reader)) != EOF) {
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
	vec_push(str, '\0');
	return reader->cursor - pos;
}

static size_t
readsym(Reader *reader, Vec(char) str)
{
	char chr;
	size_t pos = reader->cursor;
	while (!istermsexp(chr = rgetc(reader))) {
		vec_push(str, chr);
	}
	rungetc(reader, chr);
	vec_push(str, '\0');
	return reader->cursor - pos;
}


static Cell *			/* todo split string parsing routines */
nextitem(Arena *arena, Reader *reader)
{
	char chr;
	while (isspace(chr = rgetc(reader)) && chr != EOF);
	switch (chr) {
        case '(':  return (Cell *)BRA;
	case ')':  return (Cell *)KET;
	case '[':  return (Cell *)SBRA;
        case ']':  return (Cell *)SKET;
        case '\'': return (Cell *)QUOTE;
        case '`':  return (Cell *)BSTICK;
        case '.':  return (Cell *)DOT;
        case '#':  return (Cell *)HASH;
	case EOF:
		reader->err = (ReadErr){EOF_ERR, reader->cursor};
		return (Cell *)EOF;
	case '"': {		/* todo: test string */
		VEC(char, str);
		Cell *cell = cellof(arena, A_ATOM, reader->cursor - 1);
		cell->type = A_STR;
		cell->string = nil;
		CELL_LEN(cell) = readstr(reader, str) + 1 /* + " */;
		if (feof(reader->input)) {
			vec_free(str);
			reader->err = (ReadErr){EOF_ERR, reader->cursor};
			return nil;
		}
		cell->string = new(arena, strlen(str) + 1);
		strcpy(cell->string, str);
		vec_free(str);
		return cell;
 	} default:		/* todo: add double */
		  rungetc(reader, chr);
		  VEC(char, sym);
		  Cell *cell = cellof(arena, A_ATOM, reader->cursor);
		  CELL_LEN(cell) = readsym(reader, sym);
		  char *endptr;
		  /* if looks like number it's number */
		  cell->integer = strtol(sym, &endptr, 10);
		  if (*endptr == '\0') {
			  vec_free(sym);
			  cell->type = A_INT;
			  return cell;

		  }
		  cell->type = A_SYM;
		  cell->string = new(arena, strlen(sym) + 1);
		  strcpy(cell->string, sym);
		  vec_free(sym);
		  return cell;
	}
}


/* ;; READ SEXP ;; */
/* -es stands for (e)S-expression */
static Cell * reades_(Arena *arena, Reader *reader);

static void
discardsexp(Arena *arena, Reader *reader) {
	Cell *item;
	ReadErr err = reader->err;
	int depth = 0;
	/* because NOTHING_AFTER_DOT_ERR consumes KET synchronization can be omited */
	if (reader->err.type == EOF_ERR || reader->err.type == NOTHING_AFTER_DOT_ERR)
		return;
	while ((item = nextitem(arena, reader)) != (Cell *)EOF) {
		if (item == (Cell *)BRA) depth++;
		else if (item == (Cell *)KET) depth--;
		if (depth < 0) break;
	}
	reader->err = err;
}

static Cell * /* todo: implement all tokens / make area size fixed? */
readrest(Arena *arena, Reader *reader)
{
	Cell *tl = nil;
	Cell *hd = nil;
	Cell *errel = nil;
	size_t begcur = reader->cursor - 1;
	Cell *item = nextitem(arena, reader);
	while (item != (Cell *)KET) {
		switch ((uint64_t)item) {
		case EOF: return nil;
		case BRA:
			item = readrest(arena, reader);
			if (reader->err.type) {
				discardsexp(arena, reader);
				return nil;
			}
			break;
		case DOT:  /* expected is DOT <sexp> KET, otherwise error */
			if (!tl) {
				reader->err = (ReadErr){
					NOTHING_BEFORE_DOT_ERR,
					reader->cursor - 1
				};
				return nil;
			}
			size_t prevcur = reader->cursor - 1;
			CDR(tl) = reades_(arena, reader);
			if (reader->err.type) {
				if (reader->err.type == UNMATCHED_KET_ERR) {
					reader->err = (ReadErr){
						NOTHING_AFTER_DOT_ERR,
						prevcur,
					};
				}
				return nil;
			}
			/*
			 * `reades_' is used here because it doesn't consume KET
			 * In case of error hanging KET is expected to be
			 * consumed by `discardsexp' on synchronization
			 */
			prevcur = reader->err.at;
			if ((errel = reades_(arena, reader)) != nil) {
				reader->err = (ReadErr){
					DOT_MANY_FOLLOW_ERR,
					CELL_AT(errel),
				};
				return nil;
			}
			if (reader->err.type != UNMATCHED_KET_ERR)
				return nil; /* propagate further same error */
			reader->err = (ReadErr){OK, prevcur}; /* proper sexp */
			if (hd) CELL_LEN(hd) = reader->cursor - begcur;
			return hd;
		}
		Cell *cell = cons(arena, item, nil, begcur);
		if (!hd) hd = cell;
		else CDR(tl) = cell;
		tl = cell;
		item = nextitem(arena, reader);
	}
	if (hd) CELL_LEN(hd) = reader->cursor - begcur;
	return hd;
}

static Cell *
reades_(Arena *arena, Reader *reader)
{
	Cell *item = nextitem(arena, reader);
	switch ((uint64_t)item) {
	case 0:    return nil;
	case BRA: {
		Cell *cell = readrest(arena, reader);
		if (reader->err.type) discardsexp(arena, reader);
		return cell;
	}
	case KET:  reader->err = (ReadErr){UNMATCHED_KET_ERR,  reader->cursor - 1}; return nil;
	case SKET: reader->err = (ReadErr){UNMATCHED_SBRA_ERR, reader->cursor - 1}; return nil;
	case DOT:  reader->err = (ReadErr){DOT_CONTEXT_ERR,    reader->cursor - 1}; return nil;
	default:   return item;
	}
}

Sexp *
reades(Reader *reader)
{
	Sexp *sexp = malloc(sizeof(Sexp));
	sexp->arena = aini();	/* imagine trying to clear memory without arena */
	sexp->fname = reader->fname;
	reader->err = (ReadErr){OK, reader->cursor};
	sexp->cell = reades_(sexp->arena, reader);
	return sexp;
}

void
sexpfree(Sexp *sexp)
{
	deinit(sexp->arena);	/* I fucking love arena */
	free(sexp);
}


/* ;; PRINTER ;; */
static void
printes_(Cell *cell) {
	if (!cell) { printf("nil"); return; }
#ifdef PRINTES_LOCATION
	printf(RANGEFMT, RANGEP(CELL_LOC(cell)));
#endif
	if (CONSP(cell)) {
		putchar('(');
		printes_(CAR(cell));
		while (CONSP(CDR(cell)))  {
			putchar(' ');
			cell = CDR(cell);
			printes_(CAR(cell));
		};
		if (ATOMP(CDR(cell))) {
			fputs(" . ", stdout);
			printes_(CDR(cell));
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

void printes(Sexp *sexp) { printes_(sexp->cell); }
