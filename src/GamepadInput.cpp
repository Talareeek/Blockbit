#include "../include/GamepadInput.hpp"

#include "../include/GameCommon.hpp"
#include "../include/Settings.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
    // Returns stick value in [-1, 1] with Settings' deadzone subtracted and
    // the remaining range rescaled, so the response curve is smooth from the
    // deadzone edge to the full deflection.
    float normalizeAxis(float raw, float deadzone)
    {
        const float magnitude = std::abs(raw);
        if(magnitude <= deadzone) return 0.0f;

        const float sign = (raw < 0.0f) ? -1.0f : 1.0f;
        const float scaled = (magnitude - deadzone) / std::max(1.0f, 100.0f - deadzone);
        return sign * std::clamp(scaled, 0.0f, 1.0f);
    }

    // Hands the Image an anti-aliased crosshair. Simple enough to not need a
    // texture asset; stays visible against bright and dark backgrounds thanks
    // to the 1px outline added at the end.
    void buildCrosshairImage(sf::Image& img, sf::Color fill)
    {
        const sf::Vector2u size = img.getSize();
        if(size.x == 0 || size.y == 0) return;

        const int s = static_cast<int>(size.x);
        const int cx = s / 2;
        const int cy = s / 2;
        const int outer = s / 2 - 1;
        const int inner_gap = std::max(2, s / 6);
        const int thickness = std::max(1, s / 16);

        auto plot = [&](int x, int y, sf::Color c)
        {
            if(x < 0 || y < 0 || x >= s || y >= s) return;
            img.setPixel({static_cast<std::uint32_t>(x),
                          static_cast<std::uint32_t>(y)}, c);
        };

        // Arms.
        for(int i = inner_gap; i <= outer; ++i)
        {
            for(int t = -thickness; t <= thickness; ++t)
            {
                plot(cx + i, cy + t, fill);
                plot(cx - i, cy + t, fill);
                plot(cx + t, cy + i, fill);
                plot(cx + t, cy - i, fill);
            }
        }

        // Center dot for visibility.
        const int dot_r = std::max(1, s / 14);
        for(int y = -dot_r; y <= dot_r; ++y)
        for(int x = -dot_r; x <= dot_r; ++x)
        {
            if(x * x + y * y <= dot_r * dot_r) plot(cx + x, cy + y, fill);
        }

        // 1px dark outline so the cursor pops on light surfaces.
        const sf::Color outline(0, 0, 0, 220);
        for(int y = 0; y < s; ++y)
        for(int x = 0; x < s; ++x)
        {
            const sf::Color here = img.getPixel({static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)});
            if(here.a != 0) continue;

            bool adjacent = false;
            for(int dy = -1; dy <= 1 && !adjacent; ++dy)
            for(int dx = -1; dx <= 1 && !adjacent; ++dx)
            {
                const int nx = x + dx;
                const int ny = y + dy;
                if(nx < 0 || ny < 0 || nx >= s || ny >= s) continue;
                if(img.getPixel({static_cast<std::uint32_t>(nx),
                                 static_cast<std::uint32_t>(ny)}).a != 0 &&
                   !(dx == 0 && dy == 0))
                {
                    adjacent = true;
                }
            }
            if(adjacent) plot(x, y, outline);
        }
    }
}

GamepadInput::GamepadInput() = default;

bool GamepadInput::isConnected() const
{
    return Settings::gamepad.enabled &&
           sf::Joystick::isConnected(Settings::gamepad.joystick_id);
}

void GamepadInput::reset()
{
    prev_buttons.reset();
    prev_pov_x_step = 0;
    prev_mouse_sampled = false;
}

