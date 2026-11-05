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
            if(r.contains("selector_color") && r["selector_color"].is_array() && r["selector_color"].size() >= 3)
            {
                const auto& c = r["selector_color"];
                render.selector_color.r = static_cast<std::uint8_t>(std::clamp<int>(c[0].get<int>(), 0, 255));
                render.selector_color.g = static_cast<std::uint8_t>(std::clamp<int>(c[1].get<int>(), 0, 255));
                render.selector_color.b = static_cast<std::uint8_t>(std::clamp<int>(c[2].get<int>(), 0, 255));
            }
            render.selector_air_fade = std::clamp(r.value("selector_air_fade", render.selector_air_fade), 0.0f, 1.0f);
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

        if(data.contains("minimap"))
        {
            const auto& m = data["minimap"];
            minimap.enabled        = m.value("enabled",        minimap.enabled);
            minimap.render_shadows = m.value("render_shadows", minimap.render_shadows);
            minimap.size           = m.value("size",           minimap.size);
            minimap.offset_x       = m.value("offset_x",       minimap.offset_x);
            minimap.offset_y       = m.value("offset_y",       minimap.offset_y);
        }

        if(data.contains("pointer"))
        {
            const auto& p = data["pointer"];
            pointer.enabled = p.value("enabled", pointer.enabled);

            int mode_int = p.value("mode", static_cast<int>(pointer.mode));
            mode_int = std::clamp(mode_int, 0, static_cast<int>(Pointer::Mode::Item));
            pointer.mode = static_cast<Pointer::Mode>(mode_int);

            int shape_int = p.value("shape", static_cast<int>(pointer.shape));
            shape_int = std::clamp(shape_int, 0, static_cast<int>(Pointer::Shape::Diamond));
            pointer.shape = static_cast<Pointer::Shape>(shape_int);

            if(p.contains("color") && p["color"].is_array() && p["color"].size() >= 3)
            {
                const auto& c = p["color"];
                pointer.color.r = static_cast<std::uint8_t>(std::clamp<int>(c[0].get<int>(), 0, 255));
                pointer.color.g = static_cast<std::uint8_t>(std::clamp<int>(c[1].get<int>(), 0, 255));
                pointer.color.b = static_cast<std::uint8_t>(std::clamp<int>(c[2].get<int>(), 0, 255));
            }

            int sz = p.value("size", static_cast<int>(pointer.size));
            pointer.size = static_cast<uint32_t>(std::clamp(sz, 8, 128));

            pointer.item_id = p.value("item_id", pointer.item_id);
        }

        if(data.contains("gamepad"))
        {
            const auto& g = data["gamepad"];
            gamepad.enabled            = g.value("enabled",            gamepad.enabled);
            gamepad.joystick_id        = g.value("joystick_id",        gamepad.joystick_id);
            gamepad.stick_deadzone     = std::clamp(g.value("stick_deadzone", gamepad.stick_deadzone), 0.0f, 90.0f);
            gamepad.cursor_speed       = std::max(0.0f, g.value("cursor_speed",       gamepad.cursor_speed));
            gamepad.cursor_max_distance= std::max(0.5f, g.value("cursor_max_distance",gamepad.cursor_max_distance));
            gamepad.show_cursor        = g.value("show_cursor",        gamepad.show_cursor);
            gamepad.cursor_size        = static_cast<std::uint32_t>(std::clamp(g.value("cursor_size", static_cast<int>(gamepad.cursor_size)), 8, 128));

            if(g.contains("cursor_color") && g["cursor_color"].is_array() && g["cursor_color"].size() >= 3)
            {
                const auto& c = g["cursor_color"];
                gamepad.cursor_color.r = static_cast<std::uint8_t>(std::clamp<int>(c[0].get<int>(), 0, 255));
                gamepad.cursor_color.g = static_cast<std::uint8_t>(std::clamp<int>(c[1].get<int>(), 0, 255));
                gamepad.cursor_color.b = static_cast<std::uint8_t>(std::clamp<int>(c[2].get<int>(), 0, 255));
            }

            if(g.contains("buttons") && g["buttons"].is_array())
            {
                const auto& arr = g["buttons"];
                const std::size_t count = std::min(arr.size(), static_cast<std::size_t>(Gamepad::Action::Count));
                for(std::size_t i = 0; i < count; ++i)
                {
                    int v = arr[i].get<int>();
                    if(v < -1 || v > 31) v = Gamepad::UNBOUND;
                    gamepad.buttons[i] = v;
                }
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
            {"reach_zoom_scale",   render.reach_zoom_scale},
            {"selector_color", { static_cast<int>(render.selector_color.r),
                                 static_cast<int>(render.selector_color.g),
                                 static_cast<int>(render.selector_color.b) }},
            {"selector_air_fade", render.selector_air_fade}
        };
        data["ui"] = {
            {"main_color", { static_cast<int>(ui.main_color.r),
                             static_cast<int>(ui.main_color.g),
                             static_cast<int>(ui.main_color.b) }}
        };
        data["minimap"] = {
            {"enabled",        minimap.enabled},
            {"render_shadows", minimap.render_shadows},
            {"size",           minimap.size},
            {"offset_x",       minimap.offset_x},
            {"offset_y",       minimap.offset_y}
        };
        data["pointer"] = {
            {"enabled", pointer.enabled},
            {"mode",    static_cast<int>(pointer.mode)},
            {"shape",   static_cast<int>(pointer.shape)},
            {"color",   { static_cast<int>(pointer.color.r),
                          static_cast<int>(pointer.color.g),
                          static_cast<int>(pointer.color.b) }},
            {"size",    static_cast<int>(pointer.size)},
            {"item_id", pointer.item_id}
        };

        nlohmann::json buttons_json = nlohmann::json::array();
        for(std::size_t i = 0; i < static_cast<std::size_t>(Gamepad::Action::Count); ++i)
        {
            buttons_json.push_back(gamepad.buttons[i]);
        }

        data["gamepad"] = {
            {"enabled",             gamepad.enabled},
            {"joystick_id",         gamepad.joystick_id},
            {"stick_deadzone",      gamepad.stick_deadzone},
            {"cursor_speed",        gamepad.cursor_speed},
            {"cursor_max_distance", gamepad.cursor_max_distance},
            {"show_cursor",         gamepad.show_cursor},
            {"cursor_size",         static_cast<int>(gamepad.cursor_size)},
            {"cursor_color",        { static_cast<int>(gamepad.cursor_color.r),
                                      static_cast<int>(gamepad.cursor_color.g),
                                      static_cast<int>(gamepad.cursor_color.b) }},
            {"buttons",             buttons_json}
        };

        std::ofstream file(path);
        file << data.dump(4);
    }
    catch(const std::exception& e)
    {
        std::cerr << "[Settings] Failed to save " << path << ": " << e.what() << '\n';
    }
}
