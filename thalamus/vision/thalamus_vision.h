#pragma once

#include "interfaces/bus/rb.h"
#include <stdint.h>

typedef struct {
  rb_t *qin;       // from camera
  rb_t *qout;      // to visual cortex
  uint32_t stride; // every nth frame
  uint32_t count;  // i shouldn't need to tell you :)
} thalamus_vision_t;

static inline void thalamus_init(thalamus_vision_t *tv, rb_t *qin, rb_t *qout,
                                 uint32_t stride) {
  tv->qin = qin;
  tv->qout = qout;
  tv->stride = stride ? stride : 3;
  tv->count = 0;
}

void thalamus_vision_tick(thalamus_vision_t *tv);
