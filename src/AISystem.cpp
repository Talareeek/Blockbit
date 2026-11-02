#include "../include/AISystem.hpp"
#include "../include/AIComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/Block.hpp"

#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace
{
    float frand01()
    {
        static thread_local std::mt19937 rng(std::random_device{}());
        static thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng);
    }

    bool blockSolidAt(World& world, int x, int y)
    {
        return blockDatabase[world.getBlock(x, y).id].solid;
    }

    // Full-body climb test: there is a 1-block step in `dir` that the mob can
    // actually land on without smacking its head into a ceiling.
    bool obstacleClimbable(World& world,
                           const TransformComponent& t,
                           int dir)
    {
        if (dir == 0) return false;

        const double left   = t.position.x;
        const double right  = t.position.x + t.size.x;
        const double bottom = t.position.y;
        const double top    = bottom + t.size.y;

        const int probeX = (dir > 0)
            ? static_cast<int>(std::floor(right  + 0.05))
            : static_cast<int>(std::floor(left   - 0.05));
        const int feetY = static_cast<int>(std::floor(bottom));

        if (!blockSolidAt(world, probeX, feetY)) return false;

        const int body_height = std::max(1, static_cast<int>(std::ceil(top - bottom)));
        for (int dy = 1; dy <= body_height; ++dy)
            if (blockSolidAt(world, probeX, feetY + dy)) return false;

        return true;
    }

    // Two-block drop directly ahead — a wandering passive would plunge if it
    // kept walking. Hostile mobs ignore this when chasing.
    bool pitAhead(World& world, const TransformComponent& t, int dir)
    {
        if (dir == 0) return false;

        const double left  = t.position.x;
        const double right = t.position.x + t.size.x;

        const int probeX = (dir > 0)
            ? static_cast<int>(std::floor(right + 0.05))
            : static_cast<int>(std::floor(left  - 0.05));
        const int feetY = static_cast<int>(std::floor(t.position.y));

        return !blockSolidAt(world, probeX, feetY - 1)
            && !blockSolidAt(world, probeX, feetY - 2);
    }

    // Sub-tile stepped ray between two world-space points. We only need to
    // know if any solid block sits on the line — this is cheap because mob
    // detection range is small (≤ ~16 blocks).
    bool hasLineOfSight(World& world,
                        sf::Vector2<double> from,
                        sf::Vector2<double> to)
    {
        const double dx = to.x - from.x;
        const double dy = to.y - from.y;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len <= 1e-4) return true;

        const int steps = static_cast<int>(std::ceil(len * 4.0));
        const double inv = 1.0 / steps;
        int last_x = std::numeric_limits<int>::min();
        int last_y = std::numeric_limits<int>::min();

        for (int i = 1; i < steps; ++i)
        {
            const double tt = i * inv;
            const int bx = static_cast<int>(std::floor(from.x + dx * tt));
            const int by = static_cast<int>(std::floor(from.y + dy * tt));
            if (bx == last_x && by == last_y) continue;
            last_x = bx; last_y = by;
            if (blockSolidAt(world, bx, by)) return false;
        }
        return true;
    }

    struct PlayerTarget
    {
        Entity*             entity;
        sf::Vector2<double> center;
    };

    std::vector<PlayerTarget> collectValidPlayers(World& world)
    {
        std::vector<PlayerTarget> out;
        for (auto& [id, entity] : world.getEntities())
        {
            if (!entity.hasComponent<PlayerControlledComponent>()) continue;
            if (!entity.hasComponent<TransformComponent>())        continue;
            if (!entity.hasComponent<HealthComponent>())           continue;

            const auto& pc = entity.getComponent<PlayerControlledComponent>();
            if (!pc.active) continue;

            const auto& hc = entity.getComponent<HealthComponent>();
            if (!hc.damageable || hc.health == 0) continue;

            out.push_back({&entity, entity.getComponent<TransformComponent>().center()});
        }
        return out;
    }

    PlayerTarget* pickNearest(std::vector<PlayerTarget>& players,
                              sf::Vector2<double> from,
                              double& out_dist)
    {
        out_dist = std::numeric_limits<double>::infinity();
        PlayerTarget* best = nullptr;
        for (auto& p : players)
        {
            const double ddx = p.center.x - from.x;
            const double ddy = p.center.y - from.y;
            const double d = std::sqrt(ddx * ddx + ddy * ddy);
            if (d < out_dist)
            {
                out_dist = d;
                best = &p;
            }
        }
        return best;
    }

    constexpr float AI_JUMP_VELOCITY       = 10.0f;
    constexpr float AI_KNOCKBACK_X         = 7.0f;
    constexpr float AI_KNOCKBACK_Y         = 6.0f;
    constexpr float AI_LOSE_SIGHT_TIME     = 5.0f;
    constexpr float AI_MAX_CHASE_RANGE_MUL = 2.0f;
}

