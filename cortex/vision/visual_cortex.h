#pragma once

#include "thalamus/vision/frame_pool.h"
#include "interfaces/bus/rb.h"
#include <stdint.h>

typedef struct {
  frame_pool_t *pool;
  rb_t *qin;  // from thalamus
  rb_t *qout; // to brainstem
  // prev downscaled buffer for motion (gray8 160x120)
  uint16_t dw, dh;
  uint8_t prev[160 * 120];
  int has_prev;
} visual_cortex_t;

static inline void visual_cortex_init(visual_cortex_t *vc, frame_pool_t *fp,
                                      rb_t *qin, rb_t *qout) {
  vc->pool = fp;
  vc->qin = qin;
  vc->qout = qout;
  vc->dw = vc->dh = 0;
  vc->has_prev = 0;
}

void visual_cortex_tick(visual_cortex_t *vc, uint64_t now_ms);
