/**
 * 阶段 G:用 stb_truetype(单头文件)实现 SDL_ttf 的最小可用子集。
 *
 * 游戏只用这几个函数:
 *   TTF_Init / TTF_Quit / TTF_OpenFont / TTF_CloseFont
 *   TTF_RenderText_Solid / TTF_GetError
 *
 * 因此不需要 freetype / harfbuzz,直接读 kongtext.ttf 画字形。
 * 游戏里的文字全是 ASCII(菜单、分数、GAME OVER 等),这里按单字节码点处理。
 */
#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _TTF_Font {
    stbtt_fontinfo info;
    unsigned char *data;   /* 原始字体数据,保持存活 */
    int ptsize;
    float scale;           /* 像素高度缩放 */
    int ascent;
    int descent;
    int line_gap;
};

static char g_last_error[256] = "OK";

static void set_error(const char *fmt, const char *arg)
{
    snprintf(g_last_error, sizeof(g_last_error), fmt, arg ? arg : "");
}

int TTF_Init(void)
{
    return 0;
}

void TTF_Quit(void)
{
}

TTF_Font *TTF_OpenFont(const char *file, int ptsize)
{
    if (file == NULL) {
        set_error("TTF_OpenFont: null path", NULL);
        return NULL;
    }
    if (ptsize <= 0) {
        set_error("TTF_OpenFont: bad point size", NULL);
        return NULL;
    }

    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
        set_error("TTF_OpenFont: cannot open %s", file);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        set_error("TTF_OpenFont: seek failed %s", file);
        return NULL;
    }
    long size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        set_error("TTF_OpenFont: empty file %s", file);
        return NULL;
    }
    rewind(fp);

    unsigned char *data = (unsigned char *)malloc((size_t)size);
    if (data == NULL) {
        fclose(fp);
        set_error("TTF_OpenFont: out of memory", NULL);
        return NULL;
    }
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        fclose(fp);
        free(data);
        set_error("TTF_OpenFont: read failed %s", file);
        return NULL;
    }
    fclose(fp);

    TTF_Font *font = (TTF_Font *)calloc(1, sizeof(TTF_Font));
    if (font == NULL) {
        free(data);
        set_error("TTF_OpenFont: out of memory", NULL);
        return NULL;
    }

    int offset = stbtt_GetFontOffsetForIndex(data, 0);
    if (offset < 0 || stbtt_InitFont(&font->info, data, offset) == 0) {
        free(data);
        free(font);
        set_error("TTF_OpenFont: invalid font %s", file);
        return NULL;
    }

    font->data = data;
    font->ptsize = ptsize;
    font->scale = stbtt_ScaleForPixelHeight(&font->info, (float)ptsize);
    stbtt_GetFontVMetrics(&font->info, &font->ascent, &font->descent, &font->line_gap);

    return font;
}

void TTF_CloseFont(TTF_Font *font)
{
    if (font == NULL)
        return;
    free(font->data);
    free(font);
}

SDL_Surface *TTF_RenderText_Solid(TTF_Font *font, const char *text, SDL_Color fg)
{
    if (font == NULL || text == NULL || *text == '\0')
        return NULL;

    const float scale = font->scale;
    const int baseline = (int)(font->ascent * scale);

    /* 第一遍:算总宽度 */
    int total_width = 0;
    for (const char *p = text; *p; ++p) {
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&font->info, (unsigned char)*p, &advance, &lsb);
        total_width += (int)(advance * scale);
    }

    int width = total_width + 2;
    int height = (int)((font->ascent - font->descent) * scale) + 2;
    if (width <= 0 || height <= 0)
        return NULL;

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL)
        return NULL;

    /* 关键:带 per-pixel alpha 的 surface 默认 blend mode 是 NONE,
     * SDL_CreateTextureFromSurface 不会自动改(只有 colorkey surface 才会),
     * 那样透明背景会被画成黑底。这里显式设为 BLEND。 */
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    /* 透明底色 */
    memset(surface->pixels, 0, (size_t)surface->pitch * (size_t)surface->h);

    int pen_x = 1;
    for (const char *p = text; *p; ++p) {
        int codepoint = (unsigned char)*p;
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance, &lsb);

        int gw = 0, gh = 0, xoff = 0, yoff = 0;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&font->info, scale, scale, codepoint, &gw, &gh, &xoff, &yoff);
        if (bitmap != NULL) {
            int dst_x = pen_x + xoff;
            int dst_y = baseline + yoff;
            for (int row = 0; row < gh; ++row) {
                for (int col = 0; col < gw; ++col) {
                    unsigned char alpha = bitmap[row * gw + col];
                    if (alpha == 0)
                        continue;
                    int px = dst_x + col;
                    int py = dst_y + row;
                    if (px < 0 || py < 0 || px >= width || py >= height)
                        continue;
                    Uint32 *pixel = (Uint32 *)((Uint8 *)surface->pixels + (size_t)py * surface->pitch + (size_t)px * 4);
                    *pixel = SDL_MapRGBA(surface->format, fg.r, fg.g, fg.b, alpha);
                }
            }
            free(bitmap);
        }

        pen_x += (int)(advance * scale);
    }

    return surface;
}

const char *TTF_GetError(void)
{
    return g_last_error;
}
