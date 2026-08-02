#ifndef TRANSFORM_COMPONENT_HPP
#define TRANSFORM_COMPONENT_HPP

#include "Component.hpp"

#include "GameCommon.hpp"

#include <SFML/System/Vector2.hpp>

#include <string>
#include <sstream>

struct TransformComponent : public Component
{
    sf::Vector2<double> position;
    sf::Vector2<double> size;
    sf::Angle rotation;

    sf::Vector2<double> previous_position;

    void teleport(const sf::Vector2<double>& position);
    void move(const sf::Vector2<double>& offset);

    TransformComponent() = default;

    TransformComponent(const sf::Vector2<double> position, const sf::Vector2<double> size, const sf::Angle rotation)
    {
        this->position = position;
        this->size = size;
        this->rotation = rotation;
    }

    TransformComponent(const sf::Vector2<double> center, const sf::Vector2<double> size)
    {
        position = center - size / 2.0;
        this->size = size;
    }

    sf::Vector2<double> center() const
    {
        return position + size / 2.0;
    }


    std::string serialize()
    {
        std::string output;

        output += std::to_string(position.x) + ' ' + std::to_string(position.y) + '\n';
        output += std::to_string(size.x) + ' ' + std::to_string(size.y) + '\n';
        output += std::to_string(rotation.asDegrees()) + '\n';

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        std::string line;

        std::getline(iss, line);
        std::istringstream positionStream(line);
        positionStream >> position.x >> position.y;

        std::getline(iss, line);
        std::istringstream sizeStream(line);
        sizeStream >> size.x >> size.y;

        std::getline(iss, line);
        std::istringstream rotationStream(line);
        float rotationDegrees;
        rotationStream >> rotationDegrees;
        rotation = sf::degrees(rotationDegrees);
    }

    std::string name() const override
    {
        return "TransformComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["position"] = Tag(position);
        compound["size"] = Tag(size);

        compound["rotation"] = Tag(rotation.asDegrees());

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        position = tag["position"].get<sf::Vector2<double>>();
        size = tag["size"].get<sf::Vector2<double>>();

        rotation = sf::degrees(tag["rotation"].get<float>());
    }

    int chunkPosition() const
    {
        return static_cast<int>(std::floor(position.x) / 16);
    }

    int previousChunkPosition() const
    {
        return positionToChunk(previous_position);
    }

    bool movedBetweenChunks() const
    {
        return chunkPosition() != previousChunkPosition();
    }

    bool moved() const
    {
        return position != previous_position;
    }
};

#endif // TRANSFORM_COMPONENT_HPP