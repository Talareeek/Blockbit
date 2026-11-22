#ifndef SERVER_STATUS_BOX_HPP
#define SERVER_STATUS_BOX_HPP

#include "Packet.hpp"

#include <SFML/Graphics.hpp>

class ServerStatusBox
{
private:

    std::optional<StatusResponsePacket> packet;
    std::optional<uint16_t> latency;

public:

    void setValues(const StatusResponsePacket& packet, const uint16_t latency);

    void performImGui(sf::RenderTarget& target);

};

#endif // SERVER_STATUS_BOX_HPP