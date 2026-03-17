#ifndef PHYSICS_COMPONENT_HPP
#define PHYSICS_COMPONENT_HPP

#include <SFML/System/Vector2.hpp>
#include <string>
#include <sstream>

struct PhysicsComponent
{
    sf::Vector2f velocity;
    sf::Vector2f force;
    sf::Vector2f acceleration;
    float mass = 1.0f;
    bool onGround = false;
    
    bool isSolid = true;
    bool isStatic = false;
    bool isGravityActive = true;

    std::string serialize()
    {
        std::string output;

        output += std::to_string(velocity.x) + ' ' + std::to_string(velocity.y) + '\n';
        output += std::to_string(force.x) + ' ' + std::to_string(force.y) + '\n';
        output += std::to_string(mass) + '\n';
        output += std::to_string(onGround ? 1 : 0) + '\n';
        output += std::to_string(isSolid ? 1 : 0) + '\n';
        output += std::to_string(isStatic ? 1 : 0) + '\n';
        output += std::to_string(isGravityActive ? 1 : 0) + '\n';

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        iss >> velocity.x >> velocity.y;
        iss >> force.x >> force.y;
        iss >> mass;
        int onGroundInt, isSolidInt, isStaticInt, isGravityActiveInt;
        iss >> onGroundInt >> isSolidInt >> isStaticInt >> isGravityActiveInt;
        onGround = onGroundInt != 0;
        isSolid = isSolidInt != 0;
        isStatic = isStaticInt != 0;
        isGravityActive = isGravityActiveInt != 0;
    }

    constexpr static sf::Vector2f damping{6.0f, 0.2f};

    constexpr static float SAFE_FALL_VELOCITY{18.0f};
    constexpr static float FALL_DAMAGE_MULTIPLIER{1.6f};
    constexpr static float TERMINAL_VELOCITY{55.0f};
};

#endif // PHYSICS_COMPONENT_HPP