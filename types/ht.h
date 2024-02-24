/* intrusive single probing resizable hash table / FNV-la hash */
/* depends:
#include <stdlib.h>
*/
#define HT_INI_CAP 16		/* must be power of 2 */
#define HT_MIN_LOAD_FAC 0.65f
#define HT_OVERLOAD(ht) ((ht->len / (float)ht->cap) > HT_MIN_LOAD_FAC)

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Return 64-bit FNV-1a hash for key (NUL-terminated). See description:
// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static inline uint64_t
hash_key(const char *key)
{
    uint64_t hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

typedef struct {} HTLink; 	/* embed `HTlnk link' into your struct */

typedef struct {
	const char *key;
	HTLink *link;
} HTEntry;

typedef struct {
	size_t cap;
	size_t len;
	void (*del)(HTLink *);	/* set this for eventual free */
	HTEntry *entr;
} HT;

#define ht_ini() ht_init(HT_INI_CAP)
static inline HT *
ht_init(size_t size)
{
	HT *ht = malloc(sizeof(HT));
	ht->entr = calloc(size, sizeof(HTEntry));
	ht->cap = size;
	ht->del = (void*)0;
	ht->len = 0;
	return ht;
}

static inline size_t
ht_find_idx(HTEntry *entr, size_t cap, const char *key)
{
	uint64_t hash = hash_key(key);
	size_t idx = (size_t)(hash & (uint64_t)(cap - 1)); /* fast modulo */
	while (entr[idx].link) {
		if (!strcmp(key, entr[idx].key)) return idx;
		if (++idx >= cap) idx = 0;
	}
	return idx;
}

static inline void
ht_free_idx(HT *ht, size_t idx)
{
	if (!ht->entr[idx].link) return;
	free((char*)ht->entr[idx].key);
	if (ht->del) ht->del(ht->entr[idx].link);
	ht->entr[idx].link = (void*)0;
}

static inline HTLink * /* use `entryof' from aux.h for actual addres */
ht_get(HT *ht, const char *key)
{
	return ht->entr[ht_find_idx(ht->entr, ht->cap, key)].link;
} /* entryof(ht_get(ht, "key"), StructType, name_of_HTLink_field) */

static inline void
ht_ensure(HT *ht)
{
	if (!HT_OVERLOAD(ht)) return;
	HTEntry *entr = calloc(ht->cap << 1, sizeof(HTEntry));
	for (size_t i = 0; i < ht->cap; i++) {
		if (!ht->entr[i].link) continue;
		size_t idx = ht_find_idx(entr, ht->cap << 1, ht->entr[i].key);
		entr[idx] = ht->entr[i];
	}
	free(ht->entr);
	ht->cap <<= 1;
	ht->entr = entr;
}

static inline void
ht_set(HT *ht, const char *key, HTLink *link)
{
	size_t idx = ht_find_idx(ht->entr, ht->cap, key);
	if (!ht->entr[idx].link) ht->len++;
	ht_free_idx(ht, idx);
	ht->entr[idx].key = strdup(key);
	ht->entr[idx].link = link;
	ht_ensure(ht);
}

static inline void
ht_del(HT *ht, const char *key)
{
	ht_free_idx(ht, ht_find_idx(ht->entr, ht->cap, key));
	ht->len--;
}

static inline void
ht_free(HT *ht)
{
	for (size_t i = 0; i < ht->cap; i++) ht_free_idx(ht, i);
	free(ht->entr);
	free(ht);
}
