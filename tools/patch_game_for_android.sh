#!/bin/sh
# 阶段 K(作弊菜单):给游戏源码打补丁。
#
# 为什么要脚本:`game/` 在 .gitignore 里,换电脑要从 GitHub 重新下载,
# 手改的代码会丢。所以所有对上游源码的修改都写在这里,下载完跑一次即可。
#
# 用法:
#   sh tools/patch_game_for_android.sh
#   sh tools/patch_game_for_android.sh --check     # 只检查打了没,不改文件
#
# 每一处补丁都用「插入的那段文字」自己当标记,所以可以反复执行;
# 已经打过的那一条会自动跳过。
set -e
cd "$(dirname "$0")/.."

GAME=game/src
CHECK_ONLY=0
if [ "$1" = "--check" ]; then
    CHECK_ONLY=1
fi

if [ ! -f "$GAME/main.cpp" ]; then
    echo "找不到 $GAME/main.cpp —— 先按施工文档 4.1 下载游戏源码" >&2
    exit 1
fi

# 打一个补丁:$1=文件 $2=判断是否已打的标记 $3=sed 表达式
patch_once() {
    file=$1
    marker=$2
    expr=$3

    if grep -q -F "$marker" "$file"; then
        echo "  [skip ] $(basename "$file")"
        return
    fi

    if [ "$CHECK_ONLY" = "1" ]; then
        echo "  [TODO ] $(basename "$file")"
        return
    fi

    sed -i "$expr" "$file"
    echo "  [patch] $(basename "$file")"
}

echo "主菜单:增加 Cheat 入口"
patch_once "$GAME/app_state/menu.cpp" "TankTvCheatMenu.h" \
    's|#include "stage_select.h"|#include "stage_select.h"\
#include "TankTvCheatMenu.h"   /* TankTv: cheat menu */|'

patch_once "$GAME/app_state/menu.cpp" 'push_back("Cheat");' \
    's#    m_menu_items.push_back("2 Players");#    m_menu_items.push_back("2 Players");\
    m_menu_items.push_back("Cheat");   /* TankTv: cheat menu */#'

patch_once "$GAME/app_state/menu.cpp" 'm_current_menu_index == 2' \
    's#        else if (ev.isPressed(KeyCode::KEY_SPACE) || ev.isPressed(KeyCode::KEY_RETURN))#        else if ((ev.isPressed(KeyCode::KEY_SPACE) || ev.isPressed(KeyCode::KEY_RETURN)) \&\& m_current_menu_index == 2)\
        {\
            transiteTo(new TankTvCheatMenu(m_interactive_components, m_state_machine));\
        }\
        else if (ev.isPressed(KeyCode::KEY_SPACE) || ev.isPressed(KeyCode::KEY_RETURN))#'

echo "无敌:受伤时直接免伤 + 挂上护盾"
patch_once "$GAME/objects/tank/player/player.cpp" 'TankTv: invincible' \
    's#    if (hasEffectByType(Tank::TankEffectType::SHIELD))#    /* TankTv: invincible - never take damage, keep a shield on */\
    if (tanktv_cheat_invincible)\
    {\
        if (!hasEffectByType(Tank::TankEffectType::SHIELD))\
            activateShieldEffect();\
        return;\
    }\
\
    if (hasEffectByType(Tank::TankEffectType::SHIELD))#'

echo "无敌:护盾时长改成基本不过期"
patch_once "$GAME/objects/tank/tank.cpp" '86400000u' \
    's#AppConfig::tank_shield_time)$#(tanktv_cheat_invincible ? 86400000u : AppConfig::tank_shield_time))#'

echo "无限命:死亡时不减命"
patch_once "$GAME/objects/tank/player/player_destroyed_state.cpp" 'TankTv: infinite lives' \
    's#    if (player->to_erase)#    /* TankTv: infinite lives - refill before the decrement below */\
    if (tanktv_cheat_infinite_lives)\
        player->m_lives_count = 10;\
\
    if (player->to_erase)#'

echo "老巢加固:一直保持保护状态(钢墙)"
patch_once "$GAME/app_state/game/stage_environment.cpp" 'TankTv: fortify eagle' \
    's#^    if (m_eagle_protected)$#    /* TankTv: fortify eagle - always protected, walls stay steel */\
    if (tanktv_cheat_strong_eagle)\
    {\
        if (!m_eagle_protected)\
            startEagleProtection();\
        m_eagle_protection_time = 0;\
    }\
\
    if (m_eagle_protected)#'

echo
echo "检查结果(每一处都应该能看到 TankTv 标记):"
grep -n -F "TankTv:" "$GAME/app_state/menu.cpp" \
    "$GAME/app_state/game/stage_environment.cpp" \
    "$GAME/objects/tank/player/player.cpp" \
    "$GAME/objects/tank/player/player_destroyed_state.cpp" \
    "$GAME/objects/tank/tank.cpp" | sed 's#^game/src/##'
