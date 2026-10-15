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
    };

    struct Audio
    {
        float master_volume = 100.0f;
        float music_volume = 100.0f;
        float sound_volume = 100.0f;
    };

    static Graphics graphics;
    static Audio audio;

    static void load();
    static void save();
};

inline Settings::Graphics Settings::graphics{};
inline Settings::Audio Settings::audio{};

#endif // SETTINGS_HPP
