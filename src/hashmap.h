#ifndef __HASHMAP_H__
#define __HASHMAP_H__

#include <inttypes.h>

typedef struct HashNode {
  struct HashNode *next;
  const char *key;
  int key_size;
  char *val;
  uint64_t hash;
} HashNode;

typedef int EqualityFn(void *ctx, const char *x, const char *y, int len);
typedef uint64_t HashFn(void *ctx, const char *x, int len);

typedef struct {
  HashNode **cells;
  HashNode *free;
  uint64_t cap;
  int node_max;
  EqualityFn *equality_fn;
  HashFn *hash_fn;
  void *ctx;
} HashMap;

void hash_map_expand(HashMap *h);
HashNode *hash_map_find(HashMap *h, const char *key, int key_size);
HashNode *hash_map_insert(HashMap *h, const char *key, int key_size);
HashNode *hash_map_remove(HashMap *h, const char *key, int key_size);

typedef void HashNodeDeinitCallback(HashNode *n);
void hash_map_deinit(HashMap *h, HashNodeDeinitCallback *);

#endif // !__HASHMAP_H__