void GamepadInput::update(float dt, const sf::RenderWindow& window)
{
    if(!Settings::gamepad.enabled)
    {
        cursor_active = false;
        return;
    }

    sf::Joystick::update();

    if(!sf::Joystick::isConnected(Settings::gamepad.joystick_id))
    {
        cursor_active = false;
        return;
    }

    const std::uint32_t joy = Settings::gamepad.joystick_id;
    const float deadzone = Settings::gamepad.stick_deadzone;

    // Right stick lives on SFML's U/V axes for the common XInput mapping;
    // some drivers expose it as R/Z instead, so fall back if U/V aren't
    // reported.
    float rx_raw = 0.0f;
    float ry_raw = 0.0f;
    if(sf::Joystick::hasAxis(joy, sf::Joystick::Axis::U))      rx_raw = sf::Joystick::getAxisPosition(joy, sf::Joystick::Axis::U);
    else if(sf::Joystick::hasAxis(joy, sf::Joystick::Axis::Z)) rx_raw = sf::Joystick::getAxisPosition(joy, sf::Joystick::Axis::Z);
    if(sf::Joystick::hasAxis(joy, sf::Joystick::Axis::V))      ry_raw = sf::Joystick::getAxisPosition(joy, sf::Joystick::Axis::V);
    else if(sf::Joystick::hasAxis(joy, sf::Joystick::Axis::R)) ry_raw = sf::Joystick::getAxisPosition(joy, sf::Joystick::Axis::R);

    const float rx = normalizeAxis(rx_raw, deadzone);
    const float ry = normalizeAxis(ry_raw, deadzone);

    const bool right_stick_active = (rx != 0.0f || ry != 0.0f);

    if(right_stick_active)
    {
        // World units per second at full stick deflection.
        const double speed = static_cast<double>(Settings::gamepad.cursor_speed);
        cursor_offset.x += static_cast<double>(rx) * speed * static_cast<double>(dt);
        // World-space Y increases downward on screen, but with SFML's flipped
        // view (set in ClientGameState) +Y on the right stick should move the
        // cursor down in world units. The view orientation is already
        // handled; keep this as raw up-for-negative to match the mouse.
        cursor_offset.y += static_cast<double>(ry) * speed * static_cast<double>(dt);

        const double max_dist = static_cast<double>(Settings::gamepad.cursor_max_distance);
        const double len = std::hypot(cursor_offset.x, cursor_offset.y);
        if(len > max_dist && len > 0.0)
        {
            cursor_offset.x = cursor_offset.x / len * max_dist;
            cursor_offset.y = cursor_offset.y / len * max_dist;
        }

        cursor_active = true;
    }

    // Source switching: if the mouse was moved since last sample, demote the
    // gamepad back to the inactive state so the familiar mouse cursor wins.
    sf::Vector2i mouse_now = sf::Mouse::getPosition(window);
    if(prev_mouse_sampled)
    {
        const int dx = mouse_now.x - prev_mouse_pixel.x;
        const int dy = mouse_now.y - prev_mouse_pixel.y;
        // Small nudge threshold so the gamepad cursor doesn't blink off from
        // sensor jitter — 3 px at desktop resolutions is comfortably past
        // typical dead sensor noise.
        if(std::abs(dx) + std::abs(dy) > 3 && !right_stick_active)
        {
            cursor_active = false;
        }
    }
    prev_mouse_pixel = mouse_now;
    prev_mouse_sampled = true;
}

sf::Vector2<double> GamepadInput::cursorWorldPosition(sf::Vector2<double> camera,
                                                      std::optional<sf::Vector2<double>> player_center) const
{
    sf::Vector2<double> origin = player_center.value_or(camera);
    return origin + cursor_offset;
}

int GamepadInput::pollAnyPressedButton() const
{
    if(!sf::Joystick::isConnected(Settings::gamepad.joystick_id)) return -1;

    const unsigned int count = sf::Joystick::getButtonCount(Settings::gamepad.joystick_id);
    for(unsigned int i = 0; i < count && i < 32; ++i)
    {
        if(sf::Joystick::isButtonPressed(Settings::gamepad.joystick_id, i)) return static_cast<int>(i);
    }
    return -1;
}

