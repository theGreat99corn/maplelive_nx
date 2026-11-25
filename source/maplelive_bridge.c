#include <stdio.h>
#include <stdint.h>
#include "so_util.h"

extern so_module game_mod;

typedef void (*ml_init_t)(void *env, void *obj, void *device_info);
typedef void (*ml_void_t)(void *env, void *obj);
typedef void (*ml_resize_t)(void *env, void *obj, int32_t w, int32_t h, uint8_t landscape);
typedef void (*ml_start_t)(void *env, void *obj, void *args);
typedef void (*ml_event_t)(void *env, void *obj, int32_t a, int32_t b, int32_t c, int32_t d);

static ml_init_t   ml_Init          = NULL;
static ml_void_t   ml_Render_Init   = NULL;
static ml_resize_t ml_Render_Resize = NULL;
static ml_start_t  ml_StartGame     = NULL;
static ml_void_t   ml_Render        = NULL;
static ml_event_t  ml_OnEvent       = NULL;
static ml_void_t   ml_Pause         = NULL;
static ml_void_t   ml_Resume        = NULL;
static ml_void_t   ml_StopRender    = NULL;

static void log_boot(const char *msg) {
    FILE *f = fopen("sdmc:/switch/maplelive_nx/boot.log", "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}

void maplelive_bind_symbols(void) {
    ml_Init          = (ml_init_t)so_try_find_addr_rx(&game_mod, "Java_com_nexonmobile_skyproject_jni_Natives_Init");
    ml_Render_Init   = (ml_void_t)so_try_find_addr_rx(&game_mod, "Java_com_nexonmobile_skyproject_jni_Natives_Render_1Init");
    ml_Render_Resize = (ml_resize_t)so_try_find_addr_rx(&game_mod, "Java_com_nexonmobile_skyproject_jni_Natives_Render_1Resize");
    ml_StartGame     = (ml_start_t)so_try_find_addr_rx(&game_mod, "Java_com_nexonmobile_skyproject_jni_Natives_StartGame");
    ml_Render        = (ml_void_t)so_try_find_addr_rx(&game_mod, "Java_com_nexonmobile_skyproject_jni_Natives_Render");
    ml_OnEvent       = (ml_event_t)so_try_find_addr_rx(&game_mod, "Java_com_nexonmobile_skyproject_jni_Natives_OnEvent");
    ml_Pause         = (ml_void_t)so_try_find_addr_rx(&game_mod, "Java_com_nexonmobile_skyproject_jni_Natives_nativePause");
    ml_Resume        = (ml_void_t)so_try_find_addr_rx(&game_mod, "Java_com_nexonmobile_skyproject_jni_Natives_nativeResume");
    ml_StopRender    = (ml_void_t)so_try_find_addr_rx(&game_mod, "Java_com_nexonmobile_skyproject_jni_Natives_StopRender");

    char buf[256];
    snprintf(buf, sizeof(buf), "[SYM] Init=%p Render_Init=%p Resize=%p Start=%p Render=%p OnEvent=%p",
             (void*)ml_Init, (void*)ml_Render_Init, (void*)ml_Render_Resize,
             (void*)ml_StartGame, (void*)ml_Render, (void*)ml_OnEvent);
    log_boot(buf);
}

void maplelive_start_lifecycle(void *env, void *thiz, void *dev_info, void *start_args) {
    if (ml_Init)          { log_boot("[CALL] Natives.Init"); ml_Init(env, thiz, dev_info); }
    if (ml_Render_Init)   { log_boot("[CALL] Natives.Render_Init"); ml_Render_Init(env, thiz); }
    if (ml_Render_Resize) { log_boot("[CALL] Natives.Render_Resize"); ml_Render_Resize(env, thiz, 1280, 720, 1); }
    if (ml_StartGame)     { log_boot("[CALL] Natives.StartGame"); ml_StartGame(env, thiz, start_args); }
}

void maplelive_step_frame(void *env, void *thiz) {
    if (ml_Render) ml_Render(env, thiz);
}

void maplelive_key_event(void *env, void *thiz, int keycode) {
    if (ml_OnEvent) ml_OnEvent(env, thiz, 0, keycode, 0, 0);
}

void maplelive_touch_event(void *env, void *thiz, int x, int y, int id) {
    if (ml_OnEvent) ml_OnEvent(env, thiz, 2, x, y, id);
}
