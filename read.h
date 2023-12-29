#include <stdio.h>
#include "types/cell.h"

typedef struct Reader Reader;
const char *readerr(Reader *reader);
Reader *ropen(FILE *input);
void rclose(Reader *reader);
Cell *reades(Arena *arena, Reader *reader);
void printes(Cell *cell);
