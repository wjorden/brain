#include "brainstem/brainstem.h"
#include "cortex/vision/visual_cortex.h"
#include "interfaces/bus/rb.h"
#include "interfaces/bus/envelope.h"
#include "interfaces/io/vision_payloads.h"
#include "output/my_imgui.h"
#include "periphery/vision/camera.h"
#include "thalamus/vision/thalamus_vision.h"
#include "thalamus/vision/frame_pool.h"


#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void) {
    // window w & h
    float win_w = 640;
    float win_h = 480;

    // ring buffers
    rb_t *q_cam2thal = rb_create(8, sizeof(envelope_t));
    rb_t *q_thal2ctx = rb_create(8, sizeof(envelope_t));
    rb_t *q_ctx2bs = rb_create(8, sizeof(envelope_t));
    rb_t *q_bs2out = rb_create(8, sizeof(envelope_t));

    // setup frame pool
    frame_pool_t pool;
    fp_init(&pool, 64, (size_t)win_w * win_h * 2);
    printf("[FP] base=%p max=%zu slots=%d\n", (void*)pool.slot->base, (size_t)pool.max_bytes, pool.nslots);
    assert(pool.nslots > 0 && pool.nslots <= (int)(1u << SLOT_BITS));

    // camera
    camera_t cam;
    camera_init(&cam, &pool, q_cam2thal, win_w, win_h);

    // choose device via env
    const char *dev = getenv("CAM_DEV");
    // if no env, default
    if(!dev) dev = "/dev/video0";
    // open dev via kernel
    camera_open(&cam, dev, cam.w, cam.h, cam.pixfmt);

    // thalamus
    thalamus_vision_t tv;
    thalamus_init(&tv, q_cam2thal, q_thal2ctx, 3);

    // cortex
    visual_cortex_t vc;
    visual_cortex_init(&vc, &pool, q_thal2ctx, q_ctx2bs);

    // brainstem
    brainstem_t bs;
    brainstem_init(&bs, q_ctx2bs, q_bs2out, sizeof(bs.j_cap));
    bs.window_ms = 10000;


    // imgui
    if(imgui_init(win_w, win_h, "Quorrae Control") != 0){
        printf("imgui_init failed: %s", SDL_GetError());
        return 1;
    }

    // fps
    uint64_t perf = SDL_GetPerformanceFrequency();
    uint64_t t0 = SDL_GetPerformanceCounter();
    float fps_now = 0.0f;
    float fps_avg = 0.0f;
    const float alpha = 0.10f;
    camera_start(&cam);

    for(;;){
        imgui_frame_begin();

        uint64_t now_ms = SDL_GetTicks();
        // advance timeline
        printf("[tick]camera\n");
        camera_grab(&cam);
        printf("[tick]thalamus\n");
        thalamus_vision_tick(&tv);
        printf("[tick]visual cortex\n");
        visual_cortex_tick(&vc, now_ms);
        printf("[tick]brainstem\n");
        brainstem_tick(&bs, now_ms);

        envelope_t ev;
        while (rb_try_pop(bs.out, &ev)) {
            if(ev.topic == TOP_FRAME_AVAILABLE && ev.schema_v == SCH_FRAME){
                const pl_frame* pl = (const pl_frame*)ev.data;
                printf("[FRAME] id=%u w=%u h=%u stride=%u fmt=0x%08x len=%u\n",
                          pl->frame_id, pl->w, pl->h, pl->stride, cam.pixfmt, (unsigned)ev.len);
                uint8_t* ptr = NULL; uint32_t bytes = 0;
                if(fp_map(&pool, pl->frame_id, &ptr, &bytes) == 0){
                    printf("[FP_MAP] results: ptr=%zu bytes=%u", (unsigned long) ptr, bytes);
                    imgui_vision_gray8(ptr, pl->w, pl->h, pl->stride);

                    uint64_t t1 = SDL_GetPerformanceCounter();
                    double dt = (double)(t1 - t0) / (double)perf;
                    t0 = t1;
                    float fi = (dt > 0.0) ? (float)(1.0/dt) : 0.0f;
                    fps_now = fi; fps_avg = (fps_avg > 0.f) ? fps_avg*(1.f-alpha) + fi*alpha : fi;

                    cam_meta_t m = {
                        .w = pl->w, .h = pl->h, .stride = pl->stride,
                        .fourcc = cam.pixfmt,
                        .fps_now = fps_now, .fps_avg = fps_avg,
                        .frame_id = pl->frame_id
                    };
                    imgui_set_meta(&m);
                }
            }
        }

        imgui_frame_end();
        SDL_Delay(1);
    }

    camera_close(&cam);
    fp_free(&pool);
    rb_destroy(q_cam2thal);
    imgui_shutdown();
    return 0;
}
