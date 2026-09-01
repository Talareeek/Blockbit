#include "../include/Input.hpp"
#include "../include/World.hpp"
#include "../include/Render.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/Block.hpp"
#include "../include/Item.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/PlayerControlledComponent.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>

#include <iostream>
#include <cmath>


std::vector<Input> getInputs(const World& world, sf::Vector2<double> camera, const sf::RenderWindow& window)
{
    std::vector<Input> inputs;

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
    {
        inputs.push_back({InputType::MOVE, sf::Vector2<double>{-1.0, 0.0}});
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
    {
        inputs.push_back({InputType::MOVE, sf::Vector2<double>{1.0, 0.0}});
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
    {
        inputs.push_back({InputType::JUMP});
    }
    

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
    {
        inputs.push_back({InputType::DROP, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)});
    }

    return inputs;
}

std::vector<Input> getInputsFromEvent(const sf::Event& event, sf::Vector2<double> camera, const sf::RenderWindow& window, uint8_t& selectedSlot)
{
    std::vector<Input> inputs;

    if(event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();

        if(mouse->button == sf::Mouse::Button::Left)
        {
            inputs.push_back({InputType::ATTACK_START});
        }
        else if(mouse->button == sf::Mouse::Button::Right)
        {
            inputs.push_back({InputType::USE_START});
        }
    }
    else if(event.is<sf::Event::MouseButtonReleased>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonReleased>();

        if(mouse->button == sf::Mouse::Button::Left)
        {
            inputs.push_back({InputType::ATTACK_STOP});
        }
        else if(mouse->button == sf::Mouse::Button::Right)
        {
            inputs.push_back({InputType::USE_STOP});
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

void processWorldInputs(World& world, std::vector<Input> inputs, UUID id)
{
    auto& entity = world.getEntity(id);

    if(!entity.hasComponent<PhysicsComponent>() || !entity.hasComponent<RenderComponent>() || !entity.hasComponent<TransformComponent>() || !entity.hasComponent<InventoryComponent>() || !entity.hasComponent<PlayerControlledComponent>()) return;

    auto& physics = entity.getComponent<PhysicsComponent>();
    auto& render = entity.getComponent<RenderComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& inventory = entity.getComponent<InventoryComponent>();
    auto& player = entity.getComponent<PlayerControlledComponent>();

    for(const auto& input : inputs)
    {
        switch(input.type)
        {
            case InputType::MOVE:
            {
                auto direction = std::get<sf::Vector2<double>>(input.value);

                int block_x = (int)std::floor(transform.position.x + transform.size.x / 2.0f);
                int block_y = (int)std::floor(transform.position.y);
                physics.force.x += 45.0f * direction.x / blockDatabase[world.getBlock(block_x, block_y).id].drag;

                if(direction.x < 0.0f)
                {
                    render.uv = {{0, 32}, {16, 16}};
                }
                else if(direction.x > 0.0f)
                {
                    render.uv = {{32, 32}, {16, 16}};
                }

                break;
            }
            case InputType::JUMP:
            {
                if(physics.onGround)
                {
                    physics.velocity.y += 10.0f;
                }
                else if(isSubmerged(world, transform))
                {
                    physics.force.y += 60.0f;
                }

                break;
            }
            case InputType::ATTACK_START:
            {
                player.mid_attack = true;

                break;
            }
            case InputType::USE_START:
            {
                player.mid_usage = true;

                break;
            }
            case InputType::ATTACK_STOP:
            {
                player.mid_attack = false;

                break;
            }
            case InputType::USE_STOP:
            {
                player.mid_usage = false;

                break;
            }
            case InputType::DROP:
            {
                player.drops = true;
                player.drops_full_stack = std::get<bool>(input.value);

                break;
            }
            case InputType::CHANGE_SLOT:
            {
                uint8_t slot = std::get<uint8_t>(input.value);
                inventory.selectedSlot = slot % 9;

                break;
            }
        }
    }
}
