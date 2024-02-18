/*
#include "types/value.h"
#include "types/sexp.h"
#include "types/vec.h"
*/

#define BYTE_COL  "5"
#define WHERE_COL "7"
#define CODE_COL  "7"
#define ARGS_COL  "5"
#define NOTE_COL  "4"

#define DECOMP_HEADER							\
	";;; %s\n; %-"BYTE_COL"s ; %-"WHERE_COL"s ; %-"CODE_COL"s ; %-"ARGS_COL"s ; %s"

typedef struct SerialRange SerialRange;

typedef struct {
	const char *fname;
	Vec(uint8_t) code;
	Vec(Value) conspool;
	Vec(SerialRange) where;	/* run length encoding */
} Chunk;

typedef enum {
	OP_RET,
	OP_CALL,
	OP_LOAD,
	OP_SAVE,
	OP_CONS,
	OP_NEG,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
} OpCode;

ptrdiff_t decompile_op(Chunk *chunk, ptrdiff_t offset);
void decompile(Chunk *chunk, const char *name);
Chunk *compile(Sexp *sexp);
Range whereis(Chunk *chunk, ptrdiff_t offset);
void chunkfree(Chunk *chunk);
Chunk *chunknew(void);
