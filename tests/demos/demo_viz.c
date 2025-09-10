#include "brain/brainstem/brainstem.h"
#include "brain/cortex/vision/visual_cortex.h"
#include "brain/thalamus/vision/frame_pool.h"
#include "brain/thalamus/vision/thalamus_vision.h"
#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"
#include "outs/out_x11.h"
#include "periphery/vision/camera.h"
#include <linux/videodev2.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile sig_atomic_t g_run = 1;
static void on_int(int exit) { g_run = 0; }

static uint64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ((uint64_t)ts.tv_sec * 1000ull) + (ts.tv_nsec / 1000000ull);
}

// forwards decls for v4l2
int camera_open(camera_t *cam, const char *device, int width, int height,
                int pixfmt);
void camera_close(camera_t *cam);
int camera_grab(camera_t *cam);
int camera_start(camera_t *cam);

int main(void) {
  signal(SIGINT, on_int);

  // rings
  printf("[WELCOME] to the Vision Demo!\n");
  printf("[RING] First, we need to setup the ring for the camera to "
         "communicate with the thalamus\n");
  rb_t *q_cam2thal = rb_create(128, sizeof(envelope_t));
  if (!q_cam2thal) {
    perror("ring buffer fail");
    rb_destroy(q_cam2thal);
    return -1;
  }
  rb_t *q_thal2ctx = rb_create(128, sizeof(envelope_t));
  if (!q_thal2ctx) {
    perror("ring buffer fail");
    rb_destroy(q_thal2ctx);
    return -1;
  }
  rb_t *q_ctx2bs = rb_create(128, sizeof(envelope_t));
  if (!q_ctx2bs) {
    perror("ring buffer fail");
    rb_destroy(q_ctx2bs);
    return -1;
  }
  rb_t *q_bs2out = rb_create(128, sizeof(envelope_t));
  if (!q_bs2out) {
    perror("ring buffer fail");
    rb_destroy(q_bs2out);
    return -1;
  }
  printf("[POOL] for image processing\n");
  // frame pool
  frame_pool_t pool;
  printf("[NOTE] the pool will be filled by the camera_grab "
         "function\n");
  printf("[POOL] initializing frame_pool_t\n");
  fp_init(&pool, 64, 1280 * 720 * 2); // big enough for 720p
  if (!pool.arena) {
    perror("pool initialization");
    fp_free(&pool);
    return -1;
  }
  printf("[POOL] frame pool: nslots=%d max_bytes=%d\n", pool.nslots,
         pool.max_bytes);

  printf("[CAMERA] setup\n");
  camera_t cam;
  printf("[CAMERA] initializing camera_t\n");
  camera_init(&cam, &pool, q_cam2thal, 640, 480);
  printf("[CAMERA] buf=%p\n bytes=%08X\n", (void *)cam.q_to_thalamus->buf,
         cam.q_to_thalamus->buf[8]);

  printf("[THALAMUS] setup\n");
  thalamus_vision_t tv;
  printf("[THALAUS] initializing thalamus_vision_t\n");
  thalamus_init(&tv, q_cam2thal, q_thal2ctx, 1);
  printf("[THALAMUS] buf=%p\n bytes=%08X\n", (void *)tv.qin->buf,
         tv.qin->buf[8]);

  printf("[VISUAL CORTEX] setup\n");
  visual_cortex_t vc;
  printf("[VISUAL CORTEX] initializing visual_cortex_type\n");
  visual_cortex_init(&vc, &pool, q_thal2ctx, q_ctx2bs);
  printf("[VISUAL CORTEX] buf=%p\n bytes=%08X\n", (void *)vc.qin->buf,
         vc.qin->buf[8]);

  printf("[BRAINSTEM] setup\n");
  brainstem_t bs;
  printf("[BRAINSTEM] initializing brainstem_t\n");
  brainstem_init(&bs, q_ctx2bs, q_bs2out, 256);
  printf("[BRAINSTEM] set window_ms (live time) to 10000\n");
  bs.window_ms = 10000u;
  printf("[BRAINSTEM] buf=%p\n bytes=%08X\n", (void *)bs.in->buf,
         bs.in->buf[8]);

  printf("[CAMERA] opening V4L2 device on cam\n");
  if (camera_open(&cam, "/dev/video0", 640, 480, V4L2_PIX_FMT_YUYV) < 0)
    return 1;
  printf("[CAMERA] is open\n");
  printf("[CAMERA] start\n");
  if (camera_start(&cam) < 0)
    return 1;

  uint64_t t0 = now_ms(), t_last = t0, t_hb = t0;
  unsigned emits = 0, grabs = 0, eagain = 0;

  struct pollfd pfd = {
      .fd = cam.fd,
      .events = POLLIN,
  };

  printf("[LOOP] Ctrl+C to exit\n");
  while (g_run) {
    int pr = poll(&pfd, 1, 500);
    if (pr < 0) {
      perror("poll");
      break;
    }

    for (int i = 0; i < 4; ++i) {
      int rc = camera_grab(&cam);
      if (rc > 0) {
        grabs++;
        break;
      } else if (rc == 0) {
        eagain++;
        break;
      } else {
        perror("camera_grab");
        on_int(1);
      }
      break;
    }

    printf("[LOOP] advance pipline\n");
    camera_emit_frame(&cam, now_ms(), cam.w, cam.pixfmt);
    fprintf(stderr, "[CAMERA] pop: num slots=%d\n", cam.pool->nslots);
    thalamus_vision_tick(&tv);
    fprintf(stderr, "[THALAMUS] pop: count=%d\n", tv.count);
    visual_cortex_tick(&vc, now_ms());
    fprintf(stderr, "[VISUAL CORTEX] pop: num slots=%d\n", vc.pool->nslots);
    emits += brainstem_tick(&bs, now_ms());
    fprintf(stderr, "[BRAINSTEM] pop: src=%u dst=%u topic=%u schema=%u\n",
            bs.journal->src, bs.journal->dst, bs.journal->topic,
            bs.journal->schema_v);
    fprintf(stderr, "[BRAINSTEM] pop: journal len=%d emitted=%u\n",
            bs.journal->len, emits);

    // heartbeat
    uint64_t t = now_ms();
    if (t - t_hb >= 1000) {
      size_t q0 = rb_size_approx(q_cam2thal);
      size_t q1 = rb_size_approx(q_thal2ctx);
      size_t q2 = rb_size_approx(q_ctx2bs);
      size_t q3 = rb_size_approx(q_bs2out);
      fprintf(stderr,
              "[HB] +%llums grabs=%u emitted=%u eagain=%u q{cam->th=%zu "
              "th->cx=%zu cx->bs=%zu bs->out=%zu}\n",
              (unsigned long long)(t - t0), grabs, emits, eagain, q0, q1, q2,
              q3);
      emits = grabs = eagain = 0;
      t_hb = t;
      (void)t_last;
    }
  }
  out_x11_shutdown();
  camera_close(&cam);
  fp_free(&pool);
  if (q_bs2out)
    rb_destroy(q_bs2out);
  brainstem_free(&bs);
  if (q_ctx2bs)
    rb_destroy(q_ctx2bs);
  if (q_thal2ctx)
    rb_destroy(q_thal2ctx);
  return 0;
}
