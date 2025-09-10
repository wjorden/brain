// generate GRAY8 frames

#include "thalamus/vision/frame_pool.h"
#include "interfaces/bus/envelope.h"
#include "interfaces/bus/rb.h"
#include "interfaces/io/vision_payloads.h"
#include "periphery/vision/camera.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void camera_emit_frame(camera_t *cam, uint64_t now_ms, uint16_t stride,
                       uint8_t fmt) {
    uint16_t slot;
    uint8_t *ptr;
    if (fp_aquire(cam->pool, &slot, &ptr) != 0)
        return;

    fp_publish(cam->pool, slot, (uint32_t)cam->w * cam->h);

    pl_frame pl = {
        .frame_id = frame_make(slot, cam->pool->slot[slot].frame_id),
        .w = cam->w,
        .h = cam->h,
        .stride = stride,
        .fmt = fmt,
        .reserved = 0,
        .ts_ms = now_ms,
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
    e.ts_ms = now_ms;
    e.len = sizeof(pl);
    memcpy(e.data, &pl, sizeof(pl));
    rb_try_push(cam->q_to_thalamus, &e);
}
