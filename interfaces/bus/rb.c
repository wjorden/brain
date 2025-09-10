#include "rb.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t next_pow2(size_t x) {
  size_t p = 1;
  while (p < x)
    p <<= 1;
  return p;
}

rb_t *rb_create(size_t capacity, size_t elem_size) {
  if (capacity < 2 || elem_size == 0)
    return NULL;

  rb_t *q = (rb_t *)malloc(sizeof(rb_t));
  if (!q)
    return NULL;

  q->cap = next_pow2(capacity);
  q->mask = q->cap - 1;
  q->elem_size = elem_size;

  size_t bytes = q->cap * q->elem_size;

#if defined(_MSC_VER)
  void *p = NULL;
  p = _aligned_malloc(bytes, 64);
  if (!p) {
    free(q);
    return NULL;
  }
  q->buff = (unsigned char *)p;
#elif defined(_POSIX_VERSION)
  void *p = NULL;
  if (posix_memalign(&p, 64, bytes) != 0) {
    free(q);
    return NULL;
  }
  q->buf = (unsigned char *)p;
#else
  q->buf = (unsigned char *)malloc(bytes);

  if (!q->buf) {
    free(q);
    return NULL;
  }
#endif

  memset(q->buf, 0, bytes);
  atomic_init(&q->head, 0);
  atomic_init(&q->tail, 0);
  return q;
}

void rb_destroy(rb_t *q) {
  if (!q)
    return;
#if defined(_MSC_VER)
  _aligned_free(q->buf)
#else
  free(q->buf);
#endif
      free(q);
}

bool rb_try_push(rb_t *q, const void *item) {
  size_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
  size_t h = atomic_load_explicit(&q->head, memory_order_acquire);
  if ((h - t) >= q->cap)
    return false;

  size_t pos = h & q->mask;
  unsigned char *dst = q->buf + pos * q->elem_size;
  memcpy(dst, item, q->elem_size);

  atomic_store_explicit(&q->head, h + 1, memory_order_release);
  return true;
}

bool rb_try_pop(rb_t *q, void *out) {
  size_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
  size_t h = atomic_load_explicit(&q->head, memory_order_acquire);
  if (h == t)
    return false; // empty

  size_t pos = t & q->mask;
  unsigned char *src = q->buf + pos * q->elem_size;
  memcpy(out, src, q->elem_size);

  atomic_store_explicit(&q->tail, t + 1, memory_order_release);
  return true;
}

size_t rb_size_approx(const rb_t *q) {
  size_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
  size_t h = atomic_load_explicit(&q->head, memory_order_acquire);
  return h - t;
}
