#include "brain/brainstem/brainstem.h"
#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

static void must_push(rb_t *q, envelope_t *e) {
  int tries = 0;
  while (!rb_try_push(q, e)) {
    fprintf(stderr, "[EMIT DROP] topic=%u corr=%llu (out full?)\n",
            (unsigned)e->topic, (unsigned long long)e->corr);
    if (++tries > 10)
      puts("[PUSH] failed");
    _exit(2);
  }
}

extern const char *BRAINSTEM_BUILD_ID;

int main(void) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("[SIG] confirm Brainstem lib sig: %s\n", BRAINSTEM_BUILD_ID);

  // all ring buffer stuff
  printf("[RB] create buffer(s)\n");
  // ring buffers
  rb_t *in = rb_create(256, sizeof(envelope_t));
  rb_t *out = rb_create(256, sizeof(envelope_t));
  printf("[CHECK] sizeof(envelope_t)=%zu\n", sizeof(envelope_t));
  // get ring addresses
  fprintf(stderr, "[RB] addresses: in=%p out=%p\n", (void *)in, (void *)out);

  // all brainstem stuff
  printf("[BS] init\n");
  brainstem_t bs;
  if (brainstem_init(&bs, in, out, 256) != 0) {
    fprintf(stderr, "[BS] init failed!\n");
    return 1;
  }
  // confirm ring address is passed
  fprintf(stderr, "[RB] confirm addresses: in=%p out=%p jcap=%zu\n",
          (void *)bs.in, (void *)bs.out, bs.j_cap);

  // starting
  uint64_t t0 = now_ms();
  printf("[START] time is: %zu\n", t0);
  printf("[START] assigning test corr values\n");
  uint64_t corr1 = 1001, corr2 = 1002, corr3 = 1003;
  printf("[SHOW] assigned values: corr1=%zu corr2=%zu corr3=%zu\n", corr1,
         corr2, corr3);

  // check enum values for cross reference
  fprintf(
      stderr,
      "[ENUMS] INPUT=%u SAY=%u AS=%u OUT=%u SK=%u AR=%u P0=%u P1=%u P2=%u\n",
      TOP_INPUT_TEXT, TOP_DECISION_SAY, TOP_CONTROL_AS, TOP_OUTPUT_TEXT,
      TOP_CONTROL_SK, TOP_CONTROL_AR, PRI_P0, PRI_P1, PRI_P2);

  /* sim ongoing utterance
      - corr1 (DECISION_SAY)
      - corr2 normal while busy
      - corr3 priority stop
  */
  printf("[Demo 1] initializing envelope\n");
  envelope_t e = {0};
  // start a "say"
  e.magic = ENVELOPE_MAGIC;
  e.tail = ENVELOPE_TAIL;
  e.id = 1;
  e.ts_ms = now_ms();

  e.src = MOD_PERIPHERY_VOICE;
  e.dst = MOD_BRAINSTEM;
  e.topic = TOP_DECISION_SAY;
  e.pri = PRI_P1;
  e.corr = corr1;
  e.schema_v = 1;
  e.len = (uint32_t)snprintf((char *)e.data, sizeof e.data, "status");

  printf("[SEND] demo envelope\n");
  must_push(in, &e);
  fprintf(stderr,
          "[SENT] magic=%08x tail=%08x topic=%d pri=%d corr=%llu ms=%u\n",
          e.magic, e.tail, e.topic, e.pri, (unsigned long long)e.corr,
          (unsigned)e.ts_ms);

  printf("[Demo 2] initializing envelope\n");
  // new non-priority
  e.magic = ENVELOPE_MAGIC;
  e.tail = ENVELOPE_TAIL;
  e.id = 2;
  e.ts_ms = now_ms();

  e.src = MOD_IO_TEXT;
  e.dst = MOD_BRAINSTEM;
  e.topic = TOP_INPUT_TEXT;
  e.pri = PRI_P2;
  e.corr = corr2;
  e.schema_v = 1;
  e.len = (uint32_t)snprintf((char *)e.data, sizeof e.data, "help");
  printf("[SEND] demo envelope\n");
  must_push(in, &e);
  fprintf(stderr, "[SENT] magic=%08x tail=%08x topic=%d pri=%d corr=%llu\n",
          e.magic, e.tail, e.topic, e.pri, (unsigned long long)e.corr);

  printf("[Demo 3] initializing envelope\n");
  // new priority stop
  e.magic = ENVELOPE_MAGIC;
  e.tail = ENVELOPE_TAIL;
  e.id = 3;
  e.ts_ms = now_ms();

  e.src = MOD_IO_TEXT;
  e.dst = MOD_BRAINSTEM;
  e.topic = TOP_INPUT_TEXT;
  e.pri = PRI_P1;
  e.corr = corr3;
  e.schema_v = 1;
  e.len = (uint32_t)snprintf((char *)e.data, sizeof e.data, "stop");
  printf("[SEND] demo envelope\n");
  must_push(in, &e);
  fprintf(stderr,
          "[SENT] magic=%08x tail=%08x len=%u topic=%d pri=%d corr=%llu\n",
          e.magic, e.tail, e.len, e.topic, e.pri, (unsigned long long)e.corr);

  // check ring address for envelope count
  printf("[RB_IN] ring size=%u\n", (unsigned int)rb_size_approx(in));
  size_t n = brainstem_tick(&bs, now_ms());

  printf("[DONE] %zu emitted\n", n);

  printf("[RB_OUT] ring size=%u\n", (unsigned int)rb_size_approx(out));

  printf("[END] freeing memory\n");
  brainstem_free(&bs);
  rb_destroy(in), rb_destroy(out);
  (void)t0;
  return 0;
}
