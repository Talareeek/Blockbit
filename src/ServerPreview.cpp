#include "../include/ServerPreview.hpp"
#include "../include/AssetManager.hpp"

#include <algorithm>
#include <cstring>
#include <string>

#include <imgui-SFML.h>

sf::Texture getTextureFromIcon(const uint8_t* icon)
{
    constexpr unsigned WIDTH  = 64;
    constexpr unsigned HEIGHT = 64;

    std::vector<std::uint8_t> rgba(WIDTH * HEIGHT * 4);

    const auto* raw = reinterpret_cast<const std::uint8_t*>(icon);

    for (std::size_t i = 0; i < WIDTH * HEIGHT; ++i)
    {
        std::uint16_t px = static_cast<std::uint16_t>(raw[i * 2]) | (static_cast<std::uint16_t>(raw[i * 2 + 1]) << 8);

        std::uint8_t r5 = (px >> 11) & 0x1F;
        std::uint8_t g6 = (px >> 5)  & 0x3F;
        std::uint8_t b5 =  px        & 0x1F;

        std::uint8_t r8 = (r5 << 3) | (r5 >> 2);
        std::uint8_t g8 = (g6 << 2) | (g6 >> 4);
        std::uint8_t b8 = (b5 << 3) | (b5 >> 2);

        rgba[i * 4 + 0] = r8;
        rgba[i * 4 + 1] = g8;
        rgba[i * 4 + 2] = b8;
        rgba[i * 4 + 3] = 255;
    }

    sf::Image image(sf::Vector2u(WIDTH, HEIGHT), rgba.data());

    sf::Texture texture;
    if (!texture.loadFromImage(image)) throw std::runtime_error("Cannot recreate server icon");

    return texture;
}

ServerPreview::ServerPreview(StatusResponsePacket* packet) : packet{packet}
{

}

void ServerPreview::setPacket(StatusResponsePacket* packet)
{
    this->packet = packet;
    has_icon_texture = false;
}

void ServerPreview::handleEvent(const sf::Event& event)
{

}

void ServerPreview::update(float dt)
{

}

namespace
{
    void fitText(sf::Text& text, float max_width)
    {
        if (max_width <= 0.0f)
        {
            text.setString("");
            return;
        }

        if (text.getLocalBounds().size.x <= max_width) return;

        sf::String original = text.getString();
        sf::String truncated = original;

        const sf::String ellipsis = "...";
        text.setString(ellipsis);
        if (text.getLocalBounds().size.x > max_width)
        {
            text.setString("");
            return;
        }

        while (!truncated.isEmpty())
        {
            truncated = truncated.substring(0, truncated.getSize() - 1);
            text.setString(truncated + ellipsis);
            if (text.getLocalBounds().size.x <= max_width) return;
        }

        text.setString(ellipsis);
    }
}

void ServerPreview::render(sf::RenderWindow& window)
{
    if (!packet) return;
    if (size.x <= 0.0f || size.y <= 0.0f) return;

    ImGui::SetNextWindowPos(ImVec2(position.x, position.y));
    ImGui::SetNextWindowSize(ImVec2(size.x, size.y));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 180.0f / 255.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(120.0f / 255.0f, 200.0f / 255.0f, 255.0f / 255.0f, 180.0f / 255.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                              ImGuiWindowFlags_NoInputs;

    ImGui::Begin("##server_preview", nullptr, flags);

    float padding = 8.0f;
    float icon_side = std::min({size.y - padding * 2.0f, 64.0f});

    if (!has_icon_texture || std::memcmp(cached_icon, packet->icon, 8192) != 0)
    {
        icon_texture = getTextureFromIcon(packet->icon);
        std::memcpy(cached_icon, packet->icon, 8192);
        has_icon_texture = true;
    }

    ImVec2 window_pos = ImGui::GetWindowPos();

    ImGui::SetCursorPos(ImVec2(padding, padding));
    if (icon_side > 0.0f)
        ImGui::Image(icon_texture, sf::Vector2f(icon_side, icon_side));

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("%s", packet->name.c_str());
    ImGui::TextColored(ImVec4(0.78f, 0.78f, 0.78f, 1.0f), "%s", packet->description.c_str());
    ImGui::EndGroup();

    std::string players = std::to_string(packet->players) + " / " + std::to_string(packet->max_players);
    ImVec2 players_size = ImGui::CalcTextSize(players.c_str());
    ImGui::SetCursorScreenPos(ImVec2(window_pos.x + size.x - padding - players_size.x,
                                      window_pos.y + size.y - padding - players_size.y));
    ImGui::TextColored(ImVec4(0.7f, 0.86f, 1.0f, 1.0f), "%s", players.c_str());

    std::string datetime_str = dateAndHourString(packet->date, packet->hour);
    ImVec2 datetime_size = ImGui::CalcTextSize(datetime_str.c_str());
    ImGui::SetCursorScreenPos(ImVec2(window_pos.x + padding,
                                      window_pos.y + size.y - padding - datetime_size.y));
    ImGui::TextColored(ImVec4(0.7f, 0.86f, 1.0f, 1.0f), "%s", datetime_str.c_str());

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}