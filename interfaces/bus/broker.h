#pragma once
#include "rb.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct broker {
  rb_t *in;    // main input
  rb_t **outs; // array of subscriber rings
  size_t n_outs;
  // telemetry
  _Atomic size_t forwarded;
  _Atomic size_t drops; // total drops (to dsts) across outs
} broker_t;

/* outs[] must point to SPSC rings
  - with same elem_size as 'in'
*/
broker_t *broker_create(rb_t *in, rb_t **outs, size_t n_outs);
void broker_destroy(broker_t *b);

/* fan-out one batch
  - returns number events forwarded this call
  - drops to any out that is full (non-blocking)
  - increments b->drops
*/
size_t broker_pump(broker_t *b);
