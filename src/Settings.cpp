#include "../include/Settings.hpp"

#include "../include/GameCommon.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
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
            graphics.lighting_effect_strength = g.value("lighting_effect_strength", graphics.lighting_effect_strength);
        }

        if(data.contains("audio"))
        {
            const auto& a = data["audio"];
            audio.master_volume = a.value("master_volume", audio.master_volume);
            audio.music_volume  = a.value("music_volume",  audio.music_volume);
            audio.sound_volume  = a.value("sound_volume",  audio.sound_volume);
        }

        if(data.contains("debug"))
        {
            const auto& d = data["debug"];
            debug.render_hitboxes      = d.value("render_hitboxes",      debug.render_hitboxes);
            debug.render_chunk_borders = d.value("render_chunk_borders", debug.render_chunk_borders);
        }

        if(data.contains("render"))
        {
            const auto& r = data["render"];
            render.reach_zoom_enabled = r.value("reach_zoom_enabled", render.reach_zoom_enabled);
            render.reach_zoom_scale   = r.value("reach_zoom_scale",   render.reach_zoom_scale);
        }

        if(data.contains("ui"))
        {
            const auto& u = data["ui"];
            if(u.contains("main_color") && u["main_color"].is_array() && u["main_color"].size() >= 3)
            {
                const auto& c = u["main_color"];
                ui.main_color.r = static_cast<std::uint8_t>(std::clamp<int>(c[0].get<int>(), 0, 255));
                ui.main_color.g = static_cast<std::uint8_t>(std::clamp<int>(c[1].get<int>(), 0, 255));
                ui.main_color.b = static_cast<std::uint8_t>(std::clamp<int>(c[2].get<int>(), 0, 255));
            }
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
            {"fps_limit",     graphics.fps_limit},
            {"lighting_effect_strength", graphics.lighting_effect_strength}
        };
        data["audio"] = {
            {"master_volume", audio.master_volume},
            {"music_volume",  audio.music_volume},
            {"sound_volume",  audio.sound_volume}
        };
        data["debug"] = {
            {"render_hitboxes",      debug.render_hitboxes},
            {"render_chunk_borders", debug.render_chunk_borders}
        };
        data["render"] = {
            {"reach_zoom_enabled", render.reach_zoom_enabled},
            {"reach_zoom_scale",   render.reach_zoom_scale}
        };
        data["ui"] = {
            {"main_color", { static_cast<int>(ui.main_color.r),
                             static_cast<int>(ui.main_color.g),
                             static_cast<int>(ui.main_color.b) }}
        };

        std::ofstream file(path);
        file << data.dump(4);
    }
    catch(const std::exception& e)
    {
        std::cerr << "[Settings] Failed to save " << path << ": " << e.what() << '\n';
    }
}
