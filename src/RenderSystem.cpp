#include "../include/RenderSystem.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/HealthComponent.hpp"

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
    unsigned int unit_size = window.getSize().y / 9;

    for(auto& entity : entities)
    {
        if(!entity.hasComponent<RenderComponent>() || !entity.hasComponent<TransformComponent>()) continue;

        auto& component = entity.getComponent<RenderComponent>();

        sf::Sprite sprite(AssetManager::getTexture(component.textureID), component.uv);
        sprite.setScale({
            (component.size.x * unit_size) / sprite.getTextureRect().size.x,
            (component.size.y * unit_size) / sprite.getTextureRect().size.y
        });


        auto& transform = entity.getComponent<TransformComponent>();

        sprite.setPosition({
            transform.position.x * unit_size,
            window.getSize().y - (transform.position.y * unit_size)
        });
        window.draw(sprite);
    }
}