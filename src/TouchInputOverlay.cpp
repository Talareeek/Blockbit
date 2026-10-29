#include "../include/TouchInputOverlay.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kButtonSize    = 180.0f;
    constexpr float kButtonSpacing = 30.0f;
    constexpr float kMargin        = 60.0f;

    sf::Vector2f toVec2f(sf::Vector2i v)
    {
        return {static_cast<float>(v.x), static_cast<float>(v.y)};
    }

    bool rectContains(sf::FloatRect r, sf::Vector2f p)
    {
        return p.x >= r.position.x && p.x <= r.position.x + r.size.x &&
               p.y >= r.position.y && p.y <= r.position.y + r.size.y;
    }
}

void TouchInputOverlay::relayout(sf::Vector2u window_size)
{
    last_window_size = window_size;

    const float w = static_cast<float>(window_size.x);
    const float h = static_cast<float>(window_size.y);

    // Joystick: lower-left quadrant.
    joystick_radius      = std::min(w, h) * 0.12f;
    joystick_knob_radius = joystick_radius * 0.45f;
    joystick_anchor      = {kMargin + joystick_radius,
                            h - kMargin - joystick_radius};
    joystick_knob_offset = {0.0f, 0.0f};

    // Button cluster: lower-right. Vertical stack of three big buttons.
    const float btn_x = w - kMargin - kButtonSize;
    const float base_y = h - kMargin - kButtonSize;

    auto& jump = buttons[static_cast<std::size_t>(ButtonId::Jump)];
    jump.bounds = {{btn_x, base_y}, {kButtonSize, kButtonSize}};

    auto& attack = buttons[static_cast<std::size_t>(ButtonId::Attack)];
    attack.bounds = {{btn_x - kButtonSize - kButtonSpacing, base_y},
                     {kButtonSize, kButtonSize}};

    auto& use = buttons[static_cast<std::size_t>(ButtonId::Use)];
    use.bounds = {{btn_x, base_y - kButtonSize - kButtonSpacing},
                  {kButtonSize, kButtonSize}};

    // Inventory sits in the top-right corner, out of the thumb path.
    auto& inv = buttons[static_cast<std::size_t>(ButtonId::Inventory)];
    inv.bounds = {{w - kMargin - kButtonSize, kMargin},
                  {kButtonSize, kButtonSize}};
}

TouchInputOverlay::ButtonId TouchInputOverlay::buttonAt(sf::Vector2i screen_pos) const
{
    const sf::Vector2f p = toVec2f(screen_pos);
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        if (rectContains(buttons[i].bounds, p))
            return static_cast<ButtonId>(i);
    }
    return ButtonId::Count;
}

bool TouchInputOverlay::joystickHits(sf::Vector2i screen_pos) const
{
    const sf::Vector2f d = toVec2f(screen_pos) - joystick_anchor;
    // Allow a generous grab region (1.6x the ring) so the user doesn't have
    // to land dead on the circle.
    const float grab_r = joystick_radius * 1.6f;
    return (d.x * d.x + d.y * d.y) <= (grab_r * grab_r);
}

void TouchInputOverlay::handleEvent(const sf::Event& event, const sf::RenderWindow& window)
{
    if (window.getSize() != last_window_size)
        relayout(window.getSize());

    if (const auto* began = event.getIf<sf::Event::TouchBegan>())
    {
        const auto id    = began->finger;
        const auto pos   = began->position;

        // Button cluster takes priority over the joystick / world tap.
        const ButtonId b = buttonAt(pos);
        if (b != ButtonId::Count)
        {
            auto& btn = buttons[static_cast<std::size_t>(b)];
            if (!btn.finger_id)
            {
                btn.finger_id = id;
                btn.pressed_this_frame = true;
                if (b == ButtonId::Attack)    pending_attack_start = true;
                if (b == ButtonId::Use)       pending_use_start    = true;
                if (b == ButtonId::Inventory) pending_open_inv     = true;
            }
            return;
        }

        if (!joystick_finger && joystickHits(pos))
        {
            joystick_finger = id;
            joystick_knob_offset = toVec2f(pos) - joystick_anchor;
            // Clamp knob to ring.
            const float len2 = joystick_knob_offset.x * joystick_knob_offset.x +
                               joystick_knob_offset.y * joystick_knob_offset.y;
            if (len2 > joystick_radius * joystick_radius)
            {
                const float len = std::sqrt(len2);
                joystick_knob_offset *= (joystick_radius / len);
            }
            return;
        }

        // Fell through: treat as a world tap for aiming.
        last_world_tap_screen = pos;
    }
    else if (const auto* moved = event.getIf<sf::Event::TouchMoved>())
    {
        if (joystick_finger && *joystick_finger == moved->finger)
        {
            joystick_knob_offset = toVec2f(moved->position) - joystick_anchor;
            const float len2 = joystick_knob_offset.x * joystick_knob_offset.x +
                               joystick_knob_offset.y * joystick_knob_offset.y;
            if (len2 > joystick_radius * joystick_radius)
            {
                const float len = std::sqrt(len2);
                joystick_knob_offset *= (joystick_radius / len);
            }
            return;
        }

        // If the move belongs to a finger that originally landed on the
        // world (not a button or the joystick), update the aim position.
        bool is_button_finger = false;
        for (const auto& btn : buttons)
            if (btn.finger_id && *btn.finger_id == moved->finger)
                is_button_finger = true;
        if (!is_button_finger && (!joystick_finger || *joystick_finger != moved->finger))
            last_world_tap_screen = moved->position;
    }
    else if (const auto* ended = event.getIf<sf::Event::TouchEnded>())
    {
        if (joystick_finger && *joystick_finger == ended->finger)
        {
            joystick_finger.reset();
            joystick_knob_offset = {0.0f, 0.0f};
        }
        for (auto& btn : buttons)
        {
            if (btn.finger_id && *btn.finger_id == ended->finger)
                btn.finger_id.reset();
        }
        last_world_tap_screen.reset();
    }
}

