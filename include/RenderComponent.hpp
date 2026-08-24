#ifndef RENDER_COMPONENT_HPP
#define RENDER_COMPONENT_HPP

#include "Component.hpp"

#include <cstdint>
#include <SFML/Graphics/Rect.hpp>
#include <string>
#include <sstream>

struct RenderComponent : public Component
{
    uint16_t textureID;
    sf::IntRect uv;
    sf::Vector2f size;

    RenderComponent(
        uint16_t textureID = 0,
        sf::IntRect uv = {},
        sf::Vector2f size = {}
    )
        : textureID(textureID),
        uv(uv),
        size(size)
    {
    }

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

    std::string name() const override
    {
        return "RenderComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["texture_id"] = Tag(textureID);

        compound["uv"] = Tag(TagCompound({
            {"x", Tag(uv.position.x)},
            {"y", Tag(uv.position.y)},
            {"w", Tag(uv.size.x)},
            {"h", Tag(uv.size.y)}
        }));

        compound["size"] = Tag(TagCompound({
            {"x", Tag(size.x)},
            {"y", Tag(size.y)}
        }));

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        textureID = tag["texture_id"].get<uint16_t>();

        const auto& uvTag = tag["uv"].get<TagCompound>();

        uv.position.x = uvTag.at("x").get<int>();
        uv.position.y = uvTag.at("y").get<int>();
        uv.size.x = uvTag.at("w").get<int>();
        uv.size.y = uvTag.at("h").get<int>();

        const auto& sizeTag = tag["size"].get<TagCompound>();

        size.x = sizeTag.at("x").get<float>();
        size.y = sizeTag.at("y").get<float>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<RenderComponent>(*this);
    }
};

#endif // RENDER_COMPONENT_HPP