#include "async.h"
#include "hashmap.h"
#include "scheduler.h"
#include "storage.h"
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
  struct Node *parent;
  Handle h;
  bool in_queue;
} Node;

typedef struct {
  Node **elems;
  int start, end, cap;
} NodeQueue;

typedef struct {
  NodeQueue queue;
  HashMap handle_to_node;
} Graph;

void verify_queue(NodeQueue *q) {
#ifdef DEBUG
  assert(q);
  assert(q->start >= 0);
  assert(q->end <= q->cap);
  assert(q->end >= 0);
  assert(q->start <= q->end);
  if (q->cap)
    assert(q->elems);
  for (int i = q->start; i < q->end; i++) {
    assert(q->elems[i]);
    assert(q->elems[i]->in_queue);
    assert(q->elems[i]->h.idx);
    for (int j = i + 1; j < q->end; j++) {
      assert(q->elems[i] != q->elems[j]);
    }
  }
#endif
}

void graph_register_task(void *data, Handle h) {
  assert(h.idx);
  Graph *g = data;
  HashNode *hash_node =
      hash_map_insert(&g->handle_to_node, (void *)(long)h.idx, sizeof(int));
  Node *n = NULL;
  if (hash_node->val == NULL) {
    n = calloc(1, sizeof(Node));
    hash_node->val = (void *)n;
  }
  n->h = h;
  n->parent = NULL;
  n->in_queue = true;

  verify_queue(&g->queue);
  queue_push_back(&g->queue, n);
  verify_queue(&g->queue);

  hash_node->val = (void *)n;
}

void graph_finish_task(void *data, Handle *current, Handle *next) {
  Graph *g = data;
  Node *cur_node = NULL;
  verify_queue(&g->queue);
  queue_pop_front(&g->queue, &cur_node);
  verify_queue(&g->queue);

  assert(cur_node);
  assert(cur_node->in_queue);
  assert(cur_node->h.idx);
  cur_node->in_queue = false;
  *current = cur_node->h;
  Node *parent = cur_node->parent;
  if (parent && !parent->in_queue) {
    *next = parent->h;
    parent->in_queue = true;

    verify_queue(&g->queue);
    queue_push_front(&g->queue, parent);
    verify_queue(&g->queue);
  } else {
    Node *next_node = NULL;
    verify_queue(&g->queue);
    queue_peek_front(&g->queue, &next_node);
    verify_queue(&g->queue);
    assert(next_node);
    assert(next_node->in_queue);
    assert(next_node->h.idx);
    *next = next_node->h;
  }
}

void graph_free_task(void *data, Handle h) {
  Graph *g = data;
  HashNode *hash_node =
      hash_map_remove(&g->handle_to_node, (void *)(long)h.idx, sizeof(int));
  assert(hash_node);
  Node *n = (void *)hash_node->val;
  assert(n);
  assert(!n->in_queue);
  assert(n->h.idx);
  n->h.idx = 0;
  n->in_queue = false;
}

State graph_poll_task(void *data, Handle h) {
  assert(h.idx);
  return global_pool()->tasks[h.idx - 1].state;
}

void graph_wait_ready(void *data, Handle h) {
  Graph *g = data;
  Node *n = NULL;
  verify_queue(&g->queue);
  queue_pop_front(&g->queue, &n);
  verify_queue(&g->queue);
  assert(n);
  assert(n->in_queue);
  assert(n->h.idx);
  n->in_queue = false;
  HashNode *hash_node =
      hash_map_find(&g->handle_to_node, (void *)(long)h.idx, sizeof(int));
  assert(hash_node);
  Node *child = (void *)hash_node->val;

  assert(!child->parent);
  assert(child->h.idx == h.idx);
  assert(child->h.idx);
  child->parent = n;
  if (!child->in_queue) {
    child->in_queue = true;
    verify_queue(&g->queue);
    queue_push_front(&g->queue, child);
    verify_queue(&g->queue);
  }

  async_skip();
  assert(current_task_handle().idx == n->h.idx);
  assert(poll_state(child->h) == READY);
}

Handle graph_current_task(void *data) {
  Graph *g = data;
  Node *n = NULL;
  verify_queue(&g->queue);
  queue_peek_front(&g->queue, &n);
  verify_queue(&g->queue);
  assert(n);
  assert(n->in_queue);
  assert(n->h.idx);
  return n->h;
}

Handle graph_next_task(void *data) {
  Graph *g = data;
  Node *cur = NULL;
  verify_queue(&g->queue);
  queue_pop_front(&g->queue, &cur);
  verify_queue(&g->queue);
  assert(cur);
  assert(cur->in_queue);
  assert(cur->h.idx);
  verify_queue(&g->queue);
  queue_push_back(&g->queue, cur);
  verify_queue(&g->queue);
  Node *next = NULL;
  verify_queue(&g->queue);
  queue_peek_front(&g->queue, &next);
  verify_queue(&g->queue);
  assert(next);
  assert(next->in_queue);
  return next->h;
}

void graph_hash_node_deinit(HashNode *n) { free(n->val); }
void graph_cleanup(void *data) {
  Graph *g = data;
  hash_map_deinit(&g->handle_to_node, graph_hash_node_deinit);
  if (g->queue.cap)
    free(g->queue.elems);
}

static Graph graph = {0};
static SchedulerVTable vtable = {
    .register_task = graph_register_task,
    .finish_task = graph_finish_task,
    .free_task = graph_free_task,
    .poll_task = graph_poll_task,
    .wait_ready = graph_wait_ready,
    .current_task = graph_current_task,
    .next_task = graph_next_task,
    .cleanup = graph_cleanup,
};

int int_eql_fn(void *ctx, const char *x, const char *y, int size) {
  assert(size == sizeof(int));
  return (int)(long)x - (int)(long)y;
}

uint64_t int_hash_fn(void *ctx, const char *x, int size) {
  assert(size == sizeof(int));
  return (int)(long)x;
}

void use_graph_scheduler() {
  graph.handle_to_node.equality_fn = int_eql_fn;
  graph.handle_to_node.hash_fn = int_hash_fn;
  Scheduler *s = global_scheduler();
  s->data = &graph;
  s->vtable = &vtable;
}
