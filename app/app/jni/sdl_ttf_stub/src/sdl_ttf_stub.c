/**
 * SDL_ttf 占位实现:能链接、不崩溃、不画字。
 * 真正的文字渲染在阶段 G 用 stb_truetype 实现。
 */
#include <SDL2/SDL_ttf.h>

int TTF_Init(void)
{
    /* 返回 0 表示成功,避免 SDLRenderer 构造时抛异常 */
    return 0;
}

void TTF_Quit(void)
{
}

TTF_Font *TTF_OpenFont(const char *file, int ptsize)
{
    (void)file;
    (void)ptsize;
    /* 返回 NULL:调用方已有判空,会静默跳过绘制 */
    return NULL;
}

void TTF_CloseFont(TTF_Font *font)
{
    (void)font;
}

SDL_Surface *TTF_RenderText_Solid(TTF_Font *font, const char *text, SDL_Color fg)
{
    (void)font;
    (void)text;
    (void)fg;
    return NULL;
}

const char *TTF_GetError(void)
{
    return "SDL_ttf stub: text rendering not implemented yet (stage G)";
}
