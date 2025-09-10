#ifndef MY_IMGUI_H
#define MY_IMGUI_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t w, h;      // source width and height
    uint32_t stride;    // bytes per row
    uint32_t fourcc;    // V4L2 GREY='Y','1'','6',' ''
    float fps_now;      // instantaneous fps
    float fps_avg;      // rolling average
    uint64_t frame_id;  // pool frame
} cam_meta_t;

typedef enum {
    UI_CMD_NONE = 0,
    UI_CMD_TOGGLE_OVERLAY,
    UI_CMD_RUN_TEST_MOTION,
    UI_CMD_RUN_TEST_DARK
} ui_cmd_kind_t;

typedef struct {
    ui_cmd_kind_t kind;
    uint32_t    a,b;
} ui_cmd_t;

// lifecycle
int imgui_init(int view_w, int view_h, const char* title);
void imgui_shutdown(void);

// per tick
void imgui_frame_begin(void);
void imgui_frame_end(void);

// visuals & meta
int imgui_vision_gray8(const uint8_t *gray, int w, int h, int stride);
void imgui_set_meta(const cam_meta_t *m);

// logs & commands
int imgui_log(const char* fmt, ...);
int imgui_poll_cmd(ui_cmd_t* out);

#ifdef __cplusplus
}
#endif
#endif // MY_IMGUI_H
