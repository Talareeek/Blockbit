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


std::vector<Input> getInputs(const World& world, const sf::RenderWindow& window)
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

    /*
    if(sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::X) != 0)
    {
        inputs.push_back({InputType::MOVE, sf::Vector2f{1.0f * sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::X) / 100.0f, 0.0f}});
    }
    */

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)/* || sf::Joystick::isButtonPressed(0, 0)*/)
    {
        inputs.push_back({InputType::JUMP, std::monostate{}});
    }
    

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)/* || sf::Joystick::isButtonPressed(0, 2)*/)
    {
        inputs.push_back({InputType::DROP, DropInfo{getMouseWorldPosition(world, window), sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)}});
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
            inputs.push_back({InputType::ATTACK_START, getMouseWorldPosition(camera, window)});
        }
        else if(mouse->button == sf::Mouse::Button::Right)
        {
            inputs.push_back({InputType::USE_START, getMouseWorldPosition(camera, window)});
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
    else if(event.is<sf::Event::MouseMoved>())
    {
        auto mouse = event.getIf<sf::Event::MouseMoved>();

        inputs.push_back({InputType::MOUSE_MOVE, getMouseWorldPosition(camera, window)});
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
            case InputType::MOUSE_MOVE:
            {
                player.cursor_position = std::get<sf::Vector2<double>>(input.value);

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
                auto& info = std::get<DropInfo>(input.value);
                auto& stack = inventory.inventory.slots[inventory.selectedSlot];

                if(stack.empty()) break;

                Entity item(generateUUID());
                item.addComponent(TransformComponent{transform.position + sf::Vector2<double>(0.0, 1.5), {0.5, 0.5}, sf::degrees(0.0f)});
                item.addComponent(PhysicsComponent{info.mousePosition - sf::Vector2f(transform.position), {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
                item.addComponent(RenderComponent{static_cast<unsigned short>(itemDatabase[stack.itemID].texture), {{0, 0}, {16, 16}}, {0.5f, 0.5f}});

                if(info.fullStack)
                {
                    item.addComponent(ItemComponent{stack});

                    stack = {ItemID::None, 0};
                }
                else
                {
                    item.addComponent(ItemComponent{{stack.itemID, 1}});

                    if(--stack.quantity == 0)
                    {
                        stack.itemID = ItemID::None;
                    }
                }

                world.addEntity(std::move(item));

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
