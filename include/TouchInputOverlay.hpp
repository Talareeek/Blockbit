#ifndef TOUCH_INPUT_OVERLAY_HPP
#define TOUCH_INPUT_OVERLAY_HPP

#include "Input.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

// On-screen touch controls for the Android build. Owns a persistent virtual
// joystick (left thumb) and a small cluster of action buttons (right thumb).
// Writes into the same Input stream the server already consumes — the
// protocol stays unchanged.
//
// On desktop the class exists but handleEvent is a no-op (no touch events
// arrive) and render() draws nothing, so the overlay is invisible.
class TouchInputOverlay
{
public:
    enum class ButtonId : std::uint8_t
    {
        Jump = 0,
        Inventory,

        Count
    };

    // Any touch that doesn't land on the joystick, Jump, or Inventory is a
    // "world touch". Short world touches become USE_START + USE_STOP (place
    // / interact); long ones become ATTACK_START on hold-threshold crossing
    // and ATTACK_STOP on release (mine / hit).
    static constexpr float TAP_HOLD_THRESHOLD_SECONDS = 0.15f;

    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);

    // Produces per-frame inputs (MOVE while joystick is tilted past the
    // deadzone, JUMP while that button is held, and the delayed ATTACK_START
    // once a world touch crosses the hold threshold). Instant events
    // (USE_START / USE_STOP on tap release, ATTACK_STOP on hold release,
    // OPEN_INVENTORY) are queued by handleEvent and drained here.
    std::vector<Input> collectFrameInputs();

    // Current touch position in world coordinates, if a world touch is
    // active. Empty when no finger is down in the play area — the caller
    // should fall back to the tracked entity's position in that case.
    // Matches the camera + unit_size transform in getMouseWorldPosition so
    // the server receives the same coord space it does for desktop clicks.
    std::optional<sf::Vector2<double>> currentTouchWorldPos(
        const sf::RenderWindow& window, sf::Vector2<double> camera) const;

    void render(sf::RenderWindow& window);

private:
    struct Button
    {
        sf::FloatRect bounds{};
        std::optional<unsigned int> finger_id{};
        bool pressed_this_frame = false;
    };

    sf::Vector2u last_window_size{};

    // Joystick anchor + current knob offset (from anchor, in pixels).
    sf::Vector2f joystick_anchor{};
    sf::Vector2f joystick_knob_offset{};
    float joystick_radius = 180.0f;
    float joystick_knob_radius = 80.0f;
    float joystick_deadzone_frac = 0.3f;
    std::optional<unsigned int> joystick_finger{};

    std::array<Button, static_cast<std::size_t>(ButtonId::Count)> buttons;

    // Active world-touch finger. Set on TouchBegan (outside buttons and the
    // joystick grab region) and cleared on TouchEnded.
    std::optional<unsigned int> world_finger{};
    sf::Vector2i world_touch_screen{};   // current finger position
    sf::Clock world_touch_timer{};       // elapsed since TouchBegan
    bool world_touch_latched_attack = false;  // ATTACK_START already emitted

    // One-shot inputs queued by handleEvent and collectFrameInputs; drained
    // by the next collectFrameInputs() call so everything funnels through a
    // single place.
    std::vector<Input> pending_inputs;

    // Inputs deferred to the call AFTER the next collectFrameInputs. Used to
    // split USE_START and USE_STOP across two ticks so PlayerControlledSystem
    // sees mid_usage=true for one tick before USE_STOP flips it back off.
    std::vector<Input> next_frame_inputs;

    void relayout(sf::Vector2u window_size);

    // Returns the button whose bounds contain the given screen point, or
    // Count if none.
    ButtonId buttonAt(sf::Vector2i screen_pos) const;

    bool joystickHits(sf::Vector2i screen_pos) const;
};

#endif // TOUCH_INPUT_OVERLAY_HPP
