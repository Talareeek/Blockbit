#include "../include/Input.hpp"
#include "../include/World.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>

std::vector<Input> getInputs(const World& world, const sf::RenderWindow& window)
{
    std::vector<Input> inputs;

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
    {
        inputs.push_back({InputType::MOVE, sf::Vector2f{-1.0f, 0.0f}});
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
    {
        inputs.push_back({InputType::MOVE, sf::Vector2f{1.0f, 0.0f}});
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
    {
        inputs.push_back({InputType::JUMP, std::monostate{}});
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
    {
        inputs.push_back({InputType::DROP, DropInfo{
            getMouseWorldPosition(world, window),
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)}});
    }

    return inputs;
}

std::vector<Input> getInputsFromEvent(const sf::Event& event, const World& world, const sf::RenderWindow& window, uint8_t& selectedSlot)
{
    std::vector<Input> inputs;

    if(event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();

        if(mouse->button == sf::Mouse::Button::Left)
        {
            inputs.push_back({InputType::ATTACK, getMouseWorldPosition(world, window)});
        }
        else if(mouse->button == sf::Mouse::Button::Right)
        {
            inputs.push_back({InputType::USE, getMouseWorldPosition(world, window)});
        }
    }
    else if(event.is<sf::Event::MouseWheelScrolled>())
    {
        auto wheel = event.getIf<sf::Event::MouseWheelScrolled>();
        int delta = -(static_cast<int>(wheel->delta));
        selectedSlot = static_cast<uint8_t>((selectedSlot + delta + 9) % 9);
        inputs.push_back({InputType::CHANGE_SLOT, selectedSlot});
    }
    else if(event.is<sf::Event::KeyPressed>())
    {
        auto key = event.getIf<sf::Event::KeyPressed>();

        if(key->code >= sf::Keyboard::Key::Num1 && key->code <= sf::Keyboard::Key::Num9)
        {
            uint8_t slot = static_cast<uint8_t>(static_cast<int>(key->code) - static_cast<int>(sf::Keyboard::Key::Num1));
            selectedSlot = slot;
            inputs.push_back({InputType::CHANGE_SLOT, slot});
        }
    }

    return inputs;
}
