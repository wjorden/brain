#include "broker.h"
#include "event.h"
#include "rb.h"
#include <stdlib.h>

broker_t *broker_create(rb_t *in, rb_t **outs, size_t n_outs) {
  if (!in || !outs || n_outs == 0)
    return NULL;
  broker_t *b = (broker_t *)calloc(1, sizeof(broker_t));
  if (!b)
    return NULL;
  b->in = in;
  b->outs = (rb_t **)calloc(n_outs, sizeof(rb_t *));
  for (size_t i = 0; i < n_outs; i++)
    b->outs[i] = outs[i];
  b->n_outs = n_outs;
  atomic_store(&b->forwarded, 0);
  atomic_store(&b->drops, 0);
  return b;
}

void broker_destroy(broker_t *b) {
  if (!b)
    return;
  free(b->outs);
  free(b);
}

size_t broker_pump(broker_t *b) {
  event_t ev;
  size_t n = 0;
  // pop as many as available this turn (may cap if needed)
  while (rb_try_pop(b->in, &ev)) {
    // fan to outs; drop for those full
    for (size_t i = 0; i < b->n_outs; ++i) {
      if (!rb_try_push(b->outs[i], &ev)) {
        atomic_fetch_add(&b->drops, 1);
      }
    }
    n++;
  }
  atomic_fetch_add(&b->forwarded, n);
  return n;
}