/**
 * SDL_ttf 的最小占位头文件 —— 阶段 C/D 用。
 *
 * 为什么需要它:游戏的 sdl_renderer.cpp 用 SDL2_ttf 画文字,而 SDL2_ttf 2.24
 * 强依赖 freetype + harfbuzz,Android 交叉编译成本很高。
 *
 * 阶段 G 会用 stb_truetype 直接读 kongtext.ttf 渲染文字,届时删掉本目录即可。
 *
 * 安全性:sdl_renderer.cpp 里对 TTF_OpenFont / TTF_RenderText_Solid 的返回值
 * 都做了判空并直接 return,所以这里恒返回 NULL 不会崩溃,只是没有文字。
 */
#ifndef SDL_TTF_H_STUB
#define SDL_TTF_H_STUB

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TTF_Font TTF_Font;

extern int TTF_Init(void);
extern void TTF_Quit(void);

extern TTF_Font *TTF_OpenFont(const char *file, int ptsize);
extern void TTF_CloseFont(TTF_Font *font);

extern SDL_Surface *TTF_RenderText_Solid(TTF_Font *font, const char *text, SDL_Color fg);

extern const char *TTF_GetError(void);

#ifdef __cplusplus
}
#endif

#endif /* SDL_TTF_H_STUB */
