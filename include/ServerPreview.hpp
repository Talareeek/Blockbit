#ifndef SERVER_PREVIEW_HPP
#define SERVER_PREVIEW_HPP

#include "UIElement.hpp"
#include "Packet.hpp"

class ServerPreview : public UIElement
{
private:

    StatusResponsePacket* packet = nullptr;

    sf::Texture icon_texture;
    uint8_t cached_icon[8192] = {};
    bool has_icon_texture = false;

public:

    ServerPreview() = default;
    ServerPreview(StatusResponsePacket* packet);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void setPacket(StatusResponsePacket* packet);
};

#endif // SERVER_PREVIEW_HPP
