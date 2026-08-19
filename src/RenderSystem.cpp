#include "../include/RenderSystem.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/GameCommon.hpp"
#include "../include/World.hpp"

#include <cmath>

float worldToScreenY(float worldY, float unit_size, float windowHeight)
{
    return windowHeight - worldY * unit_size;
}

float screenToWorldY(float screenY, float unit_size, float windowHeight)
{
    return (windowHeight - screenY) / unit_size;
}

void RenderEntities(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / WORLD_UNIT_SIZE_FACTOR;

    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<RenderComponent>() || !entity.hasComponent<TransformComponent>()) continue;

        auto& component = entity.getComponent<RenderComponent>();

        sf::Sprite sprite(AssetManager::getTexture(component.textureID), component.uv);

        const float scaleX = (component.size.x * unit_size) / sprite.getTextureRect().size.x;
        const float scaleY = -((component.size.y * unit_size) / sprite.getTextureRect().size.y);

        bool flipX = false;
        if(entity.hasComponent<AnimationComponent>())
        {
            flipX = entity.getComponent<AnimationComponent>().direction == AnimationComponent::Direction::Right;
        }

        sprite.setScale({flipX ? -scaleX : scaleX, scaleY});

        sprite.setOrigin({flipX ? static_cast<float>(sprite.getTextureRect().size.x) : 0.f, static_cast<float>(sprite.getTextureRect().size.y)});

        auto& transform = entity.getComponent<TransformComponent>();

        sf::Vector2f screen_position = {static_cast<float>(transform.render_position.x - camera.x), static_cast<float>(transform.render_position.y - camera.y)};

        sprite.setPosition({static_cast<float>(screen_position.x * unit_size), static_cast<float>(screen_position.y * unit_size)});
        window.draw(sprite);
    }
}