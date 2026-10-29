#ifndef TOUCH_INPUT_OVERLAY_HPP
#define TOUCH_INPUT_OVERLAY_HPP

#include "Input.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
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
        Attack,
        Use,
        Inventory,

        Count
    };

    void handleEvent(const sf::Event& event, const sf::RenderWindow& window);

    // Produces per-frame inputs (MOVE while joystick is tilted past the
    // deadzone, JUMP while that button is held). One-shot ATTACK_START /
    // USE_START / OPEN_INVENTORY are emitted by handleEvent as touches land.
    std::vector<Input> collectFrameInputs();

    // Current aim target. When the player drags somewhere on the "world
    // area" of the screen we aim at that world point; otherwise aim stays
    // slightly below the camera center so single-tap mining lands on the
    // block the player is standing on.
    sf::Vector2<double> aimWorldPos(const sf::RenderWindow& window,
                                    sf::Vector2<double> camera) const;

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

    // World-area tap state. Last tapped position on the game surface, used
    // as the current aim. Cleared on finger-up so aim snaps back to default.
    std::optional<sf::Vector2i> last_world_tap_screen{};

    // Latching flags for one-shot events emitted by handleEvent and
    // consumed via collectFrameInputs() — kept so the Input stream still
    // flows through one place.
    bool pending_attack_start = false;
    bool pending_use_start    = false;
    bool pending_open_inv     = false;

    // Attack-held state: set by attack button touch-down / cleared by up.
    // collectFrameInputs emits ATTACK_STOP once when it flips to false.
    bool attack_held_prev = false;
    bool use_held_prev    = false;

    void relayout(sf::Vector2u window_size);

    // Returns the button whose bounds contain the given screen point, or
    // Count if none.
    ButtonId buttonAt(sf::Vector2i screen_pos) const;

    bool joystickHits(sf::Vector2i screen_pos) const;
};

#endif // TOUCH_INPUT_OVERLAY_HPP
