#include "types/vec.h"
#include "types/atom.h"
#include "types/arena.h"
#include "read.h"

#define DECOMP_COL "8"

typedef struct Env {
	struct Env *top;
	Vec(Atom) binds;
} Env;

/* these are vector types */
typedef struct {
	Vec(uint8_t) code;
	Vec(Atom) workspace;
} Chunk;

typedef enum {
	OP_RET  = 0,
	OP_CALL = 1,
	OP_LOAD = 2,
	OP_SAVE = 3,
	OP_CONS = 4,
} OpCode;

Chunk *
compile(Cell *sexp)
{
	Chunk *chunk = malloc(sizeof(Chunk));
	vec_ini(chunk->code);
	vec_ini(chunk->workspace);
	vec_push(chunk->code, OP_RET);
	vec_push(chunk->code, OP_CONS);
	vec_push(chunk->workspace, TO_INT(-12));
	vec_push(chunk->code, 0);
	vec_push(chunk->code, OP_CONS);
	vec_push(chunk->workspace, TO_INT(13));
	vec_push(chunk->code, 1);
	vec_push(chunk->code, OP_RET);
	return chunk;
}


void
chunkfree(Chunk *chunk)
{
	vec_free(chunk->code);
	vec_free(chunk->workspace);
	free(chunk);
}

static ptrdiff_t
basic_op(const char *name, ptrdiff_t offset)
{
	printf("%-"DECOMP_COL"s\n", name);
	return offset + 1;
}

static ptrdiff_t
cons_op(const char *name, Chunk *chunk, ptrdiff_t offset)
{
	uint8_t idx = chunk->code[offset + 1];
	printf("%-"DECOMP_COL"s %4d ;; %s\n", name, idx, atomstr(chunk->workspace[idx]));
	return offset + 2;
}

ptrdiff_t
decompile_chunk(Chunk *chunk, ptrdiff_t offset)
{
	printf("; %04ld ", offset);

	uint8_t instr = chunk->code[offset];
	switch (instr) {
	case OP_RET: return basic_op("OP_RET", offset);
	case OP_CONS: return cons_op("OP_CONS", chunk, offset);
	default:
		printf("; Unknown opcode %d\n", instr);
		return offset + 1;
	}

}

void
decompile(Chunk *chunk, const char *name)
{
	printf(";;;;;;;;|%s|;;;;;;;;\n", name);
	for (size_t offset = 0; offset < vec_len(chunk->code);)
		offset = decompile_chunk(chunk, offset);
}



int main() {
	Reader *reader = ropen(stdin);
	Arena *sexpa = aini();
	Cell *sexp = reades(sexpa, reader);
	Chunk *chunk = compile(sexp);
	decompile(chunk, "test");
	chunkfree(chunk);

	if (readerr(reader)) {
		fprintf(stderr, "%s\n", readerr(reader));
	} else {
		printes(sexp);
		fflush(stdout);
	}


	deinit(sexpa);
	rclose(reader);
	return 0;
}
