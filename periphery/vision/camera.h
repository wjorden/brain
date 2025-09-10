#pragma once
#include "frame_pool.h"
#include "interfaces/bus/rb.h"
#include <stdint.h>

typedef struct {
  void *start;
  size_t length;
} buffer_t;

typedef struct {
  int fd; // file descriptor for streaming
  frame_pool_t *pool;
  rb_t *q_to_thalamus; // dst ring
  uint16_t w, h;
  uint16_t stride_bytes;
  uint32_t pixfmt;
  buffer_t *buffer;   // mmap'd frame buffer
  unsigned int nbufs; // how many buffers for cleanup
} camera_t;

static inline void camera_init(camera_t *cam, frame_pool_t *pool, rb_t *q_to_thalamus, uint16_t w, uint16_t h) {
  cam->fd = -1;
  cam->pool = pool;
  cam->q_to_thalamus = q_to_thalamus;
  cam->w = w;
  cam->h = h;
  cam->stride_bytes = 0;
  cam->pixfmt = 0;
  cam->buffer = NULL;
  cam->nbufs = 0;
}

int camera_open(camera_t *cam, const char *device, int width, int height, int pixfmt);
int camera_start(camera_t *cam);

int camera_grab(camera_t *cam);
void camera_emit_frame(camera_t *cam, uint64_t now_ms, uint16_t stride, uint8_t fmt);

int camera_stop(camera_t *cam);
void camera_close(camera_t *cam);
