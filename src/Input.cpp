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
#include "../include/HealthComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/InventoryView.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>

#include <iostream>
#include <cmath>
#include <limits>


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

std::vector<Input> getInputsFromEvent(const sf::Event& event, sf::Vector2<double> camera, const sf::RenderWindow& window)
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

        inputs.push_back({InputType::CHANGE_SLOT, ChangeSlotData{true, static_cast<int8_t>(delta)}});
    }

    
    else if(event.is<sf::Event::KeyPressed>())
    {
        auto key = event.getIf<sf::Event::KeyPressed>();

        if(key->code >= sf::Keyboard::Key::Num1 && key->code <= sf::Keyboard::Key::Num9)
        {
            uint8_t slot = static_cast<uint8_t>(static_cast<int>(key->code) - static_cast<int>(sf::Keyboard::Key::Num1));
            inputs.push_back({InputType::CHANGE_SLOT, ChangeSlotData{false, static_cast<int8_t>(slot)}});
        }
        else if(key->code == sf::Keyboard::Key::E)
        {
            inputs.push_back({InputType::OPEN_INVENTORY});
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
    auto& animation = entity.getComponent<AnimationComponent>();

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
                physics.dirty = true;

                if(direction.x < 0.0f)
                {
                    animation.direction = AnimationComponent::Direction::Left;
                    animation.dirty = true;
                }
                else if(direction.x > 0.0f)
                {
                    animation.direction = AnimationComponent::Direction::Right;
                    animation.dirty = true;
                }

                break;
            }
            case InputType::JUMP:
            {
                if(physics.onGround)
                {
                    physics.velocity.y += 10.0f;
                    physics.onGround = false;
                }
                else if(isSubmerged(world, transform))
                {
                    physics.force.y += 60.0f;
                }
                physics.dirty = true;

                break;
            }
            case InputType::ATTACK_START:
            {
                player.mid_attack = true;
                player.dirty = true;

                const sf::Vector2<double> cursor = player.cursor_position;

                UUID best_id{};
                bool has_best = false;
                float best_distance = std::numeric_limits<float>::infinity();

                for(auto& [target_id, target_entity] : world.getEntities())
                {
                    if(target_id == id) continue;
                    if(!target_entity.hasComponent<HealthComponent>()) continue;
                    if(!target_entity.hasComponent<TransformComponent>()) continue;

                    auto& target_health = target_entity.getComponent<HealthComponent>();
                    if(!target_health.damageable) continue;
                    if(target_health.health == 0) continue;

                    auto& target_transform = target_entity.getComponent<TransformComponent>();

                    const bool cursor_inside =
                        cursor.x >= target_transform.position.x &&
                        cursor.x <= target_transform.position.x + target_transform.size.x &&
                        cursor.y >= target_transform.position.y &&
                        cursor.y <= target_transform.position.y + target_transform.size.y;

                    if(!cursor_inside) continue;

                    const float distance = shortestDistance(transform, target_transform);
                    if(distance > MAX_HIT_DISTANCE) continue;

                    if(distance < best_distance)
                    {
                        best_distance = distance;
                        best_id = target_id;
                        has_best = true;
                    }
                }

                if(has_best)
                {
                    auto& target_entity = world.getEntity(best_id);
                    auto& target_health = target_entity.getComponent<HealthComponent>();
                    auto& target_transform = target_entity.getComponent<TransformComponent>();

                    target_health.health = (target_health.health > PLAYER_HIT_DAMAGE)
                                           ? target_health.health - PLAYER_HIT_DAMAGE
                                           : 0;
                    target_health.since_last_damaged = 0.0f;
                    target_health.dirty = true;

                    if(target_entity.hasComponent<PhysicsComponent>())
                    {
                        auto& target_physics = target_entity.getComponent<PhysicsComponent>();

                        const sf::Vector2<double> player_center = transform.center();
                        const sf::Vector2<double> target_center = target_transform.center();

                        sf::Vector2<double> direction = target_center - player_center;
                        const double length = std::sqrt(direction.x * direction.x + direction.y * direction.y);

                        if(length > 1e-6)
                        {
                            direction.x /= length;
                            direction.y /= length;
                        }
                        else
                        {
                            direction = {1.0, 0.0};
                        }

                        const float strength = (MAX_HIT_DISTANCE - best_distance) * HIT_KNOCKBACK_SCALE;

                        target_physics.velocity.x += static_cast<float>(direction.x) * strength;
                        target_physics.velocity.y += HIT_KNOCKBACK_VERTICAL;
                        target_physics.dirty = true;
                    }
                }

                break;
            }
            case InputType::USE_START:
            {
                player.mid_usage = true;
                player.dirty = true;

                break;
            }
            case InputType::ATTACK_STOP:
            {
                player.mid_attack = false;
                player.dirty = true;

                break;
            }
            case InputType::USE_STOP:
            {
                player.mid_usage = false;
                player.dirty = true;

                break;
            }
            case InputType::DROP:
            {
                player.drops = true;
                player.drops_full_stack = std::get<bool>(input.value);
                player.dirty = true;

                break;
            }
            case InputType::CHANGE_SLOT:
            {
                player.change_slot = std::get<ChangeSlotData>(input.value);
                player.dirty = true;

                break;
            }
            case InputType::OPEN_INVENTORY:
            {
                player.pending_view_opens.push_back(std::make_unique<ServerInventoryView>(id));
                player.dirty = true;

                break;
            }
        }
    }
}
