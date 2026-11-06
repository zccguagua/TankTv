/*
 * 阶段 H:最高分存档。
 *
 * 游戏上游的最高分只存在内存里(game/src/high_score.h 的 inline 变量),
 * 进程一退就没了。这里不改上游一行代码,由 native 侧代劳:
 *
 *   启动时(chdir 之后、main() 之前):从工作目录读 highscore.txt 恢复
 *   运行中:一个轻量线程每 2 秒看一眼有没有变,变了就写盘
 *
 * 工作目录就是 GameActivity 解包出来的 assets 目录,所以存档和游戏
 * 自己的资源在同一个地方,不需要额外的路径传递。
 */
#include <cstdio>
#include <pthread.h>
#include <time.h>
#include <android/log.h>

#include "high_score.h"

#define LOG_TAG "TankTv"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

const char *kScoreFile = "highscore.txt";
const unsigned kMaxReasonableScore = 99999999u;

/* 上一次落盘的值,用来判断要不要写 */
unsigned g_last_saved = 0;

unsigned readScoreFile()
{
    FILE *f = std::fopen(kScoreFile, "r");
    if (f == nullptr)
        return 0;

    unsigned value = 0;
    int matched = std::fscanf(f, "%u", &value);
    std::fclose(f);
    return matched == 1 ? value : 0;
}

void writeScoreFile(unsigned value)
{
    FILE *f = std::fopen(kScoreFile, "w");
    if (f == nullptr) {
        LOGE("highscore: cannot write %s", kScoreFile);
        return;
    }
    std::fprintf(f, "%u\n", value);
    std::fclose(f);
}

void *scoreSaverLoop(void *)
{
    const struct timespec interval = {2, 0};

    for (;;) {
        nanosleep(&interval, nullptr);

        unsigned current = HighScore::score;
        if (current != g_last_saved) {
            writeScoreFile(current);
            g_last_saved = current;
            LOGI("highscore saved: %u", current);
        }
    }
    return nullptr;
}

} // namespace

/* 由 android_glue.c 在 chdir 成功之后调用 */
extern "C" void tanktvHighScoreStart(void)
{
    const unsigned saved = readScoreFile();
    if (saved > 0 && saved <= kMaxReasonableScore && saved > HighScore::score) {
        HighScore::score = saved;
        LOGI("highscore loaded: %u", saved);
    }
    g_last_saved = HighScore::score;

    pthread_t thread;
    if (pthread_create(&thread, nullptr, scoreSaverLoop, nullptr) == 0) {
        pthread_detach(thread);
    } else {
        LOGE("highscore: pthread_create failed, score will not persist");
    }
}