std::vector<Input> TouchInputOverlay::collectFrameInputs()
{
    std::vector<Input> out;

    // Horizontal MOVE from joystick tilt. Match the discrete-direction
    // shape of Input.cpp::getInputs so processWorldInputs doesn't need any
    // new code paths.
    if (joystick_finger)
    {
        const float dead_px = joystick_radius * joystick_deadzone_frac;
        if (joystick_knob_offset.x < -dead_px)
            out.push_back({InputType::MOVE, sf::Vector2<double>{-1.0, 0.0}});
        else if (joystick_knob_offset.x > dead_px)
            out.push_back({InputType::MOVE, sf::Vector2<double>{ 1.0, 0.0}});
    }

    // Held JUMP while the Jump button is pressed — same cadence as
    // sf::Keyboard::isKeyPressed(Space) on desktop.
    if (buttons[static_cast<std::size_t>(ButtonId::Jump)].finger_id)
        out.push_back({InputType::JUMP});

    // One-shot press events dispatched by handleEvent on touch-down.
    if (pending_attack_start)
    {
        out.push_back({InputType::ATTACK_START});
        pending_attack_start = false;
    }
    if (pending_use_start)
    {
        out.push_back({InputType::USE_START});
        pending_use_start = false;
    }
    if (pending_open_inv)
    {
        out.push_back({InputType::OPEN_INVENTORY});
        pending_open_inv = false;
    }

    // Edge-triggered STOP events when the finger lifts off the button.
    const bool attack_held_now =
        buttons[static_cast<std::size_t>(ButtonId::Attack)].finger_id.has_value();
    if (attack_held_prev && !attack_held_now)
        out.push_back({InputType::ATTACK_STOP});
    attack_held_prev = attack_held_now;

    const bool use_held_now =
        buttons[static_cast<std::size_t>(ButtonId::Use)].finger_id.has_value();
    if (use_held_prev && !use_held_now)
        out.push_back({InputType::USE_STOP});
    use_held_prev = use_held_now;

    return out;
}

sf::Vector2<double> TouchInputOverlay::aimWorldPos(const sf::RenderWindow& window,
                                                   sf::Vector2<double> camera) const
{
    // The game view has its origin at camera center (ClientGameState sets
    // this each frame). mapPixelToCoords therefore returns world coords.
    sf::Vector2i target = last_world_tap_screen.value_or(
        sf::Vector2i(static_cast<int>(window.getSize().x) / 2,
                     static_cast<int>(window.getSize().y) / 2));

    // Nudge the default aim slightly below screen center so an empty-tap
    // attack lands on the block the player is standing on, not the sky
    // behind them.
    if (!last_world_tap_screen)
        target.y += static_cast<int>(window.getSize().y) * 0.1f;

    const sf::Vector2f world = window.mapPixelToCoords(target);
    (void)camera;
    return {static_cast<double>(world.x), static_cast<double>(world.y)};
}

void TouchInputOverlay::render(sf::RenderWindow& window)
{
    if (window.getSize() != last_window_size)
        relayout(window.getSize());

    // Overlay draws in screen (pixel) space. getDefaultView() returns the
    // view that existed at window creation and doesn't track resizes, so
    // build a fresh identity view for the current size.
    const auto previous_view = window.getView();
    const sf::Vector2f sz(window.getSize());
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, sz)));

    const sf::Color translucent_fill(255, 255, 255, 60);
    const sf::Color border(255, 255, 255, 160);
    const sf::Color highlight(255, 255, 255, 200);

    // Joystick base ring.
    sf::CircleShape ring(joystick_radius);
    ring.setOrigin({joystick_radius, joystick_radius});
    ring.setPosition(joystick_anchor);
    ring.setFillColor(translucent_fill);
    ring.setOutlineColor(border);
    ring.setOutlineThickness(3.0f);
    window.draw(ring);

    // Joystick knob.
    sf::CircleShape knob(joystick_knob_radius);
    knob.setOrigin({joystick_knob_radius, joystick_knob_radius});
    knob.setPosition(joystick_anchor + joystick_knob_offset);
    knob.setFillColor(joystick_finger ? highlight : border);
    window.draw(knob);

    // Buttons.
    const std::array<const char*, static_cast<std::size_t>(ButtonId::Count)> labels{
        "JUMP", "ATK", "USE", "INV"
    };
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        const auto& btn = buttons[i];
        sf::RectangleShape rect(btn.bounds.size);
        rect.setPosition(btn.bounds.position);
        rect.setFillColor(btn.finger_id ? highlight : translucent_fill);
        rect.setOutlineColor(border);
        rect.setOutlineThickness(3.0f);
        window.draw(rect);

        // Simple label — ImGui default font would need an atlas update per
        // frame, so draw plain text when we have a font. If the font map
        // is empty (no AssetManager handy here) we just skip the label.
        (void)labels[i];
    }

    window.setView(previous_view);
}
