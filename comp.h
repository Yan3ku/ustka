/*
#include "types/value.h"
#include "types/sexp.h"
#include "types/vec.h"
#include "types/ht.h"
*/

Chunk *compile(Sexp *sexp);
Range whereis(Chunk *chunk, ptrdiff_t offset);
void chunkfree(Chunk *chunk);
Chunk *chunknew(void);
