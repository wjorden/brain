// rate limiter/forwarder

#include "thalamus_vision.h"
#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"
#include <stdio.h>

void thalamus_vision_tick(thalamus_vision_t *tv) {
  envelope_t e;
  while (rb_try_pop(tv->qin, &e)) {
    tv->count++;
    // catch all frames
    if (tv->count % (tv->stride ? tv->stride : 3) == 0) {
      e.src = MOD_THALAMUS_VISION;
      e.dst = MOD_VISUAL_CORTEX;
      printf("[PUSH] elem_size=%u want=%zu topic=%u len=%u magic=0x%08x\n",
             (unsigned)tv->qout->elem_size, sizeof(envelope_t), e.topic, e.len, e.magic);
      rb_try_push(tv->qout, &e);
      printf("[POP] elem_size=%u want=%zu topic=%u len=%u magic=0x%08X\n",
             (unsigned)tv->qout->elem_size, sizeof(envelope_t), e.topic, e.len, e.magic);
    }
  }
}
