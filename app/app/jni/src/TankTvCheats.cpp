/*
 * 作弊开关的存储。
 *
 * 存在工作目录(解包出来的 assets 目录)下的 cheats.txt,格式是自解释的:
 *
 *     infinite_lives=1
 *     strong_eagle=0
 *     invincible=1
 *
 * 想手动改也行 —— 改完下次启动生效。
 */
#include "TankTvCheats.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "TankTv"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

int tanktv_cheat_infinite_lives = 0;
int tanktv_cheat_strong_eagle = 0;
int tanktv_cheat_invincible = 0;

} // extern "C"

namespace {

const char *kCheatFile = "cheats.txt";
const int kLineSize = 128;

/* 解析 "key=0/1" 这种行,不是这个 key 就什么都不做 */
void parseLine(char *line, const char *key, int *out)
{
    const size_t key_len = std::strlen(key);
    if (std::strncmp(line, key, key_len) != 0 || line[key_len] != '=')
        return;

    const int value = std::atoi(line + key_len + 1);
    *out = (value != 0) ? 1 : 0;
}

} // namespace

extern "C" void tanktvCheatsLoad(void)
{
    FILE *f = std::fopen(kCheatFile, "r");
    if (f == nullptr)
        return;  /* 第一次运行:全关 */

    char line[kLineSize];
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        parseLine(line, "infinite_lives", &tanktv_cheat_infinite_lives);
        parseLine(line, "strong_eagle", &tanktv_cheat_strong_eagle);
        parseLine(line, "invincible", &tanktv_cheat_invincible);
    }
    std::fclose(f);

    LOGI("cheats: infinite_lives=%d strong_eagle=%d invincible=%d",
         tanktv_cheat_infinite_lives, tanktv_cheat_strong_eagle, tanktv_cheat_invincible);
}

extern "C" void tanktvCheatsSave(void)
{
    FILE *f = std::fopen(kCheatFile, "w");
    if (f == nullptr) {
        LOGE("cheats: cannot write %s", kCheatFile);
        return;
    }

    std::fprintf(f, "infinite_lives=%d\n", tanktv_cheat_infinite_lives);
    std::fprintf(f, "strong_eagle=%d\n", tanktv_cheat_strong_eagle);
    std::fprintf(f, "invincible=%d\n", tanktv_cheat_invincible);
    std::fclose(f);

    LOGI("cheats saved: infinite_lives=%d strong_eagle=%d invincible=%d",
         tanktv_cheat_infinite_lives, tanktv_cheat_strong_eagle, tanktv_cheat_invincible);
}

extern "C" int tanktvCheatsAnyOn(void)
{
    return (tanktv_cheat_infinite_lives || tanktv_cheat_strong_eagle || tanktv_cheat_invincible) ? 1 : 0;
}
