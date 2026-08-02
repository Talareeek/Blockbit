#include "../include/PhysicsSystem.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/World.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/TransformComponent.hpp"

#include <cmath>

bool isSubmerged(World& world, const TransformComponent& transform)
{
    int cx = (int)std::floor(transform.position.x + transform.size.x / 2.0f);
    int cy = (int)std::floor(transform.position.y + transform.size.y / 2.0f);

    return blockDatabase[world.getBlock(cx, cy).id].liquid;
}

void PhysicsSystem(World& world, float deltaTime)
{
    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<PhysicsComponent>()) continue;
        if(!entity.hasComponent<TransformComponent>()) continue;

        auto& c = entity.getComponent<PhysicsComponent>();
        auto& transform = entity.getComponent<TransformComponent>();

        bool inLiquid = isSubmerged(world, transform);

        // ---- GRAVITY ----
        if(c.isGravityActive)
        {
            float gravityScale = inLiquid ? 0.25f : 1.0f;
            c.force.y -= 32.0f * c.mass * gravityScale;

            if(inLiquid) c.force.y += 5.0f * c.mass;
        }

        sf::Vector2f acceleration = c.force / c.mass;
        c.velocity += acceleration * deltaTime;

        float terminalVelocity = inLiquid ? 10.0f : PhysicsComponent::TERMINAL_VELOCITY;
        if(c.velocity.y < -terminalVelocity)
            c.velocity.y = -terminalVelocity;
        if(inLiquid && c.velocity.y > 3.0f)
            c.velocity.y = 3.0f;

        float dragMultiplier = inLiquid ? blockDatabase[world.getBlock((int)std::floor(transform.position.x + transform.size.x / 2.0f), (int)std::floor(transform.position.y + transform.size.y / 2.0f)).id].drag : 1.0f;
        c.velocity.x *= std::exp(-PhysicsComponent::damping.x * deltaTime);
        c.velocity.y *= std::exp(-PhysicsComponent::damping.y * dragMultiplier * deltaTime);

        c.onGround = false;


        // ======================================================
        // ===================== X AXIS =========================
        // ======================================================

        transform.position.x += c.velocity.x * deltaTime;

        double left   = transform.position.x;
        double right  = transform.position.x + transform.size.x;
        double bottom = transform.position.y;
        double top    = transform.position.y + transform.size.y;

        if (c.velocity.x > 0) // RIGHT
        {
            int tileX = (int)std::floor(right - 0.001);

            for (int y = (int)std::floor(bottom); y <= (int)std::floor(top - 0.001); y++)
            {
                if (tileX < -10000) continue; // safety

                if (blockDatabase[world.getBlock(tileX, y).id].solid)
                {
                    transform.position.x = tileX - transform.size.x;
                    c.velocity.x = 0;
                    break;
                }
            }
        }
        else if (c.velocity.x < 0) // LEFT
        {
            int tileX = (int)std::floor(left);

            for (int y = (int)std::floor(bottom); y <= (int)std::floor(top - 0.001); y++)
            {
                if (tileX < -10000) continue;

                if (blockDatabase[world.getBlock(tileX, y).id].solid)
                {
                    transform.position.x = tileX + 1.0;
                    c.velocity.x = 0;
                    break;
                }
            }
        }

        // ======================================================
        // ===================== Y AXIS =========================
        // ======================================================

        transform.position.y += c.velocity.y * deltaTime;

        left   = transform.position.x;
        right  = transform.position.x + transform.size.x;
        bottom = transform.position.y;
        top    = transform.position.y + transform.size.y;

        if (c.velocity.y > 0) // UP
        {
            int tileY = (int)std::floor(top - 0.001);

            for (int x = (int)std::floor(left); x <= (int)std::floor(right - 0.001); x++)
            {
                if (blockDatabase[world.getBlock(x, tileY).id].solid)
                {
                    transform.position.y = tileY - transform.size.y;
                    c.velocity.y = 0;
                    break;
                }
            }
        }
        else if (c.velocity.y < 0) // DOWN
        {
            int tileY = (int)std::floor(bottom);

            for (int x = (int)std::floor(left); x <= (int)std::floor(right - 0.001); x++)
            {
                if (blockDatabase[world.getBlock(x, tileY).id].solid)
                {
                    transform.position.y = tileY + 1.0;
                    float fallVelocity = -c.velocity.y;
                    c.velocity.y = 0;
                    c.onGround = true;

                    if(entity.hasComponent<HealthComponent>())
                    {
                        auto& health = entity.getComponent<HealthComponent>();                        

                        if(fallVelocity > PhysicsComponent::SAFE_FALL_VELOCITY)
                        {
                            uint32_t damage = (uint32_t)((fallVelocity - PhysicsComponent::SAFE_FALL_VELOCITY) * PhysicsComponent::FALL_DAMAGE_MULTIPLIER);
                            health.health = health.health > damage ? health.health - damage : 0;
                        }
                    }
                    break;
                }
            }
        }

        c.force = {0.f, 0.f};
    }
}