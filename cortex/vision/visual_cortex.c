#ifdef WITH_VIZ
#include "output/my_imgui.h"
#endif
#include "visual_cortex.h"
#include "thalamus/vision/frame_pool.h"
#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"
#include "interfaces/io/vision_payloads.h"
#include <stdio.h>
#include <string.h>

static void downscale_box(const uint8_t *src, uint16_t sw, uint16_t sh,
                          uint8_t *dst, uint16_t dw, uint16_t dh) {
  // cheap filter
  for (uint16_t y = 0; y < dh; ++y) {
    uint32_t sy0 = (uint32_t)y * sh / dh;
    uint32_t sy1 = (uint32_t)(y + 1) * sh / dh;
    if (sy1 == sy0)
      sy1++;
    for (uint16_t x = 0; x < dw; ++x) {
      uint32_t sx0 = (uint32_t)x * sw / dw;
      uint32_t sx1 = (uint32_t)(x + 1) * sw / dw;
      if (sx1 == sx0)
        sx1++;
      uint32_t sum = 0, cnt = 0;
      for (uint32_t yy = sy0; yy < sy1; ++yy)
        for (uint32_t xx = sx0; xx < sx1; ++xx) {
          sum += src[yy * sw + xx];
          cnt++;
        }
      dst[y * dw + x] = (uint8_t)(sum / (cnt ? cnt : 1));
    }
  }
}

void visual_cortex_tick(visual_cortex_t *vc, uint64_t now_ms) {
  envelope_t e;
  while (rb_try_pop(vc->qin, &e)) {
    // make sure we can handle the data
    if ((e.topic != TOP_FRAME_AVAILABLE && e.schema_v != SCH_FRAME) &&
        (e.topic != TOP_HEALTH_VISION && e.schema_v != SCH_HEALTH)) {
      printf("[VISUAL CORTEX] wrong topic=%u/schema=%u combination\n",
              e.topic, e.schema_v);
      break;
    }

    pl_frame pl;
    memcpy(&pl, e.data, sizeof(pl));

    uint8_t *pix;
    uint32_t bytes;
    printf("[VC] mapping...\n");
    if (fp_map(vc->pool, pl.frame_id, &pix, &bytes) != 0) {
      // stale or invalid
      e.src = MOD_VISUAL_CORTEX;
      e.dst = MOD_BRAINSTEM;
      e.ts_ms = now_ms;
      pl_health hh = {.kind = 2, .value = 1, .aux = pl.frame_id};
      e.len = sizeof(hh);
      memcpy(e.data, &hh, sizeof(hh));
      printf("[PUSH] elem_size=%lu want=%zu topic=%u len=%u magic=0x%08X\n",
             (unsigned long)vc->qout->elem_size, sizeof(envelope_t), e.topic, e.len, e.magic);
      rb_try_push(vc->qout, &e);
      printf("[POP] elem_size=%lu want=%zu topic=%u len=%u magic=0x%08X\n",
             (unsigned long)vc->qout->elem_size, sizeof(envelope_t), e.topic, e.len, e.magic);
      continue;
    }

    // downscale to at most 160x120
    uint16_t dw = (pl.w > 160) ? 160 : pl.w;
    uint16_t dh = (pl.h > 120) ? 120 : pl.h;
    uint8_t ds[160 * 120];
    downscale_box(pix, pl.w, pl.h, ds, dw, dh);

    bytes = (uint32_t)dw * (uint32_t)dh;

#ifdef WITH_VIZ
    static utin_64t last_ms = 0;
    static float fps_avg = 0.0f;
    float fps_now = 0.0f;

    if(last_ms){
        uint64_t dt = now_ms - last_ms;
        if(dt){
            fps_now = 1000.0f/ (float)dt;
            fps_avg = (fps_avg > 0.0f) ? (fps_avg * 0.9f + fps_now * 0.1f) : fps_now;
        }
    }
    last_ms = now_ms;

    imgui_vision_gray8(ds, (int)dw, (int)dh, (int)dw);

    cam_meta_t m = {
        .w = dw,
        .h = dh,
        .stride = dw,
        .fourcc = pl.fmt,
        .fps_now = fps_now,
        .fps_avg = fps_avg,
        .frame_id = pl.frame_id
    };
    imgui_set_meta(&m);
#endif
    printf("[VISUAL CORTEX] x11 path w=%u h=%u bytes=%u\n",
            (unsigned)pl.w, (unsigned)pl.h, (unsigned)bytes);

    // stats
    uint64_t sum = 0, sum2 = 0;
    uint32_t dark = 0, sat = 0;
    uint32_t total = (uint32_t)dw * dh;
    for (uint32_t i = 0; i < total; i++) {
      uint8_t v = ds[i];
      sum += v;
      sum2 += (uint32_t)v * v;
      if (v < 16)
        dark++;
      if (v > 240)
        sat++;
    }
    uint32_t avg = (uint32_t)((sum * 4) / (total ? total : 1));
    uint32_t var = (uint32_t)((sum2 / total ? total : 1)) -
                   ((sum / (total ? total : 1)) * (sum / (total ? total : 1)));

    // motion
    uint32_t changed = 0;
    if (vc->has_prev) {
      for (uint32_t i = 0; i < total; i++) {
        uint8_t a = ds[i], b = vc->prev[i];
        if ((uint8_t)(a > b ? a - b : b - a) > 12)
          changed++;
      }
      memcpy(vc->prev, ds, total);
      vc->dw = dw;
      vc->dh = dh;
      vc->has_prev = 1;

      // emit stats
      pl_stats st = {0};
      st.frame_id = pl.frame_id;
      st.avg_luma = (uint16_t)avg;
      st.var_luma = (uint16_t)var;
      st.dark_pct = (uint16_t)((dark * 1000u) / (total ? total : 1));
      st.sat_pct = (uint16_t)((sat * 1000u) / (total ? total : 1));

      envelope_t es = {0};
      es.topic = TOP_VISION_STATS;
      es.schema_v = SCH_STATS;
      es.src = MOD_VISUAL_CORTEX;
      es.dst = MOD_BRAINSTEM;
      es.pri = PRI_P2;
      es.ts_ms = now_ms;
      es.corr = e.corr;
      es.len = sizeof(st);
      memcpy(es.data, &st, sizeof(st));
      rb_try_push(vc->qout, &es);

      // emit motion
      if (vc->has_prev) {
        pl_motion mo = {0};
        mo.frame_id = pl.frame_id;
        mo.level = (uint16_t)((changed * 1000u) / (total ? total : 1));
        envelope_t em = {0};
        em.magic = ENVELOPE_MAGIC;
        em.tail = ENVELOPE_TAIL;
        em.topic = TOP_VISION_MOTION;
        em.schema_v = SCH_MOTION;
        em.src = MOD_VISUAL_CORTEX;
        em.dst = MOD_BRAINSTEM;
        em.pri = PRI_P2;
        em.ts_ms = now_ms;
        em.corr = e.corr;
        em.len = sizeof(mo);
        memcpy(em.data, &mo, sizeof(mo));
        rb_try_push(vc->qout, &em);
      }
    }
  }
}