std::vector<Input> GamepadInput::collectTickInputs()
{
    std::vector<Input> out;

    if(!Settings::gamepad.enabled) return out;
    if(!sf::Joystick::isConnected(Settings::gamepad.joystick_id)) return out;

    const std::uint32_t joy = Settings::gamepad.joystick_id;
    const float deadzone = Settings::gamepad.stick_deadzone;

    // Left stick → MOVE. Horizontal only — existing MOVE handling is 1D.
    float lx_raw = 0.0f;
    if(sf::Joystick::hasAxis(joy, sf::Joystick::Axis::X))
        lx_raw = sf::Joystick::getAxisPosition(joy, sf::Joystick::Axis::X);
    const float lx = normalizeAxis(lx_raw, deadzone);
    if(lx != 0.0f)
    {
        out.push_back({InputType::MOVE, MoveData{{static_cast<double>(lx), 0.0}, false}});
    }

    // Button edge detection.
    std::bitset<32> curr_buttons;
    const unsigned int button_count = sf::Joystick::getButtonCount(joy);
    for(unsigned int i = 0; i < button_count && i < 32; ++i)
    {
        if(sf::Joystick::isButtonPressed(joy, i)) curr_buttons.set(i);
    }

    auto isPressed = [&](int button_idx) -> bool
    {
        if(button_idx < 0 || button_idx >= 32) return false;
        return curr_buttons.test(static_cast<std::size_t>(button_idx));
    };
    auto wasPressed = [&](int button_idx) -> bool
    {
        if(button_idx < 0 || button_idx >= 32) return false;
        return prev_buttons.test(static_cast<std::size_t>(button_idx));
    };
    auto justPressed = [&](int b) { return isPressed(b) && !wasPressed(b); };
    auto justReleased = [&](int b) { return !isPressed(b) && wasPressed(b); };

    const auto& map = Settings::gamepad.buttons;

    const int b_jump     = map[static_cast<std::size_t>(Settings::Gamepad::Action::Jump)];
    const int b_attack   = map[static_cast<std::size_t>(Settings::Gamepad::Action::Attack)];
    const int b_use      = map[static_cast<std::size_t>(Settings::Gamepad::Action::Use)];
    const int b_drop     = map[static_cast<std::size_t>(Settings::Gamepad::Action::Drop)];
    const int b_drop_f   = map[static_cast<std::size_t>(Settings::Gamepad::Action::DropFull)];
    const int b_prev     = map[static_cast<std::size_t>(Settings::Gamepad::Action::SlotPrev)];
    const int b_next     = map[static_cast<std::size_t>(Settings::Gamepad::Action::SlotNext)];
    const int b_inv      = map[static_cast<std::size_t>(Settings::Gamepad::Action::OpenInventory)];

    // Jump: held each tick while the button is down — matches the Space-key
    // polling path in getInputs().
    if(isPressed(b_jump)) out.push_back({InputType::JUMP});

    // Attack / Use are edge-triggered to match the mouse (START on press,
    // STOP on release).
    if(justPressed(b_attack))  out.push_back({InputType::ATTACK_START});
    if(justReleased(b_attack)) out.push_back({InputType::ATTACK_STOP});
    if(justPressed(b_use))     out.push_back({InputType::USE_START});
    if(justReleased(b_use))    out.push_back({InputType::USE_STOP});

    if(justPressed(b_drop))   out.push_back({InputType::DROP, false});
    if(justPressed(b_drop_f)) out.push_back({InputType::DROP, true});

    if(justPressed(b_inv))    out.push_back({InputType::OPEN_INVENTORY});

    // Shoulder-style previous/next-slot — ChangeSlotData's scroll=true path
    // interprets the value as a delta, so ±1 does what the mouse wheel does.
    if(justPressed(b_prev)) out.push_back({InputType::CHANGE_SLOT, ChangeSlotData{true, -1}});
    if(justPressed(b_next)) out.push_back({InputType::CHANGE_SLOT, ChangeSlotData{true,  1}});

    // D-Pad X (PovX) → one-shot slot change on direction change. SFML
    // reports PovX as -100 / 0 / +100.
    int pov_step = 0;
    if(sf::Joystick::hasAxis(joy, sf::Joystick::Axis::PovX))
    {
        const float raw = sf::Joystick::getAxisPosition(joy, sf::Joystick::Axis::PovX);
        if(raw > 50.0f)       pov_step = 1;
        else if(raw < -50.0f) pov_step = -1;
    }
    if(pov_step != 0 && pov_step != prev_pov_x_step)
    {
        out.push_back({InputType::CHANGE_SLOT, ChangeSlotData{true, static_cast<std::int8_t>(pov_step)}});
    }
    prev_pov_x_step = pov_step;

    prev_buttons = curr_buttons;

    return out;
}

void GamepadInput::rebuildCursorTextureIfNeeded()
{
    const std::uint32_t size = std::clamp<std::uint32_t>(Settings::gamepad.cursor_size, 8, 128);
    sf::Color color = Settings::gamepad.cursor_color;
    color.a = 255;

    if(texture_ready && cached_size == size && cached_color == color) return;

    sf::Image img({size, size}, sf::Color(0, 0, 0, 0));
    buildCrosshairImage(img, color);

    if(cursor_texture.loadFromImage(img))
    {
        cursor_texture.setSmooth(false);
        texture_ready = true;
        cached_size = size;
        cached_color = color;
    }
}

void GamepadInput::render(sf::RenderWindow& window,
                          sf::Vector2<double> camera,
                          std::optional<sf::Vector2<double>> player_center)
{
    if(!Settings::gamepad.enabled)     return;
    if(!Settings::gamepad.show_cursor) return;
    if(!cursor_active)                 return;

    rebuildCursorTextureIfNeeded();
    if(!texture_ready) return;

    const sf::Vector2<double> world = cursorWorldPosition(camera, player_center);

    // Convert world → window pixels. Matches getMouseWorldPosition's inverse:
    // unit_size is window.height / WORLD_UNIT_SIZE_FACTOR, and the game view
    // is y-flipped (negative height), so world-Y maps straight through.
    const sf::Vector2u win = window.getSize();
    if(win.x == 0 || win.y == 0) return;

    const double unit = static_cast<double>(win.y) / static_cast<double>(WORLD_UNIT_SIZE_FACTOR);

    const double sx = (world.x - camera.x) * unit + static_cast<double>(win.x) * 0.5;
    const double sy = static_cast<double>(win.y) * 0.5 - (world.y - camera.y) * unit;

    sf::Sprite sprite(cursor_texture);
    const sf::Vector2u tsize = cursor_texture.getSize();
    sprite.setOrigin({static_cast<float>(tsize.x) * 0.5f,
                      static_cast<float>(tsize.y) * 0.5f});
    sprite.setPosition({static_cast<float>(sx), static_cast<float>(sy)});

    window.draw(sprite);
}
