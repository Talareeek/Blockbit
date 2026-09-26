#ifndef RCON_HPP
#define RCON_HPP

#include <SFML/Graphics.hpp>

#include "../include/GameServer.hpp"

class RCon
{
private:

    bool open = false;

public:

    void handleEvent(const sf::Event& event);
    void performImGui(GameServer& server);

};

#endif // RCON_HPP