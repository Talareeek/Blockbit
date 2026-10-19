#include "../include/Settings.hpp"

#include "../include/GameCommon.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
    std::filesystem::path settingsFilePath()
    {
        return getHomePath() / "Blockbit" / "settings.json";
    }
}

void Settings::load()
{
    std::filesystem::path path = settingsFilePath();

    if(!std::filesystem::exists(path))
    {
        save();
        return;
    }

    try
    {
        std::ifstream file(path);
        nlohmann::json data = nlohmann::json::parse(file, nullptr, true, true);

        if(data.contains("graphics"))
        {
            const auto& g = data["graphics"];
            graphics.fullscreen    = g.value("fullscreen",    graphics.fullscreen);
            graphics.vsync         = g.value("vsync",         graphics.vsync);
            graphics.window_width  = g.value("window_width",  graphics.window_width);
            graphics.window_height = g.value("window_height", graphics.window_height);
            graphics.fps_limit     = g.value("fps_limit",     graphics.fps_limit);
        }

        if(data.contains("audio"))
        {
            const auto& a = data["audio"];
            audio.master_volume = a.value("master_volume", audio.master_volume);
            audio.music_volume  = a.value("music_volume",  audio.music_volume);
            audio.sound_volume  = a.value("sound_volume",  audio.sound_volume);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "[Settings] Failed to load " << path << ": " << e.what() << '\n';
    }
}

void Settings::save()
{
    std::filesystem::path path = settingsFilePath();

    try
    {
        std::filesystem::create_directories(path.parent_path());

        nlohmann::json data;
        data["graphics"] = {
            {"fullscreen",    graphics.fullscreen},
            {"vsync",         graphics.vsync},
            {"window_width",  graphics.window_width},
            {"window_height", graphics.window_height},
            {"fps_limit",     graphics.fps_limit}
        };
        data["audio"] = {
            {"master_volume", audio.master_volume},
            {"music_volume",  audio.music_volume},
            {"sound_volume",  audio.sound_volume}
        };

        std::ofstream file(path);
        file << data.dump(4);
    }
    catch(const std::exception& e)
    {
        std::cerr << "[Settings] Failed to save " << path << ": " << e.what() << '\n';
    }
}
