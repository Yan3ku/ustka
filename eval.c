#include "types/vec.h"
#include "types/value.h"
#include "types/arena.h"
#include "read.h"

#define STACK_MAX 4096
#define VM_TRACE 1

#define BYTE_COL  "5"
#define WHERE_COL "7"
#define CODE_COL  "7"
#define ARGS_COL  "5"
#define DECOMP_HEADER							\
	";;; %s ;;;\n; %-"BYTE_COL"s ; %-"WHERE_COL"s ; %-"CODE_COL"s ; %-"ARGS_COL"s ; %s"

typedef struct {
	size_t at;
	size_t len;
	size_t count;		/* count of repetitive ranges */
} SerialRange;

/* these are vector types */
typedef struct {
	const char *fname;
	Vec(uint8_t) code;
	Vec(Value) conspool;
	Vec(SerialRange) where;	/* run length encoding */
} Chunk;

typedef enum {
	OP_RET  = 0,
	OP_CALL = 1,
	OP_LOAD = 2,
	OP_SAVE = 3,
	OP_CONS = 4,
} OpCode;

typedef enum {
	OK,
	COMPILE_ERR,
	RUNTIME_ERR,
} EvalErr;

typedef struct Env {
	struct Env *top;
	Vec(Value) binds;
} Env;

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

void
cwrite(Chunk *chunk, uint8_t byte, Range pos)
{
	vec_push(chunk->code, byte);
	if (vec_len(chunk->where) > 0 && vec_end(chunk->where).at == pos.at) {
		vec_end(chunk->where).count++;
	} else {
		SerialRange range = {.at = pos.at, .len = pos.len, .count = 1};
		vec_push(chunk->where, range);
	}
}

Range
whereis(Chunk *chunk, ptrdiff_t offset)
{
	size_t i = 0;
	ptrdiff_t cursor = chunk->where[0].count;
	while (cursor <= offset) {
		cursor += chunk->where[++i].count;
	}
	return (Range){.at = chunk->where[i].at, .len = chunk->where[i].len};
}

void
cwriteval(Chunk *chunk, Value val, Range pos) {
	vec_push(chunk->conspool, val);
	cwrite(chunk, OP_CONS, pos);
	cwrite(chunk, vec_len(chunk->conspool) - 1, pos);
}

Chunk *
compile(Sexp *sexp)
{
	Chunk *chunk = chunknew();
	cwriteval(chunk, TO_INT(-13), (Range){0, 0});
	cwriteval(chunk, TO_INT(14), (Range){0, 0});
	cwriteval(chunk, TO_DOUBL(12.3), (Range){0, 0});
	cwrite(chunk, OP_RET, (Range){100, 1});
	return chunk;
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

ptrdiff_t
decompileop(Chunk *chunk, ptrdiff_t offset)
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
	case OP_CONS: return cons_op(chunk, offset);
	default:
		printf("; Unknown opcode %d\n", instr);
		return offset + 1;
	}

}

void
printcol(const char *width)
{
	printf(";-");
	for (int i = 0; i < atoi(width); i++) putchar('-');
	printf("-");
}

void
decompile(Chunk *chunk, const char *name)
{
	printf(DECOMP_HEADER"\n", name, "BYTE", "WHERE", "OPCODE", "ARGS", "NOTE");
	printcol(BYTE_COL);
	printcol(WHERE_COL);
	printcol(CODE_COL);
	printcol(ARGS_COL);
	printf(";------\n");
	for (size_t offset = 0; offset < vec_len(chunk->code);)
		offset = decompileop(chunk, offset);
}

/*;; Glorious Lisp Virtual Machine (GLVM) ;;*/
typedef struct {
	Env *env;
	uint8_t *ip;
	Chunk *chunk;
	Value stack[STACK_MAX];
} VM;

static VM vm;

#define VM_INCIP() (*vm.ip++)
#define VM_CONS() vm.chunk->conspool[VM_INCIP()]

void
vminit()
{

}

void
vmfree()
{

}

EvalErr
run()
{
	for (;;) {
#ifdef VM_TRACE
		decompileop(vm.chunk, vm.ip - vm.chunk->code);
#endif
		uint8_t opcode;
		switch (opcode = VM_INCIP()) {
		case OP_CONS: {
		        Value val = VM_CONS();
			printf("%s\n", valuestr(val));
			break;
		}
		case OP_RET: {
			return OK;
		}
		}
	}
}

EvalErr
eval(Chunk *chunk)
{
	vm.chunk = chunk;
	vm.ip = chunk->code;
	return run();
}

int main() {
	vminit();
	vmfree();
	Chunk *chunk = compile(nil);
	eval(chunk);
	chunkfree(chunk);
	/* for (int i = 0; i < 3; i++) { */
	/* 	Sexp *sexp = reades(reader); */
	/* 	decompile(chunk, "test"); */
	/* 	chunkfree(chunk); */
	/* 	if (readerr(reader)) { */
	/* 		fprintf(stderr, "%ld: %s\n", readerrat(reader), readerr(reader)); */
	/* 	} else { */
	/* 		printes(sexp); */
	/* 		fflush(stdout); */
	/* 	} */
	/* 	sexpfree(sexp); */
	/* } */
	return 0;
}
