#include "../include/RenderSystem.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/MainGameState.hpp"

#include <cmath>

float worldToScreenY(float worldY, float unit_size, float windowHeight)
{
    return windowHeight - worldY * unit_size;
}

float screenToWorldY(float screenY, float unit_size, float windowHeight)
{
    return (windowHeight - screenY) / unit_size;
}

void RenderSystem(std::vector<Entity>& entities, sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / MainGameState::UNIT_SIZE_FACTOR;

    for(auto& entity : entities)
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

        sprite.setPosition({static_cast<float>(transform.position.x * unit_size), static_cast<float>(transform.position.y * unit_size)});
        window.draw(sprite);
    }
}