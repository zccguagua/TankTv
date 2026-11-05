/* 转发头:游戏里写的是 #include <SDL2/SDL_mixer.h>,
 * 而 SDL_mixer 源码树里的头是 include/SDL_mixer.h。
 * SDL2_mixer 目标的 INTERFACE include 目录里能找到后者。 */
#ifndef TANKTV_SHIM_SDL_MIXER_H
#define TANKTV_SHIM_SDL_MIXER_H
#include <SDL_mixer.h>
#endif
