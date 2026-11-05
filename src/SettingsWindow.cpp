#include "../include/SettingsWindow.hpp"

#include "../include/Settings.hpp"
#include "../include/Game.hpp"
#include "../include/Item.hpp"

#include <SFML/Window/Joystick.hpp>

#include <algorithm>
#include <vector>

#include <imgui.h>

void SettingsWindow::assignGame(Game* game)
{
    this->game = game;
}

void SettingsWindow::show()
{
    open = true;
}

void SettingsWindow::hide()
{
    open = false;
}

void SettingsWindow::toggle()
{
    open = !open;
}

bool SettingsWindow::isOpen() const
{
    return open;
}

bool SettingsWindow::consumeGraphicsRequest()
{
    bool requested = apply_graphics_requested;
    apply_graphics_requested = false;
    return requested;
}

bool SettingsWindow::consumeAudioRequest()
{
    bool requested = apply_audio_requested;
    apply_audio_requested = false;
    return requested;
}

bool SettingsWindow::consumePointerRequest()
{
    bool requested = apply_pointer_requested;
    apply_pointer_requested = false;
    return requested;
}

void SettingsWindow::render()
{
    if(!open) return;

    ImGui::SetNextWindowSize(ImVec2(420.0f, 360.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Settings", &open, ImGuiWindowFlags_NoCollapse);

    if(ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Fullscreen", &Settings::graphics.fullscreen);
        ImGui::Checkbox("VSync", &Settings::graphics.vsync);

        int window_width  = static_cast<int>(Settings::graphics.window_width);
        int window_height = static_cast<int>(Settings::graphics.window_height);
        int fps_limit     = static_cast<int>(Settings::graphics.fps_limit);

        if(ImGui::InputInt("Window width", &window_width, 10, 100))
        {
            if(window_width < 320) window_width = 320;
            Settings::graphics.window_width = static_cast<uint32_t>(window_width);
        }
        if(ImGui::InputInt("Window height", &window_height, 10, 100))
        {
            if(window_height < 240) window_height = 240;
            Settings::graphics.window_height = static_cast<uint32_t>(window_height);
        }
        if(ImGui::SliderInt("FPS limit", &fps_limit, 0, 240))
        {
            Settings::graphics.fps_limit = static_cast<uint32_t>(fps_limit);
        }

        ImGui::Checkbox("Lighting effect", &Settings::graphics.lighting_effect);

        ImGui::SliderFloat("Lighting effect strength", &Settings::graphics.lighting_effect_strength, 0.0f, 1.0f, "%.2f");

        ImGui::Checkbox("Block animations", &Settings::graphics.block_animations);

        if(ImGui::Button("Apply graphics"))
        {
            apply_graphics_requested = true;
        }
    }

    if(ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool audio_changed = false;
        audio_changed |= ImGui::SliderFloat("Master",       &Settings::audio.master_volume, 0.0f, 100.0f, "%.0f");
        audio_changed |= ImGui::SliderFloat("Music",        &Settings::audio.music_volume,  0.0f, 100.0f, "%.0f");
        audio_changed |= ImGui::SliderFloat("Sound effects",&Settings::audio.sound_volume,  0.0f, 100.0f, "%.0f");

        if(audio_changed) apply_audio_requested = true;
    }

    if(ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Reach-zoom", &Settings::render.reach_zoom_enabled);
        ImGui::SliderFloat("Reach-zoom scale", &Settings::render.reach_zoom_scale, 0.0f, 4.0f, "%.2f");

        float selector_color[3] = {
            Settings::render.selector_color.r / 255.0f,
            Settings::render.selector_color.g / 255.0f,
            Settings::render.selector_color.b / 255.0f
        };
        if(ImGui::ColorEdit3("Selector color", selector_color))
        {
            Settings::render.selector_color.r = static_cast<std::uint8_t>(selector_color[0] * 255.0f);
            Settings::render.selector_color.g = static_cast<std::uint8_t>(selector_color[1] * 255.0f);
            Settings::render.selector_color.b = static_cast<std::uint8_t>(selector_color[2] * 255.0f);
        }

        float air_fade_percent = Settings::render.selector_air_fade * 100.0f;
        if(ImGui::SliderFloat("Selector air fade", &air_fade_percent, 0.0f, 100.0f, "%.0f%%"))
        {
            Settings::render.selector_air_fade = std::clamp(air_fade_percent / 100.0f, 0.0f, 1.0f);
        }
    }

    if(ImGui::CollapsingHeader("Minimap", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enabled##minimap", &Settings::minimap.enabled);
        ImGui::Checkbox("Render shadows", &Settings::minimap.render_shadows);
        ImGui::Checkbox("Flat colors", &Settings::minimap.flat_colors);
        ImGui::SliderFloat("Size",     &Settings::minimap.size,     0.02f, 0.5f, "%.3f");
        ImGui::SliderFloat("Offset X", &Settings::minimap.offset_x, 0.0f, 500.0f, "%.0f");
        ImGui::SliderFloat("Offset Y", &Settings::minimap.offset_y, 0.0f, 500.0f, "%.0f");
    }

    if(ImGui::CollapsingHeader("Pointer", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool pointer_changed = false;

        pointer_changed |= ImGui::Checkbox("Custom pointer", &Settings::pointer.enabled);

        static const char* mode_names[] = { "Shape", "Item texture" };
        int mode_idx = static_cast<int>(Settings::pointer.mode);
        if(ImGui::Combo("Mode", &mode_idx, mode_names, IM_ARRAYSIZE(mode_names)))
        {
            Settings::pointer.mode = static_cast<Settings::Pointer::Mode>(mode_idx);
            pointer_changed = true;
        }

        if(Settings::pointer.mode == Settings::Pointer::Mode::Shape)
        {
            static const char* shape_names[] = { "Arrow", "Crosshair", "Dot", "Ring", "Diamond" };
            int shape_idx = static_cast<int>(Settings::pointer.shape);
            if(ImGui::Combo("Shape", &shape_idx, shape_names, IM_ARRAYSIZE(shape_names)))
            {
                Settings::pointer.shape = static_cast<Settings::Pointer::Shape>(shape_idx);
                pointer_changed = true;
            }

            float pointer_color[3] = {
                Settings::pointer.color.r / 255.0f,
                Settings::pointer.color.g / 255.0f,
                Settings::pointer.color.b / 255.0f
            };
            if(ImGui::ColorEdit3("Pointer color", pointer_color))
            {
                Settings::pointer.color.r = static_cast<std::uint8_t>(pointer_color[0] * 255.0f);
                Settings::pointer.color.g = static_cast<std::uint8_t>(pointer_color[1] * 255.0f);
                Settings::pointer.color.b = static_cast<std::uint8_t>(pointer_color[2] * 255.0f);
                pointer_changed = true;
            }
        }
        else
        {
            // Build a sorted list of (id, name) once per frame — itemDatabase
            // is an unordered_map so we need deterministic ordering for the
            // combo. Skips ItemID::None so the user cannot pick "nothing".
            std::vector<std::pair<uint32_t, const std::string*>> items;
            items.reserve(itemDatabase.size());
            for(const auto& [id, data] : itemDatabase)
            {
                if(id == ItemID::None) continue;
                items.emplace_back(static_cast<uint32_t>(id), &data.name);
            }
            std::sort(items.begin(), items.end(),
                [](const auto& a, const auto& b) { return *a.second < *b.second; });

            int selected_row = -1;
            for(std::size_t i = 0; i < items.size(); ++i)
            {
                if(items[i].first == Settings::pointer.item_id) { selected_row = static_cast<int>(i); break; }
            }

            const char* preview = (selected_row >= 0) ? items[selected_row].second->c_str() : "<none>";
            if(ImGui::BeginCombo("Item", preview))
            {
                for(std::size_t i = 0; i < items.size(); ++i)
                {
                    bool is_selected = (static_cast<int>(i) == selected_row);
                    if(ImGui::Selectable(items[i].second->c_str(), is_selected))
                    {
                        Settings::pointer.item_id = items[i].first;
                        pointer_changed = true;
                    }
                    if(is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        int pointer_size = static_cast<int>(Settings::pointer.size);
        if(ImGui::SliderInt("Pointer size", &pointer_size, 8, 128))
        {
            Settings::pointer.size = static_cast<uint32_t>(pointer_size);
            pointer_changed = true;
        }

        if(pointer_changed) apply_pointer_requested = true;
    }

    if(ImGui::CollapsingHeader("Gamepad", ImGuiTreeNodeFlags_DefaultOpen))
    {
        sf::Joystick::update();
        const bool connected = sf::Joystick::isConnected(Settings::gamepad.joystick_id);

        ImGui::Checkbox("Enabled##gamepad", &Settings::gamepad.enabled);

        ImGui::TextUnformatted(connected ? "Status: connected" : "Status: not detected");

        int joystick_id = static_cast<int>(Settings::gamepad.joystick_id);
        if(ImGui::SliderInt("Joystick index", &joystick_id, 0, static_cast<int>(sf::Joystick::Count) - 1))
        {
            Settings::gamepad.joystick_id = static_cast<std::uint32_t>(joystick_id);
        }

        ImGui::SliderFloat("Stick deadzone",    &Settings::gamepad.stick_deadzone,     0.0f, 90.0f, "%.0f");
        ImGui::SliderFloat("Cursor speed",      &Settings::gamepad.cursor_speed,       1.0f, 40.0f, "%.1f");
        ImGui::SliderFloat("Cursor max reach",  &Settings::gamepad.cursor_max_distance,1.0f, 20.0f, "%.1f");

        ImGui::Checkbox("Show cursor", &Settings::gamepad.show_cursor);

        int cursor_size = static_cast<int>(Settings::gamepad.cursor_size);
        if(ImGui::SliderInt("Cursor size", &cursor_size, 8, 128))
        {
            Settings::gamepad.cursor_size = static_cast<std::uint32_t>(cursor_size);
        }

        float cursor_color[3] = {
            Settings::gamepad.cursor_color.r / 255.0f,
            Settings::gamepad.cursor_color.g / 255.0f,
            Settings::gamepad.cursor_color.b / 255.0f
        };
        if(ImGui::ColorEdit3("Cursor color", cursor_color))
        {
            Settings::gamepad.cursor_color.r = static_cast<std::uint8_t>(cursor_color[0] * 255.0f);
            Settings::gamepad.cursor_color.g = static_cast<std::uint8_t>(cursor_color[1] * 255.0f);
            Settings::gamepad.cursor_color.b = static_cast<std::uint8_t>(cursor_color[2] * 255.0f);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Button mapping");
        ImGui::TextDisabled("Click 'Rebind' then press a button on the controller.");

        // Rebind state is local to the UI — one active slot at a time. Kept
        // as statics so the ImGui frame can latch onto a pending capture.
        static int rebind_action = -1;

        const char* action_names[static_cast<std::size_t>(Settings::Gamepad::Action::Count)] = {
            "Jump", "Attack", "Use", "Drop", "Drop (full stack)",
            "Previous slot", "Next slot", "Open inventory"
        };

        // If a rebind is pending, scan the pad for any pressed button and
        // commit it. Esc or clicking Cancel aborts.
        if(rebind_action >= 0)
        {
            const unsigned int count = sf::Joystick::getButtonCount(Settings::gamepad.joystick_id);
            for(unsigned int i = 0; i < count && i < 32; ++i)
            {
                if(sf::Joystick::isButtonPressed(Settings::gamepad.joystick_id, i))
                {
                    Settings::gamepad.buttons[rebind_action] = static_cast<int>(i);
                    rebind_action = -1;
                    break;
                }
            }
        }

        for(int i = 0; i < static_cast<int>(Settings::Gamepad::Action::Count); ++i)
        {
            ImGui::PushID(i);
            const int btn = Settings::gamepad.buttons[i];
            char label[48];
            if(btn == Settings::Gamepad::UNBOUND) std::snprintf(label, sizeof(label), "unbound");
            else                                  std::snprintf(label, sizeof(label), "button %d", btn);

            ImGui::Text("%-17s  %s", action_names[i], label);
            ImGui::SameLine();
            if(rebind_action == i)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "press any button...");
                ImGui::SameLine();
                if(ImGui::SmallButton("Cancel")) rebind_action = -1;
            }
            else
            {
                if(ImGui::SmallButton("Rebind")) rebind_action = i;
                ImGui::SameLine();
                if(ImGui::SmallButton("Clear"))  Settings::gamepad.buttons[i] = Settings::Gamepad::UNBOUND;
            }
            ImGui::PopID();
        }

        if(ImGui::Button("Reset mapping"))
        {
            Settings::Gamepad defaults{};
            for(std::size_t i = 0; i < static_cast<std::size_t>(Settings::Gamepad::Action::Count); ++i)
            {
                Settings::gamepad.buttons[i] = defaults.buttons[i];
            }
        }
    }

    if(ImGui::CollapsingHeader("Interface", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float color[3] = {
            Settings::ui.main_color.r / 255.0f,
            Settings::ui.main_color.g / 255.0f,
            Settings::ui.main_color.b / 255.0f
        };
        if(ImGui::ColorEdit3("Main color", color))
        {
            Settings::ui.main_color.r = static_cast<std::uint8_t>(color[0] * 255.0f);
            Settings::ui.main_color.g = static_cast<std::uint8_t>(color[1] * 255.0f);
            Settings::ui.main_color.b = static_cast<std::uint8_t>(color[2] * 255.0f);
        }
    }

    if(ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Render hitboxes",      &Settings::debug.render_hitboxes);
        ImGui::Checkbox("Render chunk borders", &Settings::debug.render_chunk_borders);
        if(ImGui::Button("Console"))
        {
            game->getConsole().setOpen(true);
        }
    }

    ImGui::Separator();

    if(ImGui::Button("Save"))
    {
        Settings::save();
    }
    ImGui::SameLine();
    if(ImGui::Button("Close"))
    {
        open = false;
    }

    ImGui::End();
}
