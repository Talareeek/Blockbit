#include "../include/AISystem.hpp"
#include "../include/AIComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/PlayerComponent.hpp"
#include "../include/Block.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>

static float frand01()
{
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

static bool obstacleAhead(World& world, const TransformComponent& t, int dir)
{
    if (dir == 0) return false;

    const float left   = t.position.x;
    const float right  = t.position.x + t.size.x;
    const float bottom = t.position.y;

    const int probeX = (dir > 0)
        ? static_cast<int>(std::floor(right  + 0.05f))
        : static_cast<int>(std::floor(left   - 0.05f));
    const int probeY = static_cast<int>(std::floor(bottom));

    const bool solidAtFeet = blockDatabase[world.getBlock(probeX, probeY).id].solid;
    const bool airAbove    = !blockDatabase[world.getBlock(probeX, probeY + 1).id].solid;

    return solidAtFeet && airAbove;
}

static constexpr float AI_JUMP_VELOCITY = 10.0f;

void AISystem(World& world, float dt)
{
    auto& entities = world.getEntities();

    Entity* player = nullptr;
    for (auto& e : entities)
    {
        if (e.hasComponent<PlayerComponent>() && e.hasComponent<TransformComponent>())
        {
            player = &e;
            break;
        }
    }

    for (auto& entity : entities)
    {
        if (!entity.hasComponent<AIComponent>())        continue;
        if (!entity.hasComponent<TransformComponent>()) continue;
        if (!entity.hasComponent<PhysicsComponent>())   continue;

        auto& ai        = entity.getComponent<AIComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& physics   = entity.getComponent<PhysicsComponent>();

        ai.stateTimer    += dt;
        ai.attackCooldown = std::max(0.0f, ai.attackCooldown - dt);

        float distToPlayer = std::numeric_limits<float>::infinity();
        sf::Vector2f toPlayer{0.0f, 0.0f};

        if (player != nullptr)
        {
            const auto& pt = player->getComponent<TransformComponent>();
            sf::Vector2f diff = pt.position - transform.position;
            distToPlayer = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            if (distToPlayer > 0.0001f) toPlayer = diff / distToPlayer;
        }

        // ---------- STATE TRANSITIONS ----------
        if (ai.personality == AIComponent::Personality::Aggressive)
        {
            if (distToPlayer <= ai.detectionRange)
            {
                ai.state = AIComponent::State::Chasing;
            }
            else if (ai.state == AIComponent::State::Chasing)
            {
                ai.state      = AIComponent::State::Idle;
                ai.stateTimer = 0.0f;
            }
        }
        else // Passive
        {
            if (distToPlayer <= ai.fleeRange)
            {
                ai.state = AIComponent::State::Chasing; // used as "flee" for passives
            }
            else if (ai.state == AIComponent::State::Chasing)
            {
                ai.state      = AIComponent::State::Idle;
                ai.stateTimer = 0.0f;
            }
        }

        // idle <-> wandering cycle (only when not in Chasing)
        if (ai.state != AIComponent::State::Chasing && ai.stateTimer >= ai.stateDuration)
        {
            if (ai.state == AIComponent::State::Idle)
            {
                ai.state         = AIComponent::State::Wandering;
                ai.stateDuration = 1.0f + frand01() * 3.0f; // 1..4 s
                ai.wanderDir     = (frand01() < 0.5f) ? -1 : 1;
            }
            else
            {
                ai.state         = AIComponent::State::Idle;
                ai.stateDuration = 1.0f + frand01() * 2.0f; // 1..3 s
                ai.wanderDir     = 0;
            }
            ai.stateTimer = 0.0f;
        }

        // ---------- STATE ACTIONS ----------
        switch (ai.state)
        {
            case AIComponent::State::Idle:
                break;

            case AIComponent::State::Wandering:
                physics.force.x += ai.moveSpeed * static_cast<float>(ai.wanderDir);
                if (physics.onGround && obstacleAhead(world, transform, ai.wanderDir))
                    physics.velocity.y += AI_JUMP_VELOCITY;
                break;

            case AIComponent::State::Chasing:
            {
                if (player == nullptr) break;

                if (ai.personality == AIComponent::Personality::Aggressive)
                {
                    const int dir = (toPlayer.x >= 0.0f) ? 1 : -1;
                    physics.force.x += static_cast<float>(dir) * ai.moveSpeed;

                    if (physics.onGround && obstacleAhead(world, transform, dir))
                        physics.velocity.y += AI_JUMP_VELOCITY;

                    if (distToPlayer <= ai.attackRange && ai.attackCooldown <= 0.0f)
                    {
                        if (player->hasComponent<HealthComponent>())
                        {
                            auto& hp = player->getComponent<HealthComponent>();
                            hp.health = (hp.health > AIComponent::ATTACK_DAMAGE)
                                        ? hp.health - AIComponent::ATTACK_DAMAGE
                                        : 0;
                        }
                        ai.attackCooldown = AIComponent::ATTACK_INTERVAL;
                    }
                }
                else // Passive - flee
                {
                    const int dir = (toPlayer.x >= 0.0f) ? -1 : 1;
                    physics.force.x += static_cast<float>(dir) * ai.moveSpeed;

                    if (physics.onGround && obstacleAhead(world, transform, dir))
                        physics.velocity.y += AI_JUMP_VELOCITY;
                }
                break;
            }
        }
    }
}
