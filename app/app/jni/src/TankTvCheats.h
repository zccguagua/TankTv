#ifndef TANKTV_CHEATS_H
#define TANKTV_CHEATS_H

/*
 * 三个作弊开关。
 *
 * 这个头文件会被 CMake 用 -include 强制包含进 main 这个 target 的每个编译单元,
 * 所以游戏源码里可以直接写 `tanktv_cheat_invincible` 而不用再加 include。
 * (补丁脚本 tools/patch_game_for_android.sh 就是这么用的)
 *
 * 因此这里必须同时是合法的 C 和 C++(android_glue.c 是 C)。
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int tanktv_cheat_infinite_lives;   /* 无限条命 */
extern int tanktv_cheat_strong_eagle;     /* 老巢加固(钢墙) */
extern int tanktv_cheat_invincible;       /* 无敌(一直保护罩) */

/* 启动时(chdir 之后、main() 之前)读回 cheats.txt */
void tanktvCheatsLoad(void);

/* 任一开关变化后写回 cheats.txt */
void tanktvCheatsSave(void);

/* 有任何一项开着就返回 1(作弊时最高分不落盘) */
int tanktvCheatsAnyOn(void);

#ifdef __cplusplus
}
#endif

#endif /* TANKTV_CHEATS_H */
