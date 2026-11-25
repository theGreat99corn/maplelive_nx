/* MapleStory Live (com.nexonmobile.skyproject.jni.Natives) Bridge + Trace Log */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include "so_util.h"

#define JNI_PREFIX "Java_com_nexonmobile_skyproject_jni_Natives_"
#define LOG_PATH   "sdmc:/switch/maplelive_nx/boot.log"

static FILE *g_log = NULL;

void ml_trace(const char *fmt, ...) {
    if (!g_log) {
        g_log = fopen(LOG_PATH, "w");
    }
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    if (g_log) {
        va_start(ap, fmt);
        vfprintf(g_log, fmt, ap);
        va_end(ap);
        fflush(g_log);
    }
}

typedef int  (*fn_jni_onload_t)(void *vm, void *reserved);
typedef void (*fn_init_t)(void *env, void *obj, void *device_info);
typedef void (*fn_void_t)(void *env, void *obj);
typedef void (*fn_resize_t)(void *env, void *obj, int32_t w, int32_t h, uint8_t landscape);
typedef void (*fn_start_t)(void *env, void *obj, void *args);
typedef void (*fn_event_t)(void *env, void *obj, int32_t type, int32_t p1, int32_t p2, int32_t p3);

static fn_jni_onload_t fn_JNI_OnLoad    = NULL;
static fn_init_t       fn_Init          = NULL;
static fn_void_t       fn_Render_Init   = NULL;
static fn_resize_t     fn_Render_Resize = NULL;
static fn_start_t      fn_StartGame     = NULL;
static fn_void_t       fn_Render        = NULL;
static fn_event_t      fn_OnEvent       = NULL;
static fn_void_t       fn_Pause         = NULL;
static fn_void_t       fn_Resume        = NULL;
static fn_void_t       fn_StopRender    = NULL;

int maplelive_bind_exports(so_module *mod) {
    ml_trace("[1] Binding MapleLive JNI symbols...\n");
    fn_JNI_OnLoad    = (fn_jni_onload_t)so_try_find_addr_rx(mod, "JNI_OnLoad");
    fn_Init          = (fn_init_t)so_try_find_addr_rx(mod, JNI_PREFIX "Init");
    fn_Render_Init   = (fn_void_t)so_try_find_addr_rx(mod, JNI_PREFIX "Render_1Init");
    fn_Render_Resize = (fn_resize_t)so_try_find_addr_rx(mod, JNI_PREFIX "Render_1Resize");
    fn_StartGame     = (fn_start_t)so_try_find_addr_rx(mod, JNI_PREFIX "StartGame");
    fn_Render        = (fn_void_t)so_try_find_addr_rx(mod, JNI_PREFIX "Render");
    fn_OnEvent       = (fn_event_t)so_try_find_addr_rx(mod, JNI_PREFIX "OnEvent");
    fn_Pause         = (fn_void_t)so_try_find_addr_rx(mod, JNI_PREFIX "nativePause");
    fn_Resume        = (fn_void_t)so_try_find_addr_rx(mod, JNI_PREFIX "nativeResume");
    fn_StopRender    = (fn_void_t)so_try_find_addr_rx(mod, JNI_PREFIX "StopRender");

    ml_trace("    OnLoad=%p Init=%p R_Init=%p R_Resize=%p StartGame=%p Render=%p OnEvent=%p\n",
             (void*)fn_JNI_OnLoad, (void*)fn_Init, (void*)fn_Render_Init,
             (void*)fn_Render_Resize, (void*)fn_StartGame, (void*)fn_Render, (void*)fn_OnEvent);

    return (fn_Init && fn_StartGame && fn_Render) ? 0 : -1;
}

void maplelive_lifecycle_start(void *vm, void *env, void *dev_info_arr, void *start_args_arr) {
    if (fn_JNI_OnLoad) {
        ml_trace("[2] Calling JNI_OnLoad...\n");
        fn_JNI_OnLoad(vm, NULL);
    }
    if (fn_Init) {
        ml_trace("[3] Calling Natives.Init...\n");
        fn_Init(env, NULL, dev_info_arr);
    }
    if (fn_Render_Init) {
        ml_trace("[4] Calling Natives.Render_Init...\n");
        fn_Render_Init(env, NULL);
    }
    if (fn_Render_Resize) {
        ml_trace("[5] Calling Natives.Render_Resize(1280, 720, 1)...\n");
        fn_Render_Resize(env, NULL, 1280, 720, 1);
    }
    if (fn_StartGame) {
        ml_trace("[6] Calling Natives.StartGame...\n");
        fn_StartGame(env, NULL, start_args_arr);
    }
    ml_trace("[7] Entering main render loop!\n");
}

void maplelive_lifecycle_tick(void *env) {
    static int frame = 0;
    if (frame < 5) ml_trace("[Frame %d] Calling Natives.Render...\n", frame);
    if (fn_Render) fn_Render(env, NULL);
    if (frame < 5) { ml_trace("[Frame %d] Render OK\n", frame); frame++; }
}

void maplelive_input_touch(void *env, int x, int y, int finger_id) {
    if (fn_OnEvent) fn_OnEvent(env, NULL, 2, x, y, finger_id);
}

void maplelive_input_key(void *env, int android_keycode) {
    if (fn_OnEvent) fn_OnEvent(env, NULL, 0, android_keycode, 0, 0);
}

void maplelive_lifecycle_pause(void *env) {
    if (fn_Pause) fn_Pause(env, NULL);
}

void maplelive_lifecycle_resume(void *env) {
    if (fn_Resume) fn_Resume(env, NULL);
}

void maplelive_lifecycle_stop(void *env) {
    if (fn_StopRender) fn_StopRender(env, NULL);
    if (g_log) { fclose(g_log); g_log = NULL; }
}
