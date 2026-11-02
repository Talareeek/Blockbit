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
    };

    struct UI
    {
        sf::Color main_color{255, 190, 60};
    };

    struct Minimap
    {
        bool enabled = true;
        bool render_shadows = true;
        float size = 0.1f;        // fraction of window width
        float offset_x = 50.0f;   // pixels from the left edge
        float offset_y = 50.0f;   // pixels from the top edge
    };

    static Graphics graphics;
    static Audio audio;
    static Debug debug;
    static Render render;
    static UI ui;
    static Minimap minimap;

    static void load();
    static void save();
};

inline Settings::Graphics Settings::graphics{};
inline Settings::Audio Settings::audio{};
inline Settings::Debug Settings::debug{};
inline Settings::Render Settings::render{};
inline Settings::UI Settings::ui{};
inline Settings::Minimap Settings::minimap{};

#endif // SETTINGS_HPP
