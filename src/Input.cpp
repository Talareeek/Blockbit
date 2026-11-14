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
#include "../include/StaminaComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/InventoryView.hpp"
#include "../include/EffectsComponent.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>

#include <iostream>
#include <cmath>
#include <limits>
#include <random>


namespace
{
    // Returns the magnitude of the given effect on the entity, or 0 if the
    // effect isn't currently active. Looked up each input tick so Magic
    // Bottle buffs apply/retract seamlessly as the effect expires.
    uint32_t effectMagnitude(const Entity& entity, EffectID id)
    {
        if (!entity.hasComponent<EffectsComponent>()) return 0;

        const auto& effects = entity.getComponent<EffectsComponent>();
        for (const auto& e : effects.effects)
        {
            if (e.id == id) return e.magnitude;
        }
        return 0;
    }
}

std::vector<Input> getInputs(const World& world, sf::Vector2<double> camera, const sf::RenderWindow& window)
{
    std::vector<Input> inputs;

    const bool sprint = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl)
                     || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
    {
        inputs.push_back({InputType::MOVE, MoveData{{-1.0, 0.0}, sprint}});
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
    {
        inputs.push_back({InputType::MOVE, MoveData{{1.0, 0.0}, sprint}});
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

void applyLocalPlayerInputPrediction(World& world, const std::vector<Input>& inputs, UUID id, float dt)
{
    if(!world.doesEntityExist(id)) return;
    auto& entity = world.getEntity(id);

    if(!entity.hasComponent<PhysicsComponent>() ||
       !entity.hasComponent<TransformComponent>() ||
       !entity.hasComponent<AnimationComponent>() ||
       !entity.hasComponent<PlayerControlledComponent>()) return;

    auto& physics = entity.getComponent<PhysicsComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& player = entity.getComponent<PlayerControlledComponent>();

    // Mirror processWorldInputs: MOVE is stateful on the component; this
    // batch's last MOVE becomes the current intent, or clear if absent.
    {
        bool found_move = false;
        MoveData latest_move{};
        for(const auto& input : inputs)
        {
            if(input.type == InputType::MOVE)
            {
                latest_move = std::get<MoveData>(input.value);
                found_move = true;
            }
        }
        if(found_move)
        {
            player.move_intent = {static_cast<float>(latest_move.direction.x),
                                  static_cast<float>(latest_move.direction.y)};
            player.move_sprint = latest_move.sprint;
        }
        else
        {
            player.move_intent = {0.0f, 0.0f};
            player.move_sprint = false;
        }
    }

    // Client-side stamina drain prediction (mirrors server StaminaSystem one
    // tick). Done once per tick here — NOT inside applyMoveIntent, because
    // applyMoveIntent also runs on the server where StaminaSystem is already
    // the authoritative drain path and a second drain here would double it.
    bool predicted_sprint_this_tick = false;
    if(std::abs(player.move_intent.x) > 0.01f && player.move_sprint
       && entity.hasComponent<StaminaComponent>())
    {
        const auto& stamina = entity.getComponent<StaminaComponent>();
        const float gate = (stamina.since_last_drain <= 0.0f)
                           ? 0.0f
                           : StaminaComponent::MIN_TO_START_SPRINT;
        if(stamina.stamina > gate) predicted_sprint_this_tick = true;
    }

    for(const auto& input : inputs)
    {
        switch(input.type)
        {
            case InputType::MOVE:
            {
                // Handled above as stateful intent; nothing to do per-input.
                break;
            }
            case InputType::JUMP:
            {
                const float jump_mul = 1.0f + 0.30f * static_cast<float>(effectMagnitude(entity, EffectID::JumpBoost));

                if(physics.onGround)
                {
                    physics.velocity.y += 10.0f * jump_mul;
                    physics.onGround = false;
                }
                else if(isSubmerged(world, transform))
                {
                    physics.force.y += 60.0f * jump_mul;
                }

                break;
            }
            default:
                break;
        }
    }

    if(entity.hasComponent<StaminaComponent>())
    {
        auto& stamina = entity.getComponent<StaminaComponent>();
        if(predicted_sprint_this_tick && stamina.stamina > 0.0f)
        {
            stamina.stamina = std::max(0.0f, stamina.stamina - StaminaComponent::DRAIN_RATE * dt);
            stamina.since_last_drain = 0.0f;
        }
        else if(stamina.since_last_drain < StaminaComponent::REGEN_DELAY)
        {
            stamina.since_last_drain += dt;
        }
    }
}

void applyMoveIntent(World& world, UUID id, float /*dt*/)
{
    if(!world.doesEntityExist(id)) return;
    auto& entity = world.getEntity(id);

    if(!entity.hasComponent<PhysicsComponent>() ||
       !entity.hasComponent<TransformComponent>() ||
       !entity.hasComponent<AnimationComponent>() ||
       !entity.hasComponent<PlayerControlledComponent>()) return;

    auto& physics = entity.getComponent<PhysicsComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& animation = entity.getComponent<AnimationComponent>();
    auto& player = entity.getComponent<PlayerControlledComponent>();

    const float dir_x = player.move_intent.x;
    if(std::abs(dir_x) <= 0.01f) return;

    float multiplier = 0.75f;
    if(player.move_sprint && entity.hasComponent<StaminaComponent>())
    {
        auto& stamina = entity.getComponent<StaminaComponent>();
        const float gate = (stamina.since_last_drain <= 0.0f)
                           ? 0.0f
                           : StaminaComponent::MIN_TO_START_SPRINT;
        if(stamina.stamina > gate)
        {
            multiplier = 1.25f;
            stamina.sprint_requested = true;
        }
    }

    multiplier *= 1.0f + 0.25f * static_cast<float>(effectMagnitude(entity, EffectID::Speed));

    int block_x = (int)std::floor(transform.position.x + transform.size.x / 2.0f);
    int block_y = (int)std::floor(transform.position.y);
    physics.force.x += 45.0f * dir_x * multiplier / blockDatabase[world.getBlock(block_x, block_y).id].drag;
    physics.dirty = true;

    if(dir_x < 0.0f)
    {
        animation.direction = AnimationComponent::Direction::Left;
        animation.dirty = true;
    }
    else
    {
        animation.direction = AnimationComponent::Direction::Right;
        animation.dirty = true;
    }
}

void processWorldInputs(World& world, std::vector<Input> inputs, UUID id)
{
    auto& entity = world.getEntity(id);

    if(!entity.hasComponent<PhysicsComponent>() || !entity.hasComponent<RenderComponent>() || !entity.hasComponent<TransformComponent>() || !entity.hasComponent<InventoryComponent>() || !entity.hasComponent<PlayerControlledComponent>()) return;

    auto& physics = entity.getComponent<PhysicsComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& player = entity.getComponent<PlayerControlledComponent>();
    auto& animation = entity.getComponent<AnimationComponent>();
    auto& inventory = entity.getComponent<InventoryComponent>();

    // Stateful MOVE: this batch's intent is the last MOVE it contains, or
    // "no movement" if it contains none. applyMoveIntent (called every tick
    // from GameServer::update / ClientGameState::onTick) converts the stored
    // intent into force. This keeps server movement smooth even when batches
    // arrive in jittery bursts — the previous behavior of adding force
    // per-MOVE here meant 0 batches/tick → stop and 2 batches/tick → lurch.
    {
        bool found_move = false;
        MoveData latest_move{};
        for(const auto& input : inputs)
        {
            if(input.type == InputType::MOVE)
            {
                latest_move = std::get<MoveData>(input.value);
                found_move = true;
            }
        }
        if(found_move)
        {
            player.move_intent = {static_cast<float>(latest_move.direction.x),
                                  static_cast<float>(latest_move.direction.y)};
            player.move_sprint = latest_move.sprint;
        }
        else
        {
            player.move_intent = {0.0f, 0.0f};
            player.move_sprint = false;
        }
    }

    for(const auto& input : inputs)
    {
        switch(input.type)
        {
            case InputType::MOVE:
            {
                // Handled above as stateful intent; nothing to do per-input.
                break;
            }
            case InputType::JUMP:
            {
                if(player.noclip)
                {
                    player.jump = true;
                    player.dirty = true;
                    break;
                }

                const float jump_mul = 1.0f + 0.30f * static_cast<float>(effectMagnitude(entity, EffectID::JumpBoost));

                if(physics.onGround)
                {
                    physics.velocity.y += 10.0f * jump_mul;
                    physics.onGround = false;
                }
                else if(isSubmerged(world, transform))
                {
                    physics.force.y += 60.0f * jump_mul;
                }
                physics.dirty = true;

                break;
            }
            case InputType::ATTACK_START:
            {
                player.mid_attack = true;
                player.dirty = true;

                const auto& held = inventory.inventory.slots[inventory.selectedSlot];
                const ItemData& held_data = itemDatabase[held.itemID];

                if(player.since_last_attack < held_data.attack_delay) break;

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

                player.since_last_attack = 0.0f;

                if(has_best)
                {
                    auto& target_entity = world.getEntity(best_id);
                    auto& target_health = target_entity.getComponent<HealthComponent>();
                    auto& target_transform = target_entity.getComponent<TransformComponent>();

                    uint32_t hit_damage;
                    if(held_data.damage.has_value())
                    {
                        hit_damage = *held_data.damage;
                    }
                    else
                    {
                        static thread_local std::mt19937 rng{std::random_device{}()};
                        std::uniform_int_distribution<uint32_t> dist(4, 5);
                        hit_damage = dist(rng);
                    }

                    target_health.health = (target_health.health > hit_damage)
                                           ? target_health.health - hit_damage
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

                        const double strength = static_cast<double>((MAX_HIT_DISTANCE - best_distance) * HIT_KNOCKBACK_SCALE);

                        target_physics.velocity.x += direction.x * strength;
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
