#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "camera.h"
#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"
#include "interfaces/io/vision_payloads.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

static int xioctl(int fd, int request, void *arg) {
  int r;
  do {
    r = ioctl(fd, request, arg);
  } while (r == -1 && errno == EINTR);
  return r;
}

int camera_open(camera_t *cam, const char *device, int width, int height,
                int pixfmt) {
  struct v4l2_capability cap;
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  struct v4l2_buffer buf;

  cam->fd = open(device, O_RDWR | O_NONBLOCK);
  if (cam->fd == -1) {
    perror("Opening Device");
    camera_close(cam);
    return -1;
  }

  if (xioctl(cam->fd, (unsigned)VIDIOC_QUERYCAP, &cap) == -1) {
    perror("Querying Capabiities");
    camera_close(cam);
    return -1;
  }

  CLEAR(fmt);
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = width;
  fmt.fmt.pix.height = height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // YUYV or RGB24
  fmt.fmt.pix.field = V4L2_FIELD_NONE;

  if (xioctl(cam->fd, (unsigned)VIDIOC_S_FMT, &fmt) == -1) {
    perror("Setting Pixel Format");
    camera_close(cam);
    return -1;
  }
  cam->w = fmt.fmt.pix.width;
  cam->h = fmt.fmt.pix.height;
  cam->stride_bytes = fmt.fmt.pix.bytesperline;
  cam->pixfmt = fmt.fmt.pix.pixelformat;
  printf("[OPEN] fmt=0x%08x w=%u h=%u bpl=%u\n", (unsigned)cam->pixfmt,
          (unsigned)cam->w, (unsigned)cam->h, (unsigned)cam->stride_bytes);

  CLEAR(req);
  req.count = 4; // number of buffers
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (xioctl(cam->fd, (unsigned)VIDIOC_REQBUFS, &req) == -1) {
    perror("Requesting Buffer");
    camera_close(cam);
    return -1;
  }
  if (req.count < 2) {
    printf("REQBUFS returned %u\n", req.count);
    camera_close(cam);
    return -1;
  }

  cam->buffer = calloc(req.count, sizeof(buffer_t));
  if (!cam->buffer) {
      perror("calloc");
      camera_close(cam);
      return -1;
  }

  for (unsigned int i = 0; i < req.count; ++i) {
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (xioctl(cam->fd, (unsigned)VIDIOC_QUERYBUF, &buf) == -1) {
      perror("Querying Buffer");
      camera_close(cam);
      return -1;
    }

    void *start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                       cam->fd, buf.m.offset);
    if (start == MAP_FAILED) {
      perror("mmap");
      camera_close(cam);
      return -1;
    }

    cam->buffer[i].start = start;
    cam->buffer[i].length = buf.length;
    cam->nbufs++;
  }
  return 0;
}

int camera_start(camera_t *cam) {
  for (unsigned i = 0; i < cam->nbufs; i++) {
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (xioctl(cam->fd, (unsigned) VIDIOC_QBUF, &buf) == -1) {
        perror("Queue Buffer");
        camera_close(cam);
        return -1;
    }
  }

  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (xioctl(cam->fd, (unsigned) VIDIOC_STREAMON, &type) == -1) {
      perror("Start Capture");
      camera_close(cam);
      return -1;
  }
  return 0;
}

int camera_grab(camera_t *cam) {
    // create copy buffer
  struct v4l2_buffer buf;
  CLEAR(buf);

  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (xioctl(cam->fd, (unsigned)VIDIOC_DQBUF, &buf) == -1) {
    if (errno == EAGAIN)
      return 0; // no frame

    perror("Dequeuing Frame");
    camera_close(cam);
    return -1;
  }

  // convert time
  uint64_t ts_ms = (uint64_t) buf.timestamp.tv_sec * 1000ull + (uint64_t) buf.timestamp.tv_usec / 1000ull;

  // copy from mmapped buffer
  uint16_t slot;
  uint8_t *dst = NULL;
  if (fp_aquire(cam->pool, &slot, &dst) == 0) {
    // bytes filled by driver
    size_t bytes = buf.bytesused ? buf.bytesused : cam->buffer[buf.index].length;
    // clamp pool
    if (bytes > (size_t)cam->pool->max_bytes) {
      bytes = cam->pool->max_bytes;
    }
    if (!cam->pool || !cam->pool->arena || !cam->pool->slot || cam->pool->nslots == 0
        || cam->pool->max_bytes == 0) {
        perror("fp_aquire");
        return -1;
    }

    // build frame payload
    pl_frame pl = {
        .frame_id = frame_make(slot, cam->pool->slot[slot].frame_id),
        .w = cam->w,
        .h = cam->h,
        .stride = cam->w * 2,
        .fmt = (uint8_t)V4L2_PIX_FMT_YUYV,
        .reserved = 0,
        .ts_ms = ts_ms,
    };
    envelope_t e = {0};
    e.magic = ENVELOPE_MAGIC;
    e.tail = ENVELOPE_TAIL;
    e.schema_v = SCH_FRAME;
    e.topic = TOP_FRAME_AVAILABLE;
    e.pri = PRI_P2;
    e.src = MOD_PERIPHERY_CAMERA;
    e.dst = MOD_THALAMUS_VISION;
    e.corr = (uint64_t)pl.frame_id;
    e.ts_ms = ts_ms;
    e.len = sizeof(pl);
    memcpy(e.data, &pl, sizeof(pl));
    // debug
    printf("[PUSH] elem_size=%u want=%zu topic=%u len=%u want=%zu magic=0x%08X frame_id=0x%08X\n",
           (unsigned)cam->q_to_thalamus->elem_size, sizeof(envelope_t), e.topic, e.len, sizeof(pl_frame), e.magic, pl.frame_id);
    rb_try_push(cam->q_to_thalamus, &e);
    printf("[POP] elem_size=%u want=%zu topic=%u len=%u want=%zu magic=0x%08X frame_id=0x%08X\n",
           (unsigned)cam->q_to_thalamus->elem_size, sizeof(envelope_t), e.topic, e.len, sizeof(pl_frame), e.magic, pl.frame_id);
  } // else drop if full

  if (xioctl(cam->fd, (unsigned)VIDIOC_QBUF, &buf) == -1) {
    perror("Requeue Buffer");
    camera_close(cam);
    return -1;
  }

  return 1; // frame delivered
}

int camera_stop(camera_t *cam) {
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(cam->fd, (unsigned)VIDIOC_STREAMOFF, &type) == -1) {
    perror("Stream Off");
    fp_free(cam->pool);
    return -1;
  }
  return 0;
}

void camera_close(camera_t *cam) {
  camera_stop(cam);
  for (unsigned i = 0; i < cam->nbufs; ++i) {
    if (cam->buffer && cam->buffer[i].start) {
      munmap(cam->buffer[i].start, cam->buffer[i].length);
    }
  }
  free(cam->buffer);
  cam->buffer = NULL;
  cam->nbufs = 0;
  if (cam->fd != -1) {
    close(cam->fd);
    cam->fd = -1;
  }
  rb_destroy(cam->q_to_thalamus);
}
