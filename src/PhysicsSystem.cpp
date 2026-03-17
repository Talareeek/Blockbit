#include "../include/PhysicsSystem.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/World.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/TransformComponent.hpp"

#include <cmath>

void PhysicsSystem(std::vector<Entity>& entities, World& world, float deltaTime)
{
    for(auto& entity : entities)
    {
        if(!entity.hasComponent<PhysicsComponent>()) continue;

        auto& c = entity.getComponent<PhysicsComponent>();

        // ---- GRAVITY ----
        if(c.isGravityActive)
            c.force.y -= 32.0f * c.mass; // większa żeby było stabilniej

        sf::Vector2f acceleration = c.force / c.mass;
        c.velocity += acceleration * deltaTime;
        if(c.velocity.y < -PhysicsComponent::TERMINAL_VELOCITY)
            c.velocity.y = -PhysicsComponent::TERMINAL_VELOCITY;

        c.velocity.x *= std::exp(-PhysicsComponent::damping.x * deltaTime);
        c.velocity.y *= std::exp(-PhysicsComponent::damping.y * deltaTime);

        c.onGround = false;

        if(!entity.hasComponent<TransformComponent>()) continue;

        auto& transform = entity.getComponent<TransformComponent>();


        // ======================================================
        // ===================== X AXIS =========================
        // ======================================================

        transform.position.x += c.velocity.x * deltaTime;

        float left   = transform.position.x;
        float right  = transform.position.x + transform.size.x;
        float bottom = transform.position.y;
        float top    = transform.position.y + transform.size.y;

        if (c.velocity.x > 0) // RIGHT
        {
            int tileX = (int)std::floor(right - 0.001f);

            for (int y = (int)std::floor(bottom); y <= (int)std::floor(top - 0.001f); y++)
            {
                if (tileX < -10000) continue; // safety

                if (blockDatabase[world.getBlock(tileX, y)].solid)
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

            for (int y = (int)std::floor(bottom); y <= (int)std::floor(top - 0.001f); y++)
            {
                if (tileX < -10000) continue;

                if (blockDatabase[world.getBlock(tileX, y)].solid)
                {
                    transform.position.x = tileX + 1.0f;
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
            int tileY = (int)std::floor(top - 0.001f);

            for (int x = (int)std::floor(left); x <= (int)std::floor(right - 0.001f); x++)
            {
                if (blockDatabase[world.getBlock(x, tileY)].solid)
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

            for (int x = (int)std::floor(left); x <= (int)std::floor(right - 0.001f); x++)
            {
                if (blockDatabase[world.getBlock(x, tileY)].solid)
                {
                    transform.position.y = tileY + 1.0f;
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