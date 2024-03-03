#define BYTE_COL  "5"
#define WHERE_COL "7"
#define CODE_COL  "11"
#define ARGS_COL  "10"

#define DECOMP_HEADER                                                          \
	";;; %s\n; %-" BYTE_COL "s ; %-" WHERE_COL "s ; %-" CODE_COL           \
	"s ; %-" ARGS_COL "s"

ptrdiff_t decompile_op(Chunk *chunk, ptrdiff_t offset);
void decompile(Chunk *chunk, const char *name);
