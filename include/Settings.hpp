#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <cstdint>

#include <SFML/Graphics/Color.hpp>

class Settings
{
public:

    struct Graphics
    {
        bool fullscreen = false;
        bool vsync = true;
        uint32_t window_width = 1280;
        uint32_t window_height = 720;
        uint32_t fps_limit = 100;
#ifdef __ANDROID__
        bool lighting_effect = false;
        bool block_animations = false;
#else
        bool lighting_effect = true;
        bool block_animations = true;
#endif        
        float lighting_effect_strength = 0.75f;        
    };

    struct Audio
    {
        float master_volume = 100.0f;
        float music_volume = 100.0f;
        float sound_volume = 100.0f;
    };

    struct Debug
    {
        bool render_hitboxes = false;
        bool render_chunk_borders = false;
    };

    struct Render
    {
        bool reach_zoom_enabled = true;
        float reach_zoom_scale = 0.15f;
        sf::Color selector_color{0, 0, 0};
        float selector_air_fade = 0.65f; // 0 = outline fully visible on air, 1 = fully invisible
        bool show_player_names = true;
    };

    struct UI
    {
        sf::Color main_color{255, 190, 60};
    };

    struct Minimap
    {
        bool enabled = true;
        bool render_shadows = true;
        bool flat_colors = false; // render blocks as their average color instead of textures
        float size = 0.1f;        // fraction of window width
        float offset_x = 50.0f;   // pixels from the left edge
        float offset_y = 50.0f;   // pixels from the top edge
    };

    struct Pointer
    {
        enum class Shape : std::uint8_t
        {
            Arrow     = 0,
            Crosshair = 1,
            Dot       = 2,
            Ring      = 3,
            Diamond   = 4
        };

        enum class Mode : std::uint8_t
        {
            Shape = 0,
            Item  = 1
        };

        bool enabled = true;
        Mode mode = Mode::Shape;
        Shape shape = Shape::Arrow;
        sf::Color color{255, 190, 60};
        uint32_t size = 32;   // pixel edge of the cursor image

        // Item id (underlying type of ItemID) used when mode == Item. Stored
        // as uint32_t so this header does not depend on Item.hpp. 0 is
        // ItemID::None and is treated as "no selection".
        std::uint32_t item_id = 0;
    };

    struct Gamepad
    {
        // Mapping identifiers. Each entry is an action the player can trigger
        // through a gamepad button. Keep the ordering stable — save files
        // address slots by index.
        enum class Action : std::uint8_t
        {
            Jump = 0,
            Attack,
            Use,
            Drop,
            DropFull,
            SlotPrev,
            SlotNext,
            OpenInventory,
            Count
        };

        // -1 means "unbound". Default layout targets a standard Xbox-style
        // pad: A=Jump, Y=Attack, X=Use, B=Drop (hold LShift-equivalent for
        // full stack via a second button), LB/RB cycle the hotbar, Start
        // opens the inventory.
        static constexpr int UNBOUND = -1;

        bool enabled = true;
        std::uint32_t joystick_id = 0;

        // SFML axis positions are already in [-100, 100] — deadzones are in
        // the same scale.
        float stick_deadzone = 15.0f;

        // Cursor movement: world units per second at full right-stick
        // deflection.
        float cursor_speed = 10.0f;

        // Maximum world-space distance from the player that the gamepad
        // cursor may roam. Prevents it from flying off into fog.
        float cursor_max_distance = 6.0f;

        // Render a cursor sprite at the gamepad cursor position.
        bool show_cursor = true;
        std::uint32_t cursor_size = 32;
        sf::Color cursor_color{255, 190, 60};

        int buttons[static_cast<std::size_t>(Action::Count)] = {
            0,  // Jump          — A
            3,  // Attack        — Y
            2,  // Use           — X
            1,  // Drop          — B
            UNBOUND,  // DropFull
            4,  // SlotPrev      — LB
            5,  // SlotNext      — RB
            7   // OpenInventory — Start
        };
    };

    static Graphics graphics;
    static Audio audio;
    static Debug debug;
    static Render render;
    static UI ui;
    static Minimap minimap;
    static Pointer pointer;
    static Gamepad gamepad;

    static void load();
    static void save();
};

inline Settings::Graphics Settings::graphics{};
inline Settings::Audio Settings::audio{};
inline Settings::Debug Settings::debug{};
inline Settings::Render Settings::render{};
inline Settings::UI Settings::ui{};
inline Settings::Minimap Settings::minimap{};
inline Settings::Pointer Settings::pointer{};
inline Settings::Gamepad Settings::gamepad{};

#endif // SETTINGS_HPP
