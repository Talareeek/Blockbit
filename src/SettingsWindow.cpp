#include "../include/SettingsWindow.hpp"

#include "../include/Settings.hpp"

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
