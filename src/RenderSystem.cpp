#include "../include/RenderSystem.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/GameCommon.hpp"
#include "../include/PlayerControlledComponent.hpp"
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

        sf::Sprite sprite(AssetManager::getGameTexture(component.textureID), component.uv);

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

        if(entity.hasComponent<HealthComponent>())
        {
            auto& health = entity.getComponent<HealthComponent>();

            if(health.since_last_damaged < HealthComponent::DAMAGE_FLASH_DURATION)
            {
                const float progress = health.since_last_damaged / HealthComponent::DAMAGE_FLASH_DURATION;
                const float intensity = 1.0f - std::clamp(progress, 0.0f, 1.0f);

                const uint8_t fade = static_cast<uint8_t>(std::round(255.0f * (1.0f - intensity)));

                sprite.setColor(sf::Color(255, fade, fade, 255));
            }
        }

        window.draw(sprite);

        if(entity.hasComponent<PlayerControlledComponent>())
        {
            auto& player = entity.getComponent<PlayerControlledComponent>();

            if(!player.nickname.empty())
            {
                unsigned int font_size = static_cast<unsigned int>(unit_size * 0.3f);
                if(font_size < 1) font_size = 1;

                sf::Text name_text(AssetManager::getFont(AssetManager::FontID::PressStart2P), player.nickname, font_size);

                sf::FloatRect bounds = name_text.getLocalBounds();
                name_text.setOrigin({bounds.position.x + bounds.size.x / 2.0f, bounds.position.y + bounds.size.y});

                name_text.setScale({1.0f, -1.0f});

                float center_x = (screen_position.x + component.size.x / 2.0f) * unit_size;
                float above_y = (screen_position.y + component.size.y) * unit_size + unit_size * 0.2f;

                name_text.setPosition({center_x, above_y});

                name_text.setFillColor(sf::Color::White);
                name_text.setOutlineColor(sf::Color::Black);
                name_text.setOutlineThickness(1.0f);

                window.draw(name_text);
            }
        }
    }
}