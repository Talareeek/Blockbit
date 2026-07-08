#include "../include/ServerPreview.hpp"
#include "../include/AssetManager.hpp"

#include <algorithm>
#include <cstring>
#include <string>

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

    sf::RectangleShape panel(size);
    panel.setPosition(position);
    panel.setFillColor(sf::Color(0, 0, 0, 180));
    panel.setOutlineColor(sf::Color(120, 200, 255, 180));
    panel.setOutlineThickness(1.5f);
    window.draw(panel);

    float padding = std::clamp(size.y * 0.10f, 4.0f, 12.0f);
    float icon_side = std::max(0.0f, std::min({size.y - padding * 2.0f, size.x * 0.40f, 96.0f}));

    if (!has_icon_texture || std::memcmp(cached_icon, packet->icon, 8192) != 0)
    {
        icon_texture = getTextureFromIcon(packet->icon);
        std::memcpy(cached_icon, packet->icon, 8192);
        has_icon_texture = true;
    }

    if (icon_side > 0.0f)
    {
        sf::RectangleShape icon({icon_side, icon_side});
        icon.setTexture(&icon_texture);
        icon.setPosition({position.x + padding, position.y + padding});
        window.draw(icon);
    }

    float text_x = position.x + padding + (icon_side > 0.0f ? icon_side + padding : 0.0f);
    float text_right = position.x + size.x - padding;
    float text_width = std::max(0.0f, text_right - text_x);
    float text_top = position.y + padding;

    unsigned name_size = static_cast<unsigned>(std::clamp(icon_side * 0.26f, 10.0f, 22.0f));
    unsigned body_size = static_cast<unsigned>(std::clamp(icon_side * 0.18f, 9.0f, 16.0f));

    sf::Text name_text(AssetManager::getFont(0), packet->name, name_size);
    name_text.setFillColor(sf::Color(240, 240, 240));
    name_text.setOutlineColor(sf::Color::Black);
    name_text.setOutlineThickness(1.0f);
    name_text.setPosition({text_x, text_top});
    fitText(name_text, text_width);
    window.draw(name_text);

    float description_y = text_top + static_cast<float>(name_size) + 4.0f;
    float players_h = static_cast<float>(body_size) + 4.0f;
    float description_bottom = position.y + size.y - padding - players_h;

    if (description_y + body_size <= description_bottom)
    {
        sf::Text description_text(AssetManager::getFont(0), packet->description, body_size);
        description_text.setFillColor(sf::Color(200, 200, 200));
        description_text.setPosition({text_x, description_y});
        fitText(description_text, text_width);
        window.draw(description_text);
    }

    std::string players = std::to_string(packet->players) + " / " + std::to_string(packet->max_players);
    sf::Text players_text(AssetManager::getFont(0), players, body_size);
    players_text.setFillColor(sf::Color(180, 220, 255));
    players_text.setOutlineColor(sf::Color::Black);
    players_text.setOutlineThickness(1.0f);
    fitText(players_text, text_width);
    sf::FloatRect pb = players_text.getLocalBounds();
    players_text.setPosition({text_right - pb.size.x, position.y + size.y - pb.size.y - padding});
    window.draw(players_text);
}
