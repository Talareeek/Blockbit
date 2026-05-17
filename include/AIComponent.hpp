#ifndef AI_COMPONENT_HPP
#define AI_COMPONENT_HPP

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <string>
#include <sstream>

struct AIComponent
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
};

#endif // AI_COMPONENT_HPP
