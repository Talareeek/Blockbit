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

    ATTACK_START,
    ATTACK_STOP,

    USE_START,
    USE_STOP,

    DROP,
    CHANGE_SLOT,

    OPEN_INVENTORY
};

struct ChangeSlotData
{
    bool scroll;
    int8_t value;
};

struct Input
{
    InputType type;

    std::variant<std::monostate, sf::Vector2<double>, uint8_t, bool, ChangeSlotData> value;
};

std::vector<Input> getInputs(const World& world, sf::Vector2<double> camera, const sf::RenderWindow& window);
std::vector<Input> getInputsFromEvent(const sf::Event& event, sf::Vector2<double> camera, const sf::RenderWindow& window);

void processWorldInputs(World& world, std::vector<Input> inputs, UUID id);

// Client-side player prediction. Applies ONLY movement-related inputs
// (MOVE, JUMP) to the local player's physics so motion feels instant;
// server stays authoritative for attack / use / drop / inventory and will
// correct position via the next snapshot.
void applyLocalPlayerInputPrediction(World& world, const std::vector<Input>& inputs, UUID id);

#endif // INPUT_HPP
