#include "../include/TouchInputOverlay.hpp"
#include "../include/GameCommon.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kButtonSize    = 180.0f;
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

    // Jump: lower-right thumb reach. Attack/Use are no longer buttons — the
    // whole play area is tap=use / hold=attack.
    auto& jump = buttons[static_cast<std::size_t>(ButtonId::Jump)];
    jump.bounds = {{w - kMargin - kButtonSize, h - kMargin - kButtonSize},
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
                if (b == ButtonId::Inventory)
                    pending_inputs.push_back({InputType::OPEN_INVENTORY});
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

        // Fell through: this finger owns the world-touch slot. One active
        // world finger at a time — a second finger landing in the play area
        // is ignored so the attack/use decision stays unambiguous.
        if (!world_finger)
        {
            world_finger = id;
            world_touch_screen = pos;
            world_touch_timer.restart();
            world_touch_latched_attack = false;
        }
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

        if (world_finger && *world_finger == moved->finger)
            world_touch_screen = moved->position;
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

        if (world_finger && *world_finger == ended->finger)
        {
            const float elapsed = world_touch_timer.getElapsedTime().asSeconds();
            if (world_touch_latched_attack)
            {
                // Hold already crossed the threshold — close the attack.
                pending_inputs.push_back({InputType::ATTACK_STOP});
            }
            else if (elapsed < TAP_HOLD_THRESHOLD_SECONDS)
            {
                // Short tap: fire a USE pulse. USE_STOP is deferred one tick
                // so PlayerControlledSystem sees mid_usage=true for a tick
                // before processWorldInputs flips it back — otherwise both
                // flags land in the same InputPacket and the placement is
                // silently cancelled.
                pending_inputs.push_back({InputType::USE_START});
                next_frame_inputs.push_back({InputType::USE_STOP});
            }
            else
            {
                // Edge case: finger lifted on the exact frame the threshold
                // was reached but before collectFrameInputs latched it.
                // Emit a full attack pulse so the server still sees it.
                pending_inputs.push_back({InputType::ATTACK_START});
                pending_inputs.push_back({InputType::ATTACK_STOP});
            }
            world_finger.reset();
            world_touch_latched_attack = false;
        }
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

    // Delayed ATTACK_START: a world touch that stays down past the
    // tap/hold threshold converts into a held attack. The matching
    // ATTACK_STOP (or the USE pulse for a tap) is queued by handleEvent
    // when the finger lifts.
    if (world_finger && !world_touch_latched_attack &&
        world_touch_timer.getElapsedTime().asSeconds() >= TAP_HOLD_THRESHOLD_SECONDS)
    {
        out.push_back({InputType::ATTACK_START});
        world_touch_latched_attack = true;
    }

    if (!pending_inputs.empty())
    {
        out.insert(out.end(),
                   std::make_move_iterator(pending_inputs.begin()),
                   std::make_move_iterator(pending_inputs.end()));
        pending_inputs.clear();
    }

    // Promote deferred inputs so the next call drains them.
    pending_inputs = std::move(next_frame_inputs);
    next_frame_inputs.clear();

    return out;
}

std::optional<sf::Vector2<double>> TouchInputOverlay::currentTouchWorldPos(
    const sf::RenderWindow& window, sf::Vector2<double> camera) const
{
    if (!world_finger) return std::nullopt;

    // Mirror getMouseWorldPosition (src/Render.cpp): mapPixelToCoords
    // through the current view gives scaled-pixel coords, which divide by
    // unit_size to get world units offset from the camera center.
    const double unit_size = static_cast<double>(window.getSize().y) /
                             static_cast<double>(WORLD_UNIT_SIZE_FACTOR);
    if (unit_size <= 0.0) return std::nullopt;

    const sf::Vector2f view_coords = window.mapPixelToCoords(world_touch_screen);
    return camera + sf::Vector2<double>{
        static_cast<double>(view_coords.x) / unit_size,
        static_cast<double>(view_coords.y) / unit_size};
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
        "JUMP", "INV"
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
