#include "TankTvCheatMenu.h"

#include "TankTvCheats.h"
#include "appconfig.h"
#include "soundconfig.h"
#include "app_state/menu.h"

#include <string>

namespace {

const char *kLabels[] = {
    "INFINITE LIVES",
    "FORTIFY EAGLE",
    "INVINCIBLE",
};
const int kItemCount = 3;
const int kFirstItemY = 200;
const int kItemHeight = 28;

const Color kWhite = {255, 255, 255, 255};
const Color kGrey = {150, 150, 150, 255};
const Color kOnColor = {120, 220, 120, 255};
const Color kOffColor = {170, 110, 110, 255};

} // namespace

int *TankTvCheatMenu::flagAt(int index)
{
    switch (index)
    {
    case 0:
        return &tanktv_cheat_infinite_lives;
    case 1:
        return &tanktv_cheat_strong_eagle;
    default:
        return &tanktv_cheat_invincible;
    }
}

TankTvCheatMenu::TankTvCheatMenu(InteractiveComponents interactive_components, StateMachine *state_machine)
    : AppState(interactive_components, state_machine), m_index(0)
{
}

void TankTvCheatMenu::draw(Renderer &renderer)
{
    renderer.clear();

    /* 和主菜单一样的黑底(地图区 + 状态栏区) */
    renderer.drawRect(AppConfig::map_rect, {0, 0, 0, 255}, true);
    renderer.drawRect(AppConfig::status_rect, {0, 0, 0, 255}, true);

    renderer.drawText({-1, 130}, "CHEAT", kWhite, FontSize::BIGGER);

    for (int i = 0; i < kItemCount; i++)
    {
        const int y = kFirstItemY + i * kItemHeight;
        const bool on = (*flagAt(i)) != 0;

        renderer.drawText({60, y}, std::string(i == m_index ? "> " : "  ") + kLabels[i], kWhite, FontSize::BIG);
        renderer.drawText({330, y}, on ? "ON" : "OFF", on ? kOnColor : kOffColor, FontSize::BIG);
    }

    renderer.drawText({-1, 330}, "OK:TOGGLE  BACK:RETURN", kGrey, FontSize::NORMAL);

    renderer.flush();
}

void TankTvCheatMenu::update(const UpdateState &)
{
}

void TankTvCheatMenu::eventProcess(const Event &event)
{
    if (event.type() != Event::KEYBOARD)
        return;

    const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);

    if (ev.isPressed(KeyCode::KEY_UP))
    {
        m_index = (m_index + kItemCount - 1) % kItemCount;
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (ev.isPressed(KeyCode::KEY_DOWN))
    {
        m_index = (m_index + 1) % kItemCount;
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (ev.isPressed(KeyCode::KEY_RETURN) || ev.isPressed(KeyCode::KEY_SPACE))
    {
        int *flag = flagAt(m_index);
        *flag = (*flag) ? 0 : 1;
        tanktvCheatsSave();
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (ev.isPressed(KeyCode::KEY_ESCAPE))
    {
        transiteTo(new Menu(m_interactive_components, m_state_machine));
    }
}
