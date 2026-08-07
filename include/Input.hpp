#ifndef INPUT_HPP
#define INPUT_HPP

#include "UUID.hpp"

#include <SFML/System.hpp>

#include <cstdint>
#include <variant>
#include <vector>

namespace sf { class Event; class RenderWindow; }
class World;

enum class InputType
{
    MOVE,
    JUMP,

    MOUSE_MOVE,

    ATTACK_START,
    ATTACK_STOP,

    USE_START,
    USE_STOP,

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

    std::variant<std::monostate, sf::Vector2<double>, uint8_t, bool, DropInfo> value;
};

std::vector<Input> getInputs(const World& world, const sf::RenderWindow& window);
std::vector<Input> getInputsFromEvent(const sf::Event& event, sf::Vector2<double> camera, const sf::RenderWindow& window, uint8_t& selectedSlot);

void processWorldInputs(World& world, std::vector<Input> inputs, UUID id);

#endif // INPUT_HPP
