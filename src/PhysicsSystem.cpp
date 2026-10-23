#include "../include/PhysicsSystem.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/World.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/TransformComponent.hpp"

#include <cmath>
#include <algorithm>

namespace
{
    constexpr double GRAVITY            = 32.0;
    constexpr double LIQUID_GRAVITY_K   = 0.25;
    constexpr double LIQUID_BUOYANCY    = 5.0;
    constexpr double LIQUID_FALL_LIMIT  = 10.0;
    constexpr double LIQUID_RISE_LIMIT  = 3.0;

    constexpr double MAX_SUB_STEP = 0.5;
    constexpr double EDGE_EPS     = 1e-9;

    inline int ifloor(double v) noexcept
    {
        int i = static_cast<int>(v);
        return (v < static_cast<double>(i)) ? i - 1 : i;
    }
}

bool isSubmerged(World& world, const TransformComponent& transform)
{
    int cx = ifloor(transform.position.x + transform.size.x * 0.5);
    int cy = ifloor(transform.position.y + transform.size.y * 0.5);
    return blockDatabase[world.getBlock(cx, cy).id].liquid;
}

void PhysicsSystem(World& world, float deltaTime)
{
    const double dt = static_cast<double>(deltaTime);
    if(dt <= 0.0) return;

    const double damping_x_mult = std::exp(-static_cast<double>(PhysicsComponent::damping.x) * dt);
    const double damping_y_base = -static_cast<double>(PhysicsComponent::damping.y) * dt;

    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<PhysicsComponent>() || !entity.hasComponent<TransformComponent>()) continue;

        auto& c = entity.getComponent<PhysicsComponent>();
        if(c.isStatic) continue;

        auto& t = entity.getComponent<TransformComponent>();

        if(!world.isChunkLoaded(t.chunkPosition())) continue;

        const double size_x = t.size.x;
        const double size_y = t.size.y;

        const int probe_x = ifloor(t.position.x + size_x * 0.5);
        const int probe_y = ifloor(t.position.y + size_y * 0.5);
        const auto& probe_data = blockDatabase[world.getBlock(probe_x, probe_y).id];
        const bool inLiquid = probe_data.liquid;
        const double drag   = inLiquid ? static_cast<double>(probe_data.drag) : 1.0;

        if(c.isGravityActive)
        {
            double g = -GRAVITY * c.mass * (inLiquid ? LIQUID_GRAVITY_K : 1.0);
            if(inLiquid) g += LIQUID_BUOYANCY * c.mass;
            c.force.y += static_cast<float>(g);
        }

        const double inv_mass = 1.0 / static_cast<double>(c.mass);
        double vx = static_cast<double>(c.velocity.x) + static_cast<double>(c.force.x) * inv_mass * dt;
        double vy = static_cast<double>(c.velocity.y) + static_cast<double>(c.force.y) * inv_mass * dt;

        const double fallLimit = inLiquid ? LIQUID_FALL_LIMIT : static_cast<double>(PhysicsComponent::TERMINAL_VELOCITY);
        if(vy < -fallLimit) vy = -fallLimit;
        if(inLiquid && vy > LIQUID_RISE_LIMIT) vy = LIQUID_RISE_LIMIT;

        vx *= damping_x_mult;
        vy *= std::exp(damping_y_base * drag);

        c.onGround = false;

        double dx_total = vx * dt;
        double dy_total = vy * dt;

        int substeps = 1;
        {
            const double worst = std::max(std::fabs(dx_total), std::fabs(dy_total));
            if(worst > MAX_SUB_STEP) substeps = static_cast<int>(std::ceil(worst / MAX_SUB_STEP));
        }
        const double inv_steps = 1.0 / substeps;
        double dx = dx_total * inv_steps;
        double dy = dy_total * inv_steps;

        double fall_impact_v = 0.0;
        bool hit_ground = false;

        for(int step = 0; step < substeps; ++step)
        {
            // ===================== X AXIS =====================
            if(dx != 0.0)
            {
                const double prev_x = t.position.x;
                t.position.x += dx;

                const double left   = t.position.x;
                const double right  = left + size_x;
                const double bottom = t.position.y;
                const double top    = bottom + size_y;

                const int y0 = ifloor(bottom);
                const int y1 = ifloor(top - EDGE_EPS);

                if(dx > 0.0)
                {
                    const int tx = ifloor(right - EDGE_EPS);
                    // Only block if we actually CROSSED into this tile this step.
                    // (prev_right <= tx means the right edge was at/left of tile tx's left boundary before moving.)
                    const double prev_right = prev_x + size_x;
                    if(prev_right <= static_cast<double>(tx))
                    {
                        for(int y = y0; y <= y1; ++y)
                        {
                            if(blockDatabase[world.getBlock(tx, y).id].solid)
                            {
                                t.position.x = static_cast<double>(tx) - size_x;
                                vx = 0.0;
                                dx = 0.0;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    const int tx = ifloor(left);
                    // Only block if prev_left was at or past tx's right boundary.
                    const double prev_left = prev_x;
                    if(prev_left >= static_cast<double>(tx + 1))
                    {
                        for(int y = y0; y <= y1; ++y)
                        {
                            if(blockDatabase[world.getBlock(tx, y).id].solid)
                            {
                                t.position.x = static_cast<double>(tx + 1);
                                vx = 0.0;
                                dx = 0.0;
                                break;
                            }
                        }
                    }
                }
            }

            // ===================== Y AXIS =====================
            if(dy != 0.0)
            {
                const double prev_y = t.position.y;
                t.position.y += dy;

                const double left   = t.position.x;
                const double right  = left + size_x;
                const double bottom = t.position.y;
                const double top    = bottom + size_y;

                const int x0 = ifloor(left);
                const int x1 = ifloor(right - EDGE_EPS);

                if(dy > 0.0)
                {
                    const int ty = ifloor(top - EDGE_EPS);
                    const double prev_top = prev_y + size_y;
                    if(prev_top <= static_cast<double>(ty))
                    {
                        for(int x = x0; x <= x1; ++x)
                        {
                            if(blockDatabase[world.getBlock(x, ty).id].solid)
                            {
                                t.position.y = static_cast<double>(ty) - size_y;
                                vy = 0.0;
                                dy = 0.0;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    const int ty = ifloor(bottom);
                    const double prev_bottom = prev_y;
                    if(prev_bottom >= static_cast<double>(ty + 1))
                    {
                        for(int x = x0; x <= x1; ++x)
                        {
                            if(blockDatabase[world.getBlock(x, ty).id].solid)
                            {
                                if(!hit_ground)
                                {
                                    fall_impact_v = -vy;
                                    hit_ground = true;
                                }
                                t.position.y = static_cast<double>(ty + 1);
                                vy = 0.0;
                                dy = 0.0;
                                c.onGround = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if(hit_ground && fall_impact_v > PhysicsComponent::SAFE_FALL_VELOCITY && entity.hasComponent<HealthComponent>())
        {
            auto& health = entity.getComponent<HealthComponent>();
            uint32_t damage = static_cast<uint32_t>((fall_impact_v - PhysicsComponent::SAFE_FALL_VELOCITY) * PhysicsComponent::FALL_DAMAGE_MULTIPLIER);
            health.health = (health.health > damage) ? health.health - damage : 0;
            health.since_last_damaged = 0.0f;
            health.dirty = true;
        }

        c.velocity.x = static_cast<float>(vx);
        c.velocity.y = static_cast<float>(vy);
        c.force = {0.0f, 0.0f};
        t.render_position = t.position;
        c.dirty = true;
        t.dirty = true;
    }
}
