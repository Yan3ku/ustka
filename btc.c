#include "aux.h"
#include "types/arena.h"
#include "types/value.h"
#include "types/sexp.h"
#include "types/vec.h"
#include "btc.h"
#include "read.h"

/* DECOMPILATION */
struct SerialRange {
	Range range;
	size_t count;		/* count of repetitive ranges */
};

void
chunkfree(Chunk *chunk)
{
	vec_free(chunk->code);
	vec_free(chunk->where);
	vec_free(chunk->conspool);
	free(chunk);
}


Chunk *
chunknew(void)
{
	Chunk *chunk = malloc(sizeof(Chunk));
	vec_ini(chunk->code);
	vec_ini(chunk->where);
	vec_ini(chunk->conspool);
	return chunk;
}

static void
emit(Chunk *chunk, uint8_t byte, Range pos)
{
	vec_push(chunk->code, byte);
	if (vec_len(chunk->where) > 0 && vec_end(chunk->where).range.at == pos.at) {
		vec_end(chunk->where).count++;
	} else {
		SerialRange range = {.range = pos, .count = 1};
		vec_push(chunk->where, range);
	}
}

Range
whereis(Chunk *chunk, ptrdiff_t offset)
{
	size_t i = 0;
	ptrdiff_t cursor = chunk->where[0].count;
	while (cursor <= offset)
		cursor += chunk->where[++i].count;
	return chunk->where[i].range;
}

static ptrdiff_t
basic_op(const char *name, ptrdiff_t offset)
{
	printf("OP_%-"CODE_COL"s\n", name);
	return offset + 1;
}

static ptrdiff_t
cons_op(Chunk *chunk, ptrdiff_t offset)
{
	uint8_t idx = chunk->code[offset + 1];
	printf("%-"CODE_COL"s ; %-"ARGS_COL"d ; %s\n",
	       "OP_CONS", idx, valuestr(chunk->conspool[idx]));
	return offset + 2;
}

static void
printcol(const char *width)
{
	printf(";-");
	for (int i = 0; i < atoi(width); i++) putchar('-');
	printf("-");
}

static void
decompile_header(const char *name)
{
	printf(DECOMP_HEADER"\n", name, "BYTE", "WHERE", "OPCODE", "ARGS", "NOTE");
	printcol(BYTE_COL);
	printcol(WHERE_COL);
	printcol(CODE_COL);
	printcol(ARGS_COL);
	printcol(NOTE_COL);
	printf("\n");
}

static ptrdiff_t
decompile_op_(Chunk *chunk, ptrdiff_t offset)
{
	static Range lastrange;
	printf("; %0"BYTE_COL"ld ; ", offset);
        Range where = whereis(chunk, offset);
	if (offset > 0 && lastrange.at == where.at) {
		printf("%-"WHERE_COL"s ", "-");
	} else {
		char buff[BUFSIZ];
		snprintf(buff, BUFSIZ, "%-4ld %ld ", where.at, where.len);
		printf("%-8s", buff);
	}
	printf("; ");
	lastrange = where;

	uint8_t instr = chunk->code[offset];
	switch (instr) {
	case OP_RET:  return basic_op("RET", offset);
	case OP_NEG:  return basic_op("NEG", offset);
	case OP_ADD:  return basic_op("ADD", offset);
	case OP_SUB:  return basic_op("SUB", offset);
	case OP_MUL:  return basic_op("MUL", offset);
	case OP_DIV:  return basic_op("DIV", offset);
	case OP_CONS: return cons_op(chunk, offset);
	default:
		printf("; Unknown opcode %d\n", instr);
		return offset + 1;
	}

}

ptrdiff_t
decompile_op(Chunk *chunk, ptrdiff_t offset)
{
	decompile_header("OPCODE");
	return decompile_op_(chunk, offset);
}


void
decompile(Chunk *chunk, const char *name)
{
	decompile_header(name);
	for (size_t offset = 0; offset < vec_len(chunk->code);)
		offset = decompile_op_(chunk, offset);
	printf(";\n");
	printf(";;; [END]\n");
}

/* COMPILATION */
static void
emitcons(Chunk *chunk, Value val, Range pos) {
	vec_push(chunk->conspool, val);
	emit(chunk, OP_CONS, pos);
	emit(chunk, vec_len(chunk->conspool) - 1, pos);
}



static void compile_(Cell *cell, Chunk *chunk);

static void
compilerest(Cell *cell, Chunk *chunk)
{
	if (!cell) return;
	if (ATOMP(CAR(cell))) {
		if (CAR(cell)->type == A_INT) {
			emitcons(chunk, TO_INT(CAR(cell)->integer), CELL_LOC(CAR(cell)));
		}
	} else if (CONSP(CAR(cell))) {
		compile_(CAR(cell), chunk);
	}
	compilerest(CDR(cell), chunk);
}

static void
compile_(Cell *cell, Chunk *chunk)
{
	if (!cell) return;
	if (ATOMP(CAR(cell))) {
		if (CAR(cell)->type == A_SYM) {
			if (!strcmp(CAR(cell)->string, "+")) {
				compilerest(CDR(cell), chunk);
				emit(chunk, OP_ADD, CELL_LOC(CAR(cell)));
				return;
			}
		}
	}
}

Chunk *
compile(Sexp *sexp)
{
	Cell *cell = sexp->cell;
	Chunk *chunk = chunknew();
	compile_(cell, chunk);
	emit(chunk, OP_RET, (Range){0, 0});
	return chunk;
}
