cmake_minimum_required(VERSION 3.6)

project(MY_APP)

# ---------------------------------------------------------------------------
# 阶段 C:把上游游戏源码编进来
#
# 游戏源码位于 TankTv/game/src
# 目录关系:  TankTv/app/app/jni/src  →  ../../../../  =  TankTv/
# ---------------------------------------------------------------------------
set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../game")

if(NOT EXISTS "${GAME_DIR}/src/main.cpp")
    message(FATAL_ERROR "游戏源码不存在: ${GAME_DIR}/src —— 请先按文档 4.1 获取")
endif()

# ---------------------------------------------------------------------------
# 作弊菜单需要游戏源码打补丁(见 tools/patch_game_for_android.sh)。
# 没打补丁的话编译也能过,但菜单入口会静默消失 —— 所以这里直接报错。
# ---------------------------------------------------------------------------
file(READ "${GAME_DIR}/src/app_state/menu.cpp" GAME_MENU_SOURCE)
if(NOT GAME_MENU_SOURCE MATCHES "TankTvCheatMenu.h")
    message(FATAL_ERROR
        "游戏源码还没打补丁。请在项目根目录执行:\n"
        "    sh tools/patch_game_for_android.sh\n"
        "(每次重新下载 game/ 之后都要跑一次)")
endif()

# 递归收集所有 .cpp(游戏有 engine/、app_state/、objects/ 等子目录)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
list(LENGTH GAME_SOURCES GAME_SOURCE_COUNT)
message(STATUS "游戏源文件数量: ${GAME_SOURCE_COUNT}")

add_library(main SHARED)

target_sources(main PRIVATE
    ${GAME_SOURCES}
    # 阶段 D:chdir 的 JNI 胶水(android.system.Os 没有公开 chdir)
    "${CMAKE_CURRENT_SOURCE_DIR}/android_glue.c"
    # 阶段 H:最高分读写(不改上游代码,直接操作 HighScore::score)
    "${CMAKE_CURRENT_SOURCE_DIR}/high_score_io.cpp"
    # 作弊开关 + 作弊菜单(我们自己的文件,不会被重下游戏源码冲掉)
    "${CMAKE_CURRENT_SOURCE_DIR}/TankTvCheats.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TankTvCheatMenu.cpp"
    # SDL_ttf 占位实现(阶段 G 已经变成 stb_truetype 版本)
    # 注意:这仍然只是个「SDL_ttf 子集」,不是真的 SDL_ttf
    "${CMAKE_CURRENT_SOURCE_DIR}/../sdl_ttf_stub/src/sdl_ttf_stub.c"
)

target_include_directories(main PRIVATE
    "${GAME_DIR}/src"
    # 我们自己的头(TankTvCheatMenu.h / TankTvCheats.h)
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../sdl_ttf_stub/include"
    # 转发头:<SDL2/SDL_image.h> 与 <SDL2/SDL_mixer.h>
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)

target_link_libraries(main
    SDL2
    SDL2_image
    SDL2_mixer
    # android_glue.c 里用了 __android_log_print
    log
)

set_target_properties(main PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 游戏原有构建参数(Makefile 里 Linux 分支用的)
# -include: 把作弊开关的 extern 声明强制包含进本 target 的每个编译单元,
#           这样 tools/patch_game_for_android.sh 插入游戏源码的
#           `tanktv_cheat_*` 引用才能编译过(不用改上游的 include 列表)。
target_compile_options(main PRIVATE
    -Wno-narrowing
    -include "${CMAKE_CURRENT_SOURCE_DIR}/TankTvCheats.h"
)

# 让 logcat 里能直接看到游戏自己的输出
target_compile_definitions(main PRIVATE __ANDROID__)
