#include "../include/World.hpp"
#include "../include/PlayerComponent.hpp"

void PlayerInputSystem(World& world, float dt)
{
    for(auto& entity : world.getEntities())
    {
        if(!entity.hasComponent<PlayerComponent>() || !entity.hasComponent<PhysicsComponent>() || !entity.hasComponent<RenderComponent>()) continue;

        auto player = entity.getComponent<PlayerComponent>();

        if(!player.ableToMove) continue;

        
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) && entity.getComponent<PhysicsComponent>().onGround)
        {
            auto& component = entity.getComponent<PhysicsComponent>();
            component.velocity.y += 10.0f;
        }
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
        {
            auto& component = entity.getComponent<PhysicsComponent>();
            component.force.x -= 45.0f;

            auto& render = entity.getComponent<RenderComponent>();
            render.uv = {{0, 32}, {16, 16}};
        }
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
        {
            auto& component = entity.getComponent<PhysicsComponent>();
            component.force.x += 45.0f;

            auto& render = entity.getComponent<RenderComponent>();
            render.uv = {{32, 32}, {16, 16}};
        }


    }
}