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

# 递归收集所有 .cpp(游戏有 engine/、app_state/、objects/ 等子目录)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
list(LENGTH GAME_SOURCES GAME_SOURCE_COUNT)
message(STATUS "游戏源文件数量: ${GAME_SOURCE_COUNT}")

add_library(main SHARED)

target_sources(main PRIVATE
    ${GAME_SOURCES}
    # 阶段 D:chdir 的 JNI 胶水(android.system.Os 没有公开 chdir)
    "${CMAKE_CURRENT_SOURCE_DIR}/android_glue.c"
    # SDL_ttf 占位实现(阶段 G 换成 stb_truetype)
    "${CMAKE_CURRENT_SOURCE_DIR}/../sdl_ttf_stub/src/sdl_ttf_stub.c"
)

target_include_directories(main PRIVATE
    "${GAME_DIR}/src"
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
target_compile_options(main PRIVATE -Wno-narrowing)

# 让 logcat 里能直接看到游戏自己的输出
target_compile_definitions(main PRIVATE __ANDROID__)
