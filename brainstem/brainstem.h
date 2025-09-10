#pragma once

#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  rb_t *in;  // brainstem ingress (SPSC ring)
  rb_t *out; // brainstem egress
  envelope_t *journal;
  size_t j_cap;     // journal capacity
  size_t j_head;    // next write index
  uint64_t next_id; // id generator for emitted envelopes

  // watermark for tracking new events
  size_t applied_upto;
  // tunables
  uint32_t window_ms;  // reduction window
  bool autoclose_text; // auto-emit AR after OUTPUT_TEXT
} brainstem_t;

int brainstem_init(brainstem_t *bs, rb_t *in, rb_t *out,
                   size_t journal_capacity);
void brainstem_free(brainstem_t *bs);

/* Tick drains input -> return number enevlopes emitted/tick
  - append to journal
  - apply rules
  - emit to out
*/
size_t brainstem_tick(brainstem_t *bs, uint64_t now_ms);

#ifdef __cplusplus
}
#endif
