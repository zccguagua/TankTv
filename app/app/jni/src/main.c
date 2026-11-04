/**
 * 阶段 B 引导程序 —— 只验证「工具链 + SDL2 + 画面 + 按键」是否通。
 *
 * 阶段 C 会把它替换成游戏真正的 main.cpp。
 *
 * 它额外做了一件事:把收到的每个按键的 scancode / keycode 打到 logcat,
 * 用来确认 Android TV 遥控器的方向键和 OK 键到底映射成什么(文档 8.2)。
 */
#include <SDL.h>
#include <SDL_main.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_Log("=== TankTv bootstrap ===");
    SDL_Log("SDL version: %d.%d.%d",
            SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "TankTv",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("accelerated renderer failed, falling back: %s", SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) == 0)
        SDL_Log("renderer: %s", info.name);

    int running = 1;
    unsigned frame = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_KEYDOWN:
                SDL_Log("KEYDOWN scancode=%d sym=%d",
                        e.key.keysym.scancode, e.key.keysym.sym);
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    running = 0;
                break;
            case SDL_KEYUP:
                SDL_Log("KEYUP   scancode=%d sym=%d",
                        e.key.keysym.scancode, e.key.keysym.sym);
                break;
            case SDL_FINGERDOWN:
                SDL_Log("FINGERDOWN x=%f y=%f", e.tfinger.x, e.tfinger.y);
                break;
            case SDL_WINDOWEVENT:
                SDL_Log("WINDOWEVENT %d", e.window.event);
                break;
            default:
                break;
            }
        }

        /* 缓慢渐变的底色:能看出是否真的在逐帧刷新 */
        Uint8 r = (Uint8)((frame / 3) % 256);
        SDL_SetRenderDrawColor(renderer, r, 40, 120, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        frame++;
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
