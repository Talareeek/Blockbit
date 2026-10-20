#include "../include/WorldEntry.hpp"
#include "../include/AssetManager.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

void drawFitText(sf::RenderWindow& window, const std::string& str, const sf::FloatRect& box, bool center, sf::Color fill, float outline_thickness, sf::Color outline)
{
    if(str.empty() || box.size.x <= 0.0f || box.size.y <= 0.0f) return;

    sf::Text txt(AssetManager::getFont(AssetManager::FontID::PressStart2P), str, static_cast<unsigned int>(std::max(1.0f, box.size.y * 0.7f)));

    auto bounds = txt.getLocalBounds();
    if(bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) return;

    float sx = (box.size.x * 0.92f) / bounds.size.x;
    float sy = (box.size.y * 0.78f) / bounds.size.y;
    float scale = std::min({1.0f, sx, sy});

    txt.setScale({scale, scale});
    txt.setFillColor(fill);
    txt.setOutlineThickness(outline_thickness);
    txt.setOutlineColor(outline);

    bounds = txt.getLocalBounds();

    if(center)
    {
        txt.setPosition({
            box.position.x + (box.size.x - bounds.size.x * scale) * 0.5f - bounds.position.x * scale,
            box.position.y + (box.size.y - bounds.size.y * scale) * 0.5f - bounds.position.y * scale
        });
    }
    else
    {
        txt.setPosition({
            box.position.x + box.size.x * 0.04f - bounds.position.x * scale,
            box.position.y + (box.size.y - bounds.size.y * scale) * 0.5f - bounds.position.y * scale
        });
    }

    window.draw(txt);
}

WorldEntry::WorldEntry(const std::string& name, const std::filesystem::path& path)
    : name(name), path(path)
{
}

const std::string& WorldEntry::getName() const     { return name; }
const std::filesystem::path& WorldEntry::getPath() const { return path; }
bool WorldEntry::wasPlayRequested() const   { return play_requested; }
bool WorldEntry::wasHostRequested() const   { return host_requested; }
bool WorldEntry::wasDeleteRequested() const { return delete_requested; }
void WorldEntry::clearRequests()
{
    play_requested = false;
    host_requested = false;
    delete_requested = false;
}

void WorldEntry::handleEvent(const sf::Event&) {}
void WorldEntry::update(float) {}

void WorldEntry::render(sf::RenderWindow& window)
{
    (void)window;

    if(size.x <= 0.0f || size.y <= 0.0f) return;

    sf::FloatRect entry_rect{position, size};
    sf::FloatRect vis = entry_rect;
    if(clip_rect.size.x > 0.0f && clip_rect.size.y > 0.0f)
    {
        auto inter = entry_rect.findIntersection(clip_rect);
        if(!inter) return;
        vis = *inter;
    }
    if(vis.size.x <= 0.0f || vis.size.y <= 0.0f) return;

    char win_id[64];
    std::snprintf(win_id, sizeof(win_id), "##world_entry_%p", static_cast<void*>(this));

    constexpr float PAD = 12.0f;
    constexpr float GAP = 10.0f;

    ImGui::SetNextWindowPos(ImVec2(vis.position.x, vis.position.y));
    ImGui::SetNextWindowSize(ImVec2(vis.size.x, vis.size.y));

    ImGui::PushStyleColor(ImGuiCol_WindowBg,     IM_COL32( 42,  70,  42, 240));
    ImGui::PushStyleColor(ImGuiCol_Border,       IM_COL32(130, 200, 130, 200));
    ImGui::PushStyleColor(ImGuiCol_Text,         IM_COL32(240, 255, 240, 255));
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar  (ImGuiStyleVar_FrameRounding,    0.0f);
    ImGui::PushStyleVar  (ImGuiStyleVar_FrameBorderSize,  0.0f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar       | ImGuiWindowFlags_NoResize        |
        ImGuiWindowFlags_NoMove           | ImGuiWindowFlags_NoScrollbar     |
        ImGuiWindowFlags_NoCollapse       | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin(win_id, nullptr, flags);

    float ox = position.x - vis.position.x;
    float oy = position.y - vis.position.y;

    ImGui::SetWindowFontScale(1.6f);
    ImFont* font     = ImGui::GetFont();
    float   font_sz  = ImGui::GetFontSize();
    float   title_h  = ImGui::GetTextLineHeight();

    float x_size      = title_h + 6.0f;
    float title_max_w = size.x - PAD * 2.0f - x_size - GAP;

    std::string title = name;
    auto widthOf = [&](const std::string& s)
    {
        return font->CalcTextSizeA(font_sz, FLT_MAX, 0.0f, s.c_str()).x;
    };
    if(widthOf(title) > title_max_w)
    {
        while(!title.empty() && widthOf(title + "...") > title_max_w) title.pop_back();
        if(!title.empty()) title += "...";
    }

    ImGui::SetCursorPos(ImVec2(ox + PAD, oy + PAD));
    ImGui::TextUnformatted(title.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPos(ImVec2(ox + size.x - PAD - x_size, oy + PAD + (title_h - x_size) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(140,  45,  45, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(185,  60,  60, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(215,  80,  80, 255));
    char del_lbl[48];
    std::snprintf(del_lbl, sizeof(del_lbl), "x##d%p", static_cast<void*>(this));
    if(ImGui::Button(del_lbl, ImVec2(x_size, x_size)))
    {
        delete_requested = true;
        click_sound.setVolume(100);
        click_sound.play();
    }
    ImGui::PopStyleColor(3);

    float row_top  = PAD + title_h + GAP;
    float row_h    = size.y - row_top - PAD;
    if(row_h < 10.0f) row_h = 10.0f;
    float row_w    = (size.x - PAD * 2.0f - GAP) * 0.5f;

    ImGui::SetWindowFontScale(1.15f);

    ImGui::SetCursorPos(ImVec2(ox + PAD, oy + row_top));
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32( 55, 115,  55, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32( 80, 160,  80, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(100, 190, 100, 255));
    char play_lbl[48];
    std::snprintf(play_lbl, sizeof(play_lbl), "Play##p%p", static_cast<void*>(this));
    if(ImGui::Button(play_lbl, ImVec2(row_w, row_h)))
    {
        play_requested = true;
        click_sound.setVolume(100);
        click_sound.play();
    }
    ImGui::PopStyleColor(3);

    ImGui::SetCursorPos(ImVec2(ox + PAD + row_w + GAP, oy + row_top));
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32( 55,  95, 135, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32( 80, 125, 180, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(100, 155, 210, 255));
    char host_lbl[48];
    std::snprintf(host_lbl, sizeof(host_lbl), "Host##h%p", static_cast<void*>(this));
    if(ImGui::Button(host_lbl, ImVec2(row_w, row_h)))
    {
        host_requested = true;
        click_sound.setVolume(100);
        click_sound.play();
    }
    ImGui::PopStyleColor(3);

    ImGui::SetWindowFontScale(1.0f);

    ImGui::End();
    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(3);
}
