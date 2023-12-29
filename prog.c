#define AUX_IMPL
#include "aux.h"
#include "read.h"

void usage() { printf("error\n"); }

int main(int argc, char *argv[]) {
	Reader *reader = ropen(stdin);
	Cell *sexp;
	Arena *arena;
	ARGBEGIN {
		case 'a': printf("%s", ARGF()); break;
	} ARGEND
	for (int i = 0; i < 3; i++) {
		arena = aini();
		sexp = reades(arena, reader);
		if (readerr(reader)) {
			fprintf(stderr, "%s\n", readerr(reader));
		} else {
			printes(sexp);
			fflush(stdout);
		}
		adeinit(arena);
	}
	rclose(reader);
	return 0;
}
