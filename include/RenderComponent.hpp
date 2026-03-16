#ifndef RENDER_COMPONENT_HPP
#define RENDER_COMPONENT_HPP

#include <cstdint>
#include <SFML/Graphics/Rect.hpp>
#include <string>
#include <sstream>

struct RenderComponent
{
    uint16_t textureID;
    sf::IntRect uv;
    sf::Vector2f size;

    std::string serialize()
    {
        std::string output;

        output += std::to_string(textureID) + '\n';
        output += std::to_string(uv.position.x) + ' ' + std::to_string(uv.position.y) + ' ' + std::to_string(uv.size.x) + ' ' + std::to_string(uv.size.y) + '\n';
        output += std::to_string(size.x) + ' ' + std::to_string(size.y) + '\n';

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        iss >> textureID;
        iss >> uv.position.x >> uv.position.y >> uv.size.x >> uv.size.y;
        iss >> size.x >> size.y;
    }

};

#endif // RENDER_COMPONENT_HPP