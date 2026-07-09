#ifndef TRANSFORM_COMPONENT_HPP
#define TRANSFORM_COMPONENT_HPP

#include <SFML/System/Vector2.hpp>

#include <string>
#include <sstream>

struct TransformComponent
{
    sf::Vector2<double> position;
    sf::Vector2<double> size;
    sf::Angle rotation;

    void teleport(const sf::Vector2<double>& position);
    void move(const sf::Vector2<double>& offset);

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
};

#endif // TRANSFORM_COMPONENT_HPP