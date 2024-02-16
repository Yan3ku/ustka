#include "vm.h"

#define STACK_MAX 4096
#define VM_TRACE 1

/*;; Glorious Lisp Virtual Machine (GLVM) ;;*/
typedef enum {
	OK,
	COMPILE_ERR,
	RUNTIME_ERR,
} EvalErr;

typedef struct Env {
	struct Env *top;
	Vec(Value) binds;
} Env;

typedef struct {
	Env *env;
	uint8_t *ip;
	Chunk *chunk;
	Value stack[STACK_MAX];
	Value *sp;
} VM;

static VM vm;

#define VM_INCIP() (*vm.ip++)
#define VM_CONS() vm.chunk->conspool[VM_INCIP()]

void
vminit()
{
	vm.sp = vm.stack;
}

void
vmfree()
{

}

void push(Value value) { *vm.sp++ = value; }
Value pop(void) { return *(--vm.sp); }

#define BIN_OP(op) do {							\
		Value b_ = pop();					\
		Value a_ = pop();					\
		if (DOUBLP(a_) || DOUBLP(b_))				\
			push(TO_DOUBL(AS_NUM(a_) op AS_NUM(b_)));	\
		else							\
			push(TO_INT(AS_INT(a_) op AS_INT(b_)));		\
	} while (0);

EvalErr
run()
{
	for (;;) {
#ifdef VM_TRACE
		printf(";;; STACK;");
		for (Value* slot = vm.stack; slot < vm.sp; slot++)
			printf(" %s ;", valuestr(*slot));
		printf("\n");
		printf(";;; OPCODE");
		decompile_op(vm.chunk, vm.ip - vm.chunk->code);
		printf("\n");
#endif
		uint8_t opcode;
		switch (opcode = VM_INCIP()) {
		case OP_NEG:{
			Value val = pop();
			if (ASSERTV(NUMP, val)) break;
			if INTP(val) push(TO_INT(-AS_INT(val)));
			else if DOUBLP(val) push(TO_DOUBL(-AS_DOUBL(val)));
			break;
		}
		case OP_ADD: BIN_OP(+); break;
		case OP_SUB: BIN_OP(-); break;
		case OP_MUL: BIN_OP(*); break;
		case OP_DIV: BIN_OP(/); break;
		case OP_CONS: {
		        Value val = VM_CONS();
			push(val);
			break;
		}
		case OP_RET: {
			printf("%s", valuestr(pop()));
			return OK;
		}
		default: assert(0 && "unreachable");
		}
	}
}
