#include <stdio.h>
#include "types/sexp.h"

#define PRINTES_LOCATION 1

typedef struct Reader Reader;
const char *readerr(Reader *reader);
size_t readerrat(Reader *reader);
int readeof(Reader *reader);
Reader *ropen(const char *input);
void rclose(Reader *reader);
Sexp *reades(Reader *reader);
void sexpfree(Sexp *sexp);
void printes(Sexp *sexp);
