#ifndef INPUT_HPP
#define INPUT_HPP

#include <SFML/System.hpp>

#include <cstdint>
#include <variant>

enum class InputType
{
    MOVE,
    JUMP,
    USE,
    ATTACK,
    DROP,
    CHANGE_SLOT
};

struct DropInfo
{
    sf::Vector2f mousePosition;
    bool fullStack;
};

struct Input
{
    InputType type;

    std::variant<std::monostate, sf::Vector2f, uint8_t, bool, DropInfo> value;
};

#endif // INPUT_HPP
