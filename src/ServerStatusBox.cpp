#include "../include/ServerStatusBox.hpp"

#include <imgui.h>

void ServerStatusBox::setValues(const StatusResponsePacket& packet, const uint16_t latency)
{
    this->packet.emplace(packet);
    this->latency.emplace(latency);
}

void ServerStatusBox::performImGui(sf::RenderTarget& target)
{
    
}