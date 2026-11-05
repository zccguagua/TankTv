/* 转发头:游戏里写的是 #include <SDL2/SDL_image.h>,
 * 而 SDL_image 源码树里的头是 include/SDL_image.h。
 * SDL2_image 目标的 INTERFACE include 目录里能找到后者。 */
#ifndef TANKTV_SHIM_SDL_IMAGE_H
#define TANKTV_SHIM_SDL_IMAGE_H
#include <SDL_image.h>
#endif
