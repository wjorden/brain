#include "brainstem.h"
#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"
#include "interfaces/io/vision_payloads.h"
#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *BRAINSTEM_BUILD_ID = "brainstem|sig=2025-08-28";

typedef struct {
  uint64_t corr;
} corr_slot_t;

// emit helper
static bool emit(brainstem_t *bs, uint64_t now_ms, module_t src, module_t dst,
                 topic_t topic, pri_t pri, uint64_t corr,
                 const char *text /*nullable*/) {

  envelope_t out = {0};
  memset(&out, 0, sizeof out);

  out.magic = ENVELOPE_MAGIC;
  out.tail = ENVELOPE_TAIL;
  out.id = ++bs->next_id;
  out.ts_ms = now_ms;

  out.src = src;
  out.dst = dst;
  out.topic = (uint16_t)topic;
  out.pri = (uint8_t)pri;
  out.corr = corr;
  out.schema_v = 1;

  if (text && *text) {
    size_t L = strlen(text);
    if (L > sizeof(out.data))
      L = sizeof(out.data);
    memcpy(out.data, text, L);
    out.len = (uint32_t)L;
  }

  if (!rb_try_push(bs->out, &out))
    return false;

  size_t idx = bs->j_head % bs->j_cap;
  bs->journal[idx] = out;
  bs->j_head++;

  return true;
}

int brainstem_init(brainstem_t *bs, rb_t *in, rb_t *out,
                   size_t journal_capacity) {
  if (!bs || !in || !out)
    return -1;
  if ((journal_capacity & (journal_capacity - 1)) != 0)
    return -2;
  memset(bs, 0, sizeof &bs);
  bs->in = in;
  bs->out = out;
  bs->j_cap = journal_capacity ? journal_capacity : 256;
  bs->j_head = 0;
  bs->journal = (envelope_t *)calloc(bs->j_cap, sizeof(envelope_t));
  if (!bs->journal)
    return -1;
  bs->window_ms = 10000; // 10s window for reductions
  bs->autoclose_text = true;
  bs->next_id = 1;
  bs->applied_upto = 0; // watermark
  return 0;
}

void brainstem_free(brainstem_t *bs) {
  if (!bs)
    return;
  free(bs->journal);
  memset(bs, 0, sizeof *bs);
}

// m(now)
size_t brainstem_tick(brainstem_t *bs, uint64_t now_ms) {
  // keep count of emitted data
  size_t emitted = 0;
  // if window of oppertunity time = 0, reset to 10000
  if (bs->window_ms == 0)
    bs->window_ms = 10000;

  // 1. copy ring -> journal
  envelope_t ev;
  // add envelopes -> bs->in
  while (rb_try_pop(bs->in, &ev)) {
    // if magic numbers don't match
    if (ev.magic != ENVELOPE_MAGIC || ev.tail != ENVELOPE_TAIL) {
      // exit
      break;
    }

    // set idx to head % capacity (should be 0)
    size_t idx = bs->j_head % bs->j_cap;
    // do copy at journal[idx]
    bs->journal[idx] = ev;
    // move read location
    bs->j_head++;
  }

  // var inits for easier reading //
  // journal capacity
  const size_t cap = bs->j_cap;
  // freeze baseline this tick, journal head
  const size_t base = bs->j_head;
  // make sure there are envelopes
  if (base == bs->applied_upto)
    return 0;
  // old -> new
  const size_t span = (base > cap) ? cap : base;
  // say_open is TOP_DECISION_SAY, used to track when asked for a response
  bool open_any = false;
  // used for preventing duplicates
  uint64_t last_say_corr = 0;

  // read from oldest to newest
  for (size_t back = 0; back < span; ++back) {
    const envelope_t *e = &bs->journal[back];

    // make sure the envelope is current
    if (now_ms - e->ts_ms > bs->window_ms)
      break;

    // 3a - P1 input
    if (e->topic == TOP_DECISION_SAY) {
      open_any = true;
      last_say_corr = e->corr;
      break;
    }
  }

  if(bs->j_cap == 0) return 0;
  if(base == bs->applied_upto) return 0;


  // rules over new inputs, indices[applied_upto, base]
  for ( size_t i = 0; i < span; ++i) {
      size_t idx = (base - span + i);
    const envelope_t *e = &bs->journal[idx];
      // make sure the envelope is current
      if (now_ms - e->ts_ms > bs->window_ms)
          break;

      // starting index
      const size_t start = bs->applied_upto;
      size_t total_new = (base >= start) ? (base - start) : 0;
      if(total_new > cap) total_new = cap;
      for(size_t i = 0; i < total_new; ++i){
        idx = (base - span + i) & cap;
        e = &bs->journal[idx];
        if (e->topic == TOP_INPUT_TEXT && e->dst == MOD_BRAINSTEM)
      // interupt: PRI_P1
            if (e->pri <= PRI_P1) {
                if (last_say_corr) {
                    emitted += (emit(bs, now_ms, MOD_BRAINSTEM, MOD_PERIPHERY_VOICE,
                           TOP_CONTROL_SK, PRI_P1, e->corr, NULL));

                    emitted += (emit(bs, now_ms, MOD_BRAINSTEM, MOD_IO_TEXT,
                           TOP_OUTPUT_TEXT, PRI_P1, e->corr, "ack"));

                    emitted += (emit(bs, now_ms, MOD_BRAINSTEM, MOD_PERIPHERY_VOICE,
                           TOP_CONTROL_AR, PRI_P1, e->corr, NULL));

                    continue;
                }

        // Defer: P2 while speaking
                if (open_any && e->pri >= PRI_P2) {
                    emitted += (emit(bs, now_ms, MOD_BRAINSTEM, MOD_IO_TEXT,
                           TOP_CONTROL_AS, e->pri, e->corr, "stand by"));
                    continue;
                }
            continue;
        }

        // VISION EVENTS
        if (e->dst == MOD_BRAINSTEM && e->topic == TOP_VISION_MOTION &&
            e->schema_v == SCH_MOTION) {
            const pl_motion *m = (const pl_motion *)e->data;
            static uint8_t seen_motion = 0;
            if (!seen_motion && m->level >= 120) {
                emitted += emit(bs, now_ms, MOD_BRAINSTEM, MOD_IO_TEXT, TOP_OUTPUT_TEXT,
                        PRI_P2, e->corr, "motion detected");
                seen_motion = 1;
            } else if (seen_motion && m->level <= 30) {
                seen_motion = 0;
            }
        continue;
        }

        if (e->dst == MOD_BRAINSTEM && e->topic == TOP_VISION_STATS &&
            e->schema_v == SCH_STATS) {
            const pl_stats *s = (const pl_stats *)e->data;
            if (s->dark_pct >= 700) { // 70%
                emitted += emit(bs, now_ms, MOD_BRAINSTEM, MOD_IO_TEXT, TOP_OUTPUT_TEXT,
                        PRI_P2, e->corr, "lighting low");
            }
            continue;
        }

        // camera
        if (e->dst == MOD_BRAINSTEM && e->topic == TOP_FRAME_AVAILABLE &&
            e->schema_v == SCH_FRAME) {
            const pl_frame *f = (const pl_frame *)e->data;

            // handle yuyv output (MJPEG)
            emitted += emit(bs, now_ms, MOD_BRAINSTEM, MOD_IO_TEXT,
                      TOP_FRAME_AVAILABLE, PRI_P2, e->corr, "frame available");
        }
      }
    }
    bs->applied_upto = base;
    return emitted;
}
