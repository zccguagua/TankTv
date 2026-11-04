cmake_minimum_required(VERSION 3.6)

project(GAME)

# ---------------------------------------------------------------------------
# 本文件由 SDL 官方 android-project/app/jni/CMakeLists.txt 改写。
# 原始模板用的是 add_subdirectory(SDL),要求 SDL 源码放在 app/jni/SDL。
# 我们不想把 90MB 的 SDL 复制进 app/,所以改成指向 TankTv/third_party/SDL。
#
# 目录关系:  TankTv/app/app/jni  →  ../../../  =  TankTv/
# ---------------------------------------------------------------------------
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party")

if(NOT EXISTS "${THIRD_PARTY_DIR}/SDL/CMakeLists.txt")
    message(FATAL_ERROR "SDL 源码不存在: ${THIRD_PARTY_DIR}/SDL  —— 请先按文档 4.1 克隆 SDL")
endif()

add_subdirectory("${THIRD_PARTY_DIR}/SDL" SDL)

# 伴随库:后面阶段按需打开(资源/声音/文字)
#if(EXISTS "${THIRD_PARTY_DIR}/SDL_image/CMakeLists.txt")
#    add_subdirectory("${THIRD_PARTY_DIR}/SDL_image" SDL_image)
#endif()
#if(EXISTS "${THIRD_PARTY_DIR}/SDL_mixer/CMakeLists.txt")
#    add_subdirectory("${THIRD_PARTY_DIR}/SDL_mixer" SDL_mixer)
#endif()

# 应用本体
add_subdirectory(src)
