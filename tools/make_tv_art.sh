#!/bin/sh
# 从游戏本体的贴图里切出 Android 图标和 TV banner。
# 可重复执行(幂等):素材(assets/texture.png)一改,重跑一次就行。
#
# 用法:  sh tools/make_tv_art.sh
#
# 贴图坐标来自 game/src/spriteconfig.cpp:
#   ST_PLAYER_1 = {640, 0, 32, 32}   玩家坦克(朝上第 1 帧)
#   ST_TANKS_LOGO = {0, 260, 406, 72}
set -e
cd "$(dirname "$0")/.."

TEX=app/app/src/main/assets/texture.png
RES=app/app/src/main/res
FONT=app/app/src/main/assets/kongtext.ttf
BG=0x101010

if [ ! -f "$TEX" ]; then
    echo "缺少 $TEX —— 先按施工文档 6.1 放资源" >&2
    exit 1
fi

# 图标:深色底 + 放大的坦克,整数倍缩放保证像素不糊
make_icon() {
    size=$1
    inner=$2
    off=$(( (size - inner) / 2 ))
    ffmpeg -y -v error -i "$TEX" -frames:v 1 \
        -vf "crop=32:32:640:0,scale=${inner}:${inner}:flags=neighbor,pad=${size}:${size}:${off}:${off}:color=${BG}" \
        "$RES/$3/ic_launcher.png"
    echo "  $3/ic_launcher.png  ${size}x${size}"
}

echo "生成图标:"
make_icon 48  40 mipmap-mdpi
make_icon 72  60 mipmap-hdpi
make_icon 96  80 mipmap-xhdpi
make_icon 144 120 mipmap-xxhdpi
make_icon 192 160 mipmap-xxxhdpi

# TV banner:320x180(TV 首页横条)。黑底 + TANKS LOGO + 一行小字
echo "生成 banner:"
mkdir -p "$RES/drawable"
cp "$FONT" ./.tv-font.ttf
ffmpeg -y -v error \
    -f lavfi -i "color=c=0x0d0d0d:s=320x180:d=1" \
    -i "$TEX" -frames:v 1 \
    -filter_complex "[1:v]crop=406:72:0:260,scale=240:43:flags=neighbor[logo];[0:v][logo]overlay=(W-w)/2:40,drawtext=fontfile=./.tv-font.ttf:text='ANDROID TV':fontcolor=0x9aa0a6:fontsize=16:x=(w-text_w)/2:y=112" \
    "$RES/drawable/tv_banner.png"
rm -f ./.tv-font.ttf
echo "  drawable/tv_banner.png  320x180"

echo "完成。"
