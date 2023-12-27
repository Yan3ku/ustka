#include <stdlib.h>

/* https://stackoverflow.com/a/20481237/22903728 */
#define VEC_GROW 1.5

typedef struct {
	size_t cap;
	size_t len;
} Vec_;

#define Vec(type) type *
#define vecptr(data) ((Vec_ *)(data) - 1)
#define vecdata(vec) ((void *)((Vec_ *)(vec) + 1))

#define vec_siz(data) (sizeof(Vec_) + sizeof(*data) + vecptr(data)->cap)
#define vec_len(data) (vecptr(data)->len)
#define vec_cap(data) (vecptr(data)->cap)

#define vec_ini(data) vec_init(data, 16)
#define vec_init(data, siz)                                                    \
	do {                                                                   \
		data = vecdata(malloc(sizeof(Vec_) + sizeof(*data) * siz));    \
		vecptr(data)->cap = siz;                                       \
		vecptr(data)->len = 0;                                         \
	} while (0)

#define vec_free(data)                                                         \
	do {                                                                   \
		free(vecptr(data));                                            \
		data = NULL;                                                   \
	} while (0)

#define vec_ensure(data, siz)                                                  \
	do {                                                                   \
		if (vecptr(data)->len + siz > vecptr(data)->cap) {             \
			vecptr(data)->cap = vecptr(data)->len * VEC_GROW;      \
			if (vecptr(data)->len + siz > vecptr(data)->cap)       \
				vecptr(data)->cap = vecptr(data)->len + siz;   \
			data = vecdata(realloc(vecptr(data), vec_siz(data)));  \
		}                                                              \
	} while (0)

#define vec_push(data, el)                                                     \
	do {                                                                   \
		vec_ensure(data, 1);                                           \
		data[vec_len(data)++] = el;                                    \
	} while (0)
