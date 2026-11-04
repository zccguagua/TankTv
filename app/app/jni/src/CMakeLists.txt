cmake_minimum_required(VERSION 3.6)

project(MY_APP)

# SDL2 目标由上层 CMakeLists.txt 里的 add_subdirectory(SDL) 提供。
# 阶段 C 会把这里的 main.c 换成游戏真正的源码。
add_library(main SHARED)

target_sources(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)

target_link_libraries(main SDL2)

# 游戏是 C++(阶段 C 起),提前把标准打开,避免后面每加一个源文件就踩一次坑
set_target_properties(main PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
