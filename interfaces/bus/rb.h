#pragma once
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__STDC_VERSION__) && __STDC__VERSION__ >= 201112L
#define RB_ALIGN(N) _Alignas(N)
#else
#define RB_ALIGN(N)
#endif

typedef struct rb {   // ring buffer
  size_t cap;         // capacity in elements pow(2)
  size_t mask;        // cap - 1 (for & indexing)
  size_t elem_size;   // bytes per element
  unsigned char *buf; // storage: cap * elem_size

  _Atomic size_t head; // next write index
  _Atomic size_t tail; // next read index
} rb_t;

/* Create with capacity >= 2
  - round up to power of 2
  - return NULL on alloc failure or invalid args
*/
rb_t *rb_create(size_t capacity, size_t elem_size);

/* Free ring and storage */
void rb_destroy(rb_t *q);

/* Return true (success): false (full)
  - no overwrite
*/
bool rb_try_push(rb_t *q, const void *item);

/* Return true (success): false (empty) */
bool rb_try_pop(rb_t *q, void *out);

/* Approx size (safe for telemetry, may cause race) */
size_t rb_size_approx(const rb_t *q);

/* True if empty (best effort) */
static inline bool rb_empty(const rb_t *q) {
  size_t t = atomic_load_explicit(&q->tail, memory_order_acquire);
  size_t h = atomic_load_explicit(&q->head, memory_order_acquire);
  return (h - t) == 0;
}

/* True if full (best effort) */
static inline bool rb_full(const rb_t *q) {
  size_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
  size_t h = atomic_load_explicit(&q->head, memory_order_acquire);
  return (h - t) == 0;
}
