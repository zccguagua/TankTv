#ifndef TANKTV_CHEAT_MENU_H
#define TANKTV_CHEAT_MENU_H

#include "app_state/appstate.h"

/**
 * 主菜单里 "Cheat" 这一项打开的界面。
 *
 * 只做三件事:上下选、OK 开关、返回上一步。
 * 开关的存储和实际效果都不在这里(TankTvCheats.cpp + 补丁进游戏源码的那几处)。
 *
 * 这个类不在 game/ 目录里,是我们自己的文件,所以不会被"重新下载游戏源码"冲掉。
 */
class TankTvCheatMenu : public AppState
{
public:
    TankTvCheatMenu(InteractiveComponents interactive_components, StateMachine *state_machine);

    void draw(Renderer &renderer) override;
    void update(const UpdateState &updateState) override;
    void eventProcess(const Event &event) override;

private:
    /** 第 index 个菜单项对应的开关变量 */
    static int *flagAt(int index);

    int m_index;
};

#endif // TANKTV_CHEAT_MENU_H
