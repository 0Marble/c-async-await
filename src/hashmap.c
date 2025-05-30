#include "hashmap.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hash_map_expand(HashMap *h) {
  uint64_t new_cap = h->cap * 2 + 10;
  h->cells = realloc(h->cells, new_cap * sizeof(HashNode *));
  memset(h->cells + h->cap, 0, (new_cap - h->cap) * sizeof(HashNode *));

  for (uint64_t cell = 0; cell < h->cap; cell++) {
    HashNode *prev = NULL;
    for (HashNode *n = h->cells[cell]; n;) {
      HashNode *next = n->next;
      uint64_t new_cell = n->hash % new_cap;
      if (new_cell != cell) {
        if (prev) {
          prev->next = next;
        } else {
          h->cells[cell] = next;
        }
        n->next = h->cells[new_cell];
        h->cells[new_cell] = n;
      } else {
        prev = n;
      }
      n = next;
    }
  }

  h->cap = new_cap;
}

HashNode *hash_map_find(HashMap *h, const char *key, int key_size) {
  if (h->cap == 0)
    return NULL;
  uint64_t hash = h->hash_fn(h->ctx, key, key_size);
  uint64_t cell = hash % h->cap;
  for (HashNode *n = h->cells[cell]; n; n = n->next) {
    if (n->key_size == key_size && n->hash == hash &&
        h->equality_fn(h->ctx, key, n->key, key_size) == 0) {
      return n;
    }
  }
  return NULL;
}

HashNode *hash_map_insert(HashMap *h, const char *key, int key_size) {
  if (h->node_max >= h->cap * 5) {
    hash_map_expand(h);
  }
  assert(h->cap != 0);
  uint64_t hash = h->hash_fn(h->ctx, key, key_size);
  uint64_t cell = hash % h->cap;

  HashNode *n = NULL;
  for (n = h->cells[cell]; n; n = n->next) {
    if (n->key_size == key_size && n->hash == hash &&
        h->equality_fn(h->ctx, key, n->key, key_size) == 0) {
      return n;
    }
  }
  if (h->free) {
    n = h->free;
    h->free = n->next;
  } else {
    n = calloc(1, sizeof(HashNode));
    assert(n);
    h->node_max++;
  }
  n->next = h->cells[cell];
  h->cells[cell] = n;

  n->hash = hash;
  n->key = key;
  n->key_size = key_size;
  return n;
}

HashNode *hash_map_remove(HashMap *h, const char *key, int key_size) {
  if (h->cap == 0)
    return NULL;

  uint64_t hash = h->hash_fn(h->ctx, key, key_size);
  uint64_t cell = hash % h->cap;
  HashNode *n = h->cells[cell];
  HashNode *prev = NULL;
  for (; n; prev = n, n = n->next) {
    if (n->key_size == key_size && n->hash == hash &&
        h->equality_fn(h->ctx, key, n->key, key_size) == 0) {
      break;
    }
  }

  if (!n) {
    return NULL;
  }

  HashNode *next = n->next;
  if (prev) {
    prev->next = next;
  } else {
    assert(n == h->cells[cell]);
    h->cells[cell] = next;
  }
  n->next = h->free;
  h->free = n->next;

  return n;
}

void hash_map_deinit(HashMap *h, HashNodeDeinitCallback *hash_node_deinit) {
  for (uint64_t i = 0; i < h->cap; i++) {
    for (HashNode *n = h->cells[i]; n;) {
      HashNode *next = n->next;
      if (hash_node_deinit) {
        hash_node_deinit(n);
      }
      free(n);
      n = next;
    }
  }
  for (HashNode *n = h->free; n;) {
    HashNode *next = n->next;
    free(n);
    n = next;
  }
  free(h->cells);
}
