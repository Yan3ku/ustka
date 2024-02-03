#include <stdlib.h>
#ifndef VEC_H
#define VEC_H

#define VEC_GROW 1.5

typedef struct {
	size_t cap;
	size_t len;
} Vec_;

#define Vec(type) type *
#define VEC(type, name) type *name = vec_ini(name)
#define vecptr(data) ((Vec_ *)(data)-1)
#define vecdata(vec) ((void *)((Vec_ *)(vec) + 1))

#define vec_siz(data) (sizeof(Vec_) + sizeof(*data) + vecptr(data)->cap)
#define vec_len(data) (vecptr(data)->len)
#define vec_cap(data) (vecptr(data)->cap)

#define vec_ini(data) vec_init(data, 16)
#define vec_init(data, siz) vec_init_((void **)&data, sizeof(*data), siz)
static inline void *
vec_init_(void **data, size_t els, size_t siz)
{
	*data = vecdata(malloc(sizeof(Vec_) + els * siz));
	vecptr(*data)->cap = siz;
	vecptr(*data)->len = 0;
	return *data;
}


#define vec_free(data)                                                         \
	do {                                                                   \
		free(vecptr(data));                                            \
		data = NULL;                                                   \
	} while (0)

#define vec_ensure(data, siz)                                                  \
	do {                                                                   \
		if (vecptr(data)->len + (siz) > vecptr(data)->cap) {           \
			vecptr(data)->cap = vecptr(data)->len * VEC_GROW;      \
			if (vecptr(data)->len + (siz) > vecptr(data)->cap)     \
				vecptr(data)->cap = vecptr(data)->len + (siz); \
			data = vecdata(realloc(vecptr(data), vec_siz(data)));  \
		}                                                              \
	} while (0)

#define vec_push(data, el)                                                     \
	do {                                                                   \
		vec_ensure(data, 1);                                           \
		data[vec_len(data)++] = el;                                    \
	} while (0)
#endif
