#include <stdio.h>

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
} Lit; /* Atom-Literal */

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
		Lit atom;
		Cons cons;
	} as;
	CellType type;
};


typedef struct Reader Reader;
const char *readererr(Reader *reader);
Reader *open(FILE *input);
void close(Reader *reader);
Cell *read(Reader *reader);
void print(Cell *cell);
void atomfree(Lit *atom);
void freecell(Cell **cell);

#define cdr(c)       ((c)->as.cons.cdr)
#define car(c)       ((c)->as.cons.car)
#define cadr(c)      car(cdr(c))
#define cddr(c)      cdr(cdr(c))
#define consp(c)     ((c)->type == A_CONS)
#define atomp(c)     ((c)->type == A_ATOM)
#define nilp(c)      (c && !car(c) && !cdr(c))
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
