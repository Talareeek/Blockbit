#ifndef PLAYER_COMPONENT_HPP
#define PLAYER_COMPONENT_HPP

#include <string>
#include <cstdint>
#include <sstream>

struct PlayerComponent
{
    bool ableToMove = true;

    enum class InputDevice
    {
        MOUSE_KEYBOARD,
        JOYSTICK // currently unavailable
    } device;

    std::string serialize() const
    {
        std::string output;

        output += std::to_string(ableToMove) + '\n';
        output += std::to_string(static_cast<uint32_t>(device)) + '\n';

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream stream(data);
        
        stream >> ableToMove;

        uint32_t device_i;

        stream >> device_i;

        device = static_cast<PlayerComponent::InputDevice>(device_i);
    }
};

#endif // PLAYER_COMPONENT_HPP