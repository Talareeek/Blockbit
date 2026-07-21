#ifndef AI_COMPONENT_HPP
#define AI_COMPONENT_HPP

#include "Component.hpp"

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <string>
#include <sstream>

struct AIComponent : public Component
{
    enum class State : uint8_t
    {
        Idle      = 0,
        Wandering = 1,
        Chasing   = 2
    };

    enum class Personality : uint8_t
    {
        Passive    = 0,
        Aggressive = 1
    };

    State       state       = State::Idle;
    Personality personality = Personality::Passive;

    // detection / combat (in tiles)
    float detectionRange = 8.0f;
    float attackRange    = 1.5f;
    float fleeRange      = 5.0f;

    // movement force magnitude (in line with PlayerInputSystem values)
    float moveSpeed = 35.0f;

    // idle <-> wandering cycle
    float stateTimer    = 0.0f;
    float stateDuration = 2.0f;
    int   wanderDir     = 0; // -1 left, 0 still, +1 right

    // attack pacing
    float attackCooldown = 0.0f;

    static constexpr float    ATTACK_INTERVAL = 1.0f;
    static constexpr uint32_t ATTACK_DAMAGE   = 5;

    std::string serialize()
    {
        std::string output;

        output += std::to_string(static_cast<uint32_t>(state)) + ' ';
        output += std::to_string(static_cast<uint32_t>(personality)) + '\n';
        output += std::to_string(detectionRange) + ' ';
        output += std::to_string(attackRange) + ' ';
        output += std::to_string(fleeRange) + ' ';
        output += std::to_string(moveSpeed) + '\n';
        output += std::to_string(stateTimer) + ' ';
        output += std::to_string(stateDuration) + ' ';
        output += std::to_string(wanderDir) + '\n';
        output += std::to_string(attackCooldown) + '\n';

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);

        uint32_t stateInt = 0;
        uint32_t personalityInt = 0;

        iss >> stateInt >> personalityInt;
        iss >> detectionRange >> attackRange >> fleeRange >> moveSpeed;
        iss >> stateTimer >> stateDuration >> wanderDir;
        iss >> attackCooldown;

        state       = static_cast<State>(stateInt);
        personality = static_cast<Personality>(personalityInt);
    }


    std::string name() const override
    {
        return "AIComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["state"] = Tag(static_cast<uint8_t>(state));
        compound["personality"] = Tag(static_cast<uint8_t>(personality));
        compound["detection_range"] = Tag(detectionRange);
        compound["attack_range"] = Tag(attackRange);
        compound["flee_range"] = Tag(fleeRange);
        compound["move_speed"] = Tag(moveSpeed);
        compound["state_timer"] = Tag(stateTimer);
        compound["state_duration"] = Tag(stateDuration);
        compound["wander_dir"] = Tag(wanderDir);
        compound["attack_cooldown"] = Tag(attackCooldown);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        state = static_cast<State>(tag["state"].get<uint8_t>());
        personality = static_cast<Personality>(tag["personality"].get<uint8_t>());
        detectionRange = tag["detection_range"].get<float>();
        attackRange = tag["attack_range"].get<float>();
        fleeRange = tag["flee_range"].get<float>();
        moveSpeed = tag["move_speed"].get<float>();
        stateTimer = tag["state_timer"].get<float>();
        stateDuration = tag["state_duration"].get<float>();
        wanderDir = tag["wander_dir"].get<int>();
        attackCooldown = tag["attack_cooldown"].get<float>();
    }
};

#endif // AI_COMPONENT_HPP
