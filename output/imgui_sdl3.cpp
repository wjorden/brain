#include "my_imgui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h"

#include <cstdarg>
#include <mutex>
#include <vector>
#include <string>

static SDL_Window* g_win = nullptr;
static SDL_GLContext g_gl = nullptr;
static GLuint g_tex = 0;            // GL texture for gray8
static int g_tw = 0, g_th = 0;      // texture size
static std::mutex   g_log_mutex;
static std::vector<std::string> g_logs;
static cam_meta_t g_meta = {0,0,0,0,0,0,0};
static bool g_overlay = false;
static std::vector<ui_cmd_t> g_cmd;

static void ensure_tex(int w, int h){
    if(g_tex && (w==g_tw) && (h==g_th)) return;
    if(!g_tex) glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // single-channel store
    GLint swz[4] = { GL_RED, GL_RED, GL_RED, GL_ONE};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swz);
    g_tw = w; g_th = h;
}

int imgui_init(int view_w, int view_h, const char *title){
    // make sure we do not already have a window
    if(g_win) return 0;

    // init window
    SDL_Init(SDL_INIT_VIDEO);

    // set OpenGL version 330, core, title
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    g_win = SDL_CreateWindow(title ? title : "Quorrae Control",
                             view_w + 360, view_h + 40, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    // make sure we got a window
    if(!g_win) return -2;
    // make OpenGL context, current, and swap
    g_gl = SDL_GL_CreateContext(g_win);
    SDL_GL_MakeCurrent(g_win, g_gl);
    SDL_GL_SetSwapInterval(0);

    // ImGui, pretty straight forward
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(g_win, g_gl);
    ImGui_ImplOpenGL3_Init("#version 330");

    // get us our texture
    ensure_tex(view_w, view_h);
    return 0;
}

void imgui_shutdown(void){
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if(g_tex) { glDeleteTextures(1, &g_tex); g_tex = 0; }
    if(g_gl) { SDL_GL_DestroyContext(g_gl); g_gl = nullptr; }
    if(g_win) { SDL_DestroyWindow(g_win); g_win = nullptr; }
    SDL_Quit();
}

void imgui_frame_begin(void){
    SDL_Event ev;
    while(SDL_PollEvent(&ev)){
        ImGui_ImplSDL3_ProcessEvent(&ev);
        switch(ev.type){
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            switch(SDL_EVENT_KEY_DOWN){
                case SDLK_ESCAPE:
                    ui_cmd_t c{UI_CMD_NONE, 0,0};
                    c.kind = UI_CMD_NONE; // consumer decides quit
                    g_cmd.push_back(c);
                    imgui_shutdown();
                break;
            }
        }
    }
    // generate frames
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

static void ui_right_panel(){
    ImGui::Begin("Control", nullptr, ImGuiWindowFlags_NoCollapse);
    if(ImGui::BeginTabBar("tabs")){
        if(ImGui::BeginTabItem("Options")){
            if(ImGui::Checkbox("Overlay", &g_overlay)){
                g_cmd.push_back(ui_cmd_t{UI_CMD_TOGGLE_OVERLAY, (uint32_t)g_overlay, 00});
            }
            ImGui::SeparatorText(("Camera metadata"));
            ImGui::Text("WxH %u x %u", g_meta.w, g_meta.h);
            ImGui::Text("Stride: %u", g_meta.stride);
            ImGui::Text("FOURCC: 0x%08X", g_meta.fourcc);
            ImGui::Text("FrameID: 0x%08llX", (unsigned long long)g_meta.frame_id);
            ImGui::Text("FPS: now %.1f | avg %.1f", g_meta.fps_now, g_meta.fps_avg);
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Rules")){
            if(ImGui::Button("Test: Motion")) g_cmd.push_back(ui_cmd_t{UI_CMD_RUN_TEST_MOTION, 0, 0});
            ImGui::SameLine();
            if(ImGui::Button("Test:: Dark")) g_cmd.push_back(ui_cmd_t{UI_CMD_RUN_TEST_DARK, 0, 0});
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Logs")){
            std::scoped_lock lk(g_log_mutex);
            ImGui::BeginChild("log_scroll", ImVec2(0,0), ImGuiChildFlags_Border);
            for(auto& s : g_logs) ImGui::TextUnformatted(s.c_str());
            if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void imgui_frame_end(void){
    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)g_tw, (float)g_th), ImGuiCond_FirstUseEver);
    ImGui::Begin("Vision", nullptr, ImGuiWindowFlags_NoCollapse);
    if(g_tex){
        ImVec2 size((float)g_tw, (float)g_th);
        ImGui::Image((ImTextureID)(intptr_t)g_tex, size);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2((float)g_tw + 16, 8), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(340, (float)g_th), ImGuiCond_Always);
    ui_right_panel();

    ImGui::Render();
    int ww, wh; SDL_GetWindowSize(g_win, &ww, &wh);
    glViewport(0,0, ww, wh);
    glClearColor(0.1f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(g_win);
}

int imgui_vision_gray8(const uint8_t *gray, int w, int h, int stride){
    if(!gray || w <= 0 || h <= 0) return -1;
    ensure_tex(w,h);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, gray);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return 0;
}

void imgui_set_meta(const cam_meta_t* m){
    if(!m) return;
    g_meta = *m;
}

int imgui_log(const char *fmt, ...){
    char buf[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::scoped_lock lk(g_log_mutex);
    g_logs.emplace_back(buf);
    // keep bounded, erase ~1/2
    if(g_logs.size() > 4000) g_logs.erase(g_logs.begin(), g_logs.begin()+2000);
    return 0;
}

int imgui_poll_cmd(ui_cmd_t *out){
    if(!out) return 0;
    if(g_cmd.empty()) return 0;
    *out = g_cmd.front();
    g_cmd.erase(g_cmd.begin());
    return 1;
}
