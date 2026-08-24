#ifndef PHYSICS_COMPONENT_HPP
#define PHYSICS_COMPONENT_HPP

#include "Component.hpp"

#include <SFML/System/Vector2.hpp>
#include <string>
#include <sstream>

struct PhysicsComponent : public Component
{
    sf::Vector2f velocity;
    sf::Vector2f force;
    sf::Vector2f acceleration;
    float mass = 1.0f;
    bool onGround = false;
    
    bool isSolid = true;
    bool isStatic = false;
    bool isGravityActive = true;

    PhysicsComponent(sf::Vector2f velocity = {}, sf::Vector2f force = {}, sf::Vector2f acceleration = {}, float mass = 1.0f, bool onGround = false, bool isSolid = true, bool isStatic = false, bool isGravityActive = true)
        : velocity(velocity),
        force(force),
        acceleration(acceleration),
        mass(mass),
        onGround(onGround),
        isSolid(isSolid),
        isStatic(isStatic),
        isGravityActive(isGravityActive)
    {
        
    }

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

    std::string name() const override
    {
        return "PhysicsComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["velocity"] = Tag(velocity);
        compound["force"] = Tag(force);
        compound["mass"] = Tag(mass);
        compound["on_ground"] = Tag(onGround);
        compound["is_solid"] = Tag(isSolid);
        compound["is_static"] = Tag(isStatic);
        compound["is_gravity_active"] = Tag(isGravityActive);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        velocity = tag["velocity"].get<sf::Vector2f>();
        force = tag["force"].get<sf::Vector2f>();
        mass = tag["mass"].get<float>();
        onGround = tag["on_ground"].get<bool>();
        isSolid = tag["is_solid"].get<bool>();
        isStatic = tag["is_static"].get<bool>();
        isGravityActive = tag["is_gravity_active"].get<bool>();
    }

    constexpr static sf::Vector2f damping{6.0f, 0.2f};

    constexpr static float SAFE_FALL_VELOCITY{18.0f};
    constexpr static float FALL_DAMAGE_MULTIPLIER{1.75f};
    constexpr static float TERMINAL_VELOCITY{60.0f};

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<PhysicsComponent>(*this);
    }
};

#endif // PHYSICS_COMPONENT_HPP