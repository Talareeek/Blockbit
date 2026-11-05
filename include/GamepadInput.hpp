#ifndef GAMEPAD_INPUT_HPP
#define GAMEPAD_INPUT_HPP

#include "Input.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <bitset>
#include <cstdint>
#include <optional>
#include <vector>

// Gamepad controller on top of SFML's sf::Joystick. Produces the same Input
// stream the rest of the pipeline consumes:
//   - Left stick      → MOVE (held each frame while past the deadzone)
//   - Right stick     → steers a floating world-space cursor (the server's
//                        PlayerControlledComponent::cursor_position target)
//   - Buttons         → edge-detected JUMP, ATTACK_START/STOP, USE_START/STOP,
//                        DROP, CHANGE_SLOT, OPEN_INVENTORY — layout comes from
//                        Settings::gamepad.buttons[Action].
//   - D-Pad (PovX)    → CHANGE_SLOT previous / next (one-shot).
//
// The cursor is clamped to a world-space radius around the player so it never
// wanders outside interaction range, and its texture is rendered on top of
// the HUD when the gamepad is the active input source.
class GamepadInput
{
public:
    GamepadInput();

    // Per-frame polling. Updates the floating cursor offset from the right
    // stick and resolves "is the gamepad the active input source" based on
    // whether the sticks moved past the deadzone this frame or whether the
    // mouse was moved since the last tick.
    void update(float dt, const sf::RenderWindow& window);

    // Called when a tick fires. Returns the Input list to merge into the
    // outgoing InputPacket. Consumes edge state (so a single press produces
    // exactly one START/STOP pair across the tick boundary).
    std::vector<Input> collectTickInputs();

    // Where the gamepad cursor sits in world coordinates. Derived from the
    // tracked player's center position + cursor_offset. Falls back to the
    // camera if the player isn't available.
    sf::Vector2<double> cursorWorldPosition(sf::Vector2<double> camera,
                                            std::optional<sf::Vector2<double>> player_center) const;

    // True while the user is actively driving aim with the right stick. The
    // ClientGameState uses this to decide whether to send gamepad cursor
    // coords (instead of the mouse) in the ClientSnapshotPacket, and to draw
    // the cursor sprite.
    bool isCursorActive() const { return cursor_active; }

    // Rebind helper: polls every button on the selected joystick and returns
    // the first one pressed this frame, or -1 if nothing is held. Used by
    // the Settings window rebinder.
    int pollAnyPressedButton() const;

    void render(sf::RenderWindow& window,
                sf::Vector2<double> camera,
                std::optional<sf::Vector2<double>> player_center);

    // Returns true when SFML reports the configured joystick is connected
    // AND the setting is enabled. Settings window / debug can display this.
    bool isConnected() const;

    // Clears any held-down state. Call when leaving gameplay so a button
    // that was pressed when the pause menu opened doesn't fire a stale
    // STOP event on return.
    void reset();

private:
    // Right-stick-driven cursor offset, in world units, from the player
    // (or camera) origin.
    sf::Vector2<double> cursor_offset{0.0, 0.0};

    // Previous button-pressed snapshot — buttons[i] ↔ bit i. Up to 32
    // buttons, matching sf::Joystick::ButtonCount.
    std::bitset<32> prev_buttons;

    // Edge-tracked D-Pad step state. -1/0/+1, prev frame's value.
    int prev_pov_x_step = 0;

    // Previous mouse pixel, used to detect mouse movement that should
    // flip the active input source back to mouse.
    sf::Vector2i prev_mouse_pixel{};
    bool prev_mouse_sampled = false;

    bool cursor_active = false;

    // Render-cache for the cursor sprite. Rebuilt when any of the inputs
    // that shape it (size, color) change.
    sf::Texture cursor_texture;
    std::uint32_t cached_size = 0;
    sf::Color cached_color{0, 0, 0, 0};
    bool texture_ready = false;

    void rebuildCursorTextureIfNeeded();
};

#endif // GAMEPAD_INPUT_HPP
