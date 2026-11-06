/*
 * 阶段 D:给 Java 侧用的最小 JNI 胶水。
 *
 * 三件事:
 * 1) chdir —— 游戏源码里资源路径全是相对路径(appconfig.h),上游代码不改,
 *    只能把进程工作目录切到解包后的目录。android.system.Os 里没有公开的
 *    chdir,所以必须自己写一个。
 * 2) 补发一次 SDL_WINDOWEVENT_RESIZED —— 见下方 send_initial_resize 注释。
 * 3) 顺带把阶段 H 的最高分存档拉起来(high_score_io.cpp)。
 */
#include <jni.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <android/log.h>
#include <SDL2/SDL.h>

#define LOG_TAG "TankTv"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* 游戏只创建一个窗口,SDL 的窗口 id 从 1 开始 */
#define GAME_WINDOW_ID 1
#define WAIT_RETRIES   3000   /* 3000 * 10ms = 30s 超时 */
#define WAIT_STEP_NS   (10 * 1000 * 1000)

/* high_score_io.cpp 里的实现(需要 CWD 已经切到资源目录) */
extern void tanktvHighScoreStart(void);

static void wait_a_bit(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = WAIT_STEP_NS;
    nanosleep(&ts, NULL);
}

/*
 * 为什么要手工补一个 RESIZED 事件:
 *
 * 游戏靠 SDLRenderer::setViewportForWindowSize() 做等比缩放 + 居中,
 * 而它只在收到 WINDOW 的 RESIZED / MAXIMIZED / RESTORED / SHOWN 事件时才被调用。
 * Android 上 RESIZED 来自 Java 的 surfaceChanged -> onNativeResize,
 * 但那一刻游戏侧窗口(Android_Window)往往还没创建,事件就被丢掉了(SDL 里
 * 有 if (Android_Window) 的判断),之后也不会再补。
 * 结果就是 SDL_RenderSetScale 一直是 1,游戏画面按 464x416 原始像素画在左上角。
 *
 * 这里在游戏 main() 跑起来之后,把真实 surface 尺寸作为 RESIZED 事件塞进队列,
 * 游戏随后 poll 到它就会自己算好缩放和居中。上游代码一行都不用改。
 */
static void *send_initial_resize(void *unused)
{
    (void) unused;

    /* 等游戏 main() 里 SDL_Init(SDL_INIT_VIDEO) 完成 */
    for (int i = 0; i < WAIT_RETRIES && !SDL_WasInit(SDL_INIT_VIDEO); i++)
        wait_a_bit();

    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        LOGE("resize helper: SDL video not initialized, giving up");
        return NULL;
    }

    /* 等 SDL_CreateWindow 完成(SDL 会按真实 surface 尺寸设置 window->w/h) */
    SDL_Window *window = NULL;
    for (int i = 0; i < WAIT_RETRIES && window == NULL; i++) {
        window = SDL_GetWindowFromID(GAME_WINDOW_ID);
        if (window == NULL)
            wait_a_bit();
    }
    if (window == NULL) {
        LOGE("resize helper: window %d not found, giving up", GAME_WINDOW_ID);
        return NULL;
    }

    /* 用真实屏幕分辨率修正窗口尺寸,而不是读 window 的尺寸。
     * 冷启动时第一个 surfaceChanged 可能把窗口报成游戏请求的 464x416,
     * 导致画面只在左上角;display mode 来自 getRealMetrics,恒为全屏尺寸。 */
    int old_w = 0, old_h = 0;
    SDL_GetWindowSize(window, &old_w, &old_h);

    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.w > 0 && mode.h > 0) {
        SDL_SetWindowSize(window, mode.w, mode.h);
        int new_w = 0, new_h = 0;
        SDL_GetWindowSize(window, &new_w, &new_h);
        LOGI("resize helper: window %dx%d -> display %dx%d -> window %dx%d",
             old_w, old_h, mode.w, mode.h, new_w, new_h);
    } else {
        LOGE("resize helper: cannot get display mode (window %dx%d)", old_w, old_h);
    }

    return NULL;
}

static void start_resize_helper(void)
{
    pthread_t thread;
    if (pthread_create(&thread, NULL, send_initial_resize, NULL) == 0)
        pthread_detach(thread);
    else
        LOGE("resize helper: pthread_create failed");
}

JNIEXPORT jboolean JNICALL
Java_com_zcc_tanktv_GameActivity_nativeChdir(JNIEnv *env, jclass clazz, jstring path)
{
    (void) clazz;
    if (path == NULL) {
        LOGE("nativeChdir: null path");
        return JNI_FALSE;
    }

    const char *c_path = (*env)->GetStringUTFChars(env, path, NULL);
    if (c_path == NULL) {
        LOGE("nativeChdir: GetStringUTFChars failed");
        return JNI_FALSE;
    }

    int rc = chdir(c_path);
    if (rc == 0) {
        LOGI("chdir -> %s", c_path);
    } else {
        LOGE("chdir failed: %s (errno=%d)", c_path, errno);
    }

    (*env)->ReleaseStringUTFChars(env, path, c_path);

    if (rc == 0) {
        /* 必须在 chdir 之后:存档文件按相对路径读写 */
        tanktvHighScoreStart();
        start_resize_helper();
    }

    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}