void AISystem(ServerWorld& world, float dt)
{
    auto& entities = world.getEntities();
    auto players = collectValidPlayers(world);

    for (auto& [id, entity] : entities)
    {
        if (!entity.hasComponent<AIComponent>())        continue;
        if (!entity.hasComponent<TransformComponent>()) continue;
        if (!entity.hasComponent<PhysicsComponent>())   continue;

        auto& ai        = entity.getComponent<AIComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& physics   = entity.getComponent<PhysicsComponent>();

        if (!world.isChunkLoaded(transform.chunkPosition())) continue;

        ai.stateTimer    += dt;
        ai.attackCooldown = std::max(0.0f, ai.attackCooldown - dt);

        const AIComponent::State old_state = ai.state;

        double distToPlayer = std::numeric_limits<double>::infinity();
        PlayerTarget* target = pickNearest(players, transform.center(), distToPlayer);

        sf::Vector2<double> toPlayer{0.0, 0.0};
        bool has_los = false;
        if (target != nullptr)
        {
            sf::Vector2<double> diff = target->center - transform.center();
            if (distToPlayer > 1e-4) toPlayer = diff / distToPlayer;
            has_los = hasLineOfSight(world, transform.center(), target->center);
        }

        // ---------- STATE TRANSITIONS ----------
        if (ai.personality == AIComponent::Personality::Aggressive)
        {
            const bool  in_detect = target && distToPlayer <= ai.detectionRange && has_los;
            const float max_chase = ai.detectionRange * AI_MAX_CHASE_RANGE_MUL;
            const bool  in_chase  = target && distToPlayer <= max_chase;

            if (in_detect)
            {
                ai.state           = AIComponent::State::Chasing;
                ai.sinceTargetSeen = 0.0f;
            }
            else if (ai.state == AIComponent::State::Chasing)
            {
                ai.sinceTargetSeen += dt;
                if (!in_chase || ai.sinceTargetSeen >= AI_LOSE_SIGHT_TIME)
                {
                    ai.state           = AIComponent::State::Idle;
                    ai.stateTimer      = 0.0f;
                    ai.sinceTargetSeen = 0.0f;
                }
            }
        }
        else // Passive
        {
            if (target != nullptr && distToPlayer <= ai.fleeRange && has_los)
            {
                ai.state = AIComponent::State::Fleeing;
            }
            else if (ai.state == AIComponent::State::Fleeing)
            {
                ai.state      = AIComponent::State::Idle;
                ai.stateTimer = 0.0f;
            }
        }

        // idle <-> wandering cycle (only when not pursuing / fleeing)
        if (ai.state != AIComponent::State::Chasing
            && ai.state != AIComponent::State::Fleeing
            && ai.stateTimer >= ai.stateDuration)
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
        int  animDir   = 0;
        bool moved     = false;

        switch (ai.state)
        {
            case AIComponent::State::Idle:
                break;

            case AIComponent::State::Wandering:
            {
                int dir = ai.wanderDir;
                if (dir != 0 && physics.onGround && pitAhead(world, transform, dir))
                {
                    dir = -dir;
                    ai.wanderDir = dir;
                }
                physics.force.x += ai.moveSpeed * static_cast<float>(dir);
                if (physics.onGround && obstacleClimbable(world, transform, dir))
                    physics.velocity.y = std::max(physics.velocity.y, AI_JUMP_VELOCITY);
                animDir = dir;
                moved   = true;
                break;
            }

            case AIComponent::State::Chasing:
            {
                if (target == nullptr) break;

                const int dir = (toPlayer.x >= 0.0) ? 1 : -1;
                physics.force.x += static_cast<float>(dir) * ai.moveSpeed;

                if (physics.onGround && obstacleClimbable(world, transform, dir))
                    physics.velocity.y = std::max(physics.velocity.y, AI_JUMP_VELOCITY);

                // If the target is clearly above us and the block over our head
                // is open, try a hop even without a side obstacle — otherwise
                // a mob chasing a player who climbs a ledge gets stuck below.
                if (physics.onGround && toPlayer.y > 0.6)
                {
                    const int headY = static_cast<int>(
                        std::floor(transform.position.y + transform.size.y + 0.1));
                    const int bodyX = static_cast<int>(std::floor(transform.center().x));
                    if (!blockSolidAt(world, bodyX, headY))
                        physics.velocity.y = std::max(physics.velocity.y, AI_JUMP_VELOCITY);
                }

                if (distToPlayer <= ai.attackRange && has_los && ai.attackCooldown <= 0.0f)
                {
                    Entity* te = target->entity;
                    if (te->hasComponent<HealthComponent>() && te->hasComponent<PhysicsComponent>())
                    {
                        auto& hp = te->getComponent<HealthComponent>();
                        if (hp.damageable && hp.health > 0)
                        {
                            hp.health = (hp.health > AIComponent::ATTACK_DAMAGE)
                                        ? hp.health - AIComponent::ATTACK_DAMAGE
                                        : 0;
                            hp.since_last_damaged = 0.0f;
                            hp.dirty = true;

                            auto& tp = te->getComponent<PhysicsComponent>();
                            tp.velocity.x += static_cast<float>(dir) * AI_KNOCKBACK_X;
                            tp.velocity.y  = std::max(tp.velocity.y, AI_KNOCKBACK_Y);
                            tp.dirty = true;
                        }
                    }
                    ai.attackCooldown = AIComponent::ATTACK_INTERVAL;
                }
                animDir = dir;
                moved   = true;
                break;
            }

            case AIComponent::State::Fleeing:
            {
                if (target == nullptr) break;

                int dir = (toPlayer.x >= 0.0) ? -1 : 1;
                if (physics.onGround && pitAhead(world, transform, dir))
                {
                    const int alt = -dir;
                    if (!pitAhead(world, transform, alt)) dir = alt;
                    else                                  dir = 0;
                }
                physics.force.x += static_cast<float>(dir) * ai.moveSpeed;
                if (dir != 0 && physics.onGround && obstacleClimbable(world, transform, dir))
                    physics.velocity.y = std::max(physics.velocity.y, AI_JUMP_VELOCITY);
                animDir = dir;
                moved   = true;
                break;
            }
        }

        if (moved) physics.dirty = true;

        if (entity.hasComponent<AnimationComponent>())
        {
            auto& animation = entity.getComponent<AnimationComponent>();
            const AnimationState prev_state = animation.currentState;
            const AnimationComponent::Direction prev_dir = animation.direction;

            if (animDir != 0)
            {
                animation.currentState = AnimationState::Walking;
                animation.direction = (animDir > 0)
                    ? AnimationComponent::Direction::Right
                    : AnimationComponent::Direction::Left;
            }
            else
            {
                animation.currentState = AnimationState::Idle;
            }

            if (prev_state != animation.currentState || prev_dir != animation.direction)
                animation.dirty = true;
        }

        if (old_state != ai.state) ai.dirty = true;
    }
}
