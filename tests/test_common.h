#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "brain/brainstem/brainstem.h"
#include "interfaces/bus//rb.h"
#include "interfaces/bus/envelope.h"

// global mock clock
extern uint64_t g_now_ms;
static inline uint64_t now_ms(void) { return g_now_ms; }

// make an envelope
static inline envelope_t make_in(module_t src, topic_t top, pri_t pri,
                                 uint64_t corr, const char *text) {
  envelope_t e = {0};
  e.magic = ENVELOPE_MAGIC;
  e.tail = ENVELOPE_TAIL;
  e.id = 0;
  e.ts_ms = now_ms();

  e.src = src;
  e.dst = MOD_BRAINSTEM;
  e.topic = top;
  e.pri = pri;
  e.corr = corr;
  e.schema_v = 1;
  if (text) {
    size_t L = strlen(text);
    if (L > sizeof e.data)
      L = sizeof e.data;
    memcpy(e.data, text, L);
    e.len = (uint32_t)L;
  }
  return e;
}

static inline void must_push(rb_t *q, envelope_t *e) {
  bool ok = rb_try_push(q, e);
  assert(ok && "ring push failed in test");
  (void)ok;
}

static inline size_t pop_all(rb_t *q, envelope_t *out, size_t cap) {
  size_t n = 0;
  envelope_t tmp;
  while (n < cap && rb_try_pop(q, &tmp)) {
    out[n++] = tmp;
  }
  return n;
}

static inline void bs_setup(brainstem_t *bs, rb_t **qin, rb_t **qout,
                            size_t jcap, size_t rcap) {
  *qin = rb_create(rcap, sizeof(envelope_t));
  *qout = rb_create(rcap, sizeof(envelope_t));
  assert(*qin && *qout);
  int rc = brainstem_init(bs, *qin, *qout, jcap);
  assert(rc == 0);
  (void)rc;
  bs->window_ms = 10000;
  bs->autoclose_text = true;
}

static inline void bs_teardown(brainstem_t *bs, rb_t *qin, rb_t *qout) {
  brainstem_free(bs);
  rb_destroy(qout);
  rb_destroy(qin);
}
