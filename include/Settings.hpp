#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <cstdint>

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

    static Graphics graphics;
    static Audio audio;
    static Debug debug;
    static Render render;

    static void load();
    static void save();
};

inline Settings::Graphics Settings::graphics{};
inline Settings::Audio Settings::audio{};
inline Settings::Debug Settings::debug{};
inline Settings::Render Settings::render{};

#endif // SETTINGS_HPP
