cmake_minimum_required(VERSION 3.6)

project(GAME)

# ---------------------------------------------------------------------------
# 由 SDL 官方 android-project/app/jni/CMakeLists.txt 改写。
#
# 原始模板用 add_subdirectory(SDL),要求 SDL 源码放在 app/jni/SDL。
# 我们不想把 SDL 复制进 app/,所以改成指向 TankTv/third_party/。
#
# 目录关系:  TankTv/app/app/jni  →  ../../../  =  TankTv/
# ---------------------------------------------------------------------------
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party")

if(NOT EXISTS "${THIRD_PARTY_DIR}/SDL/CMakeLists.txt")
    message(FATAL_ERROR "SDL 源码不存在: ${THIRD_PARTY_DIR}/SDL  —— 请先按文档 4.1 获取")
endif()

add_subdirectory("${THIRD_PARTY_DIR}/SDL" SDL)

# SDL_image:PNG 解码走 stb_image,不需要 libpng
# SDL_mixer:OGG 解码走 stb_vorbis,不需要 libogg/libvorbis
add_subdirectory("${THIRD_PARTY_DIR}/SDL_image" SDL_image)
add_subdirectory("${THIRD_PARTY_DIR}/SDL_mixer" SDL_mixer)

# 应用本体(游戏源码由 src/CMakeLists.txt 引入)
add_subdirectory(src)
