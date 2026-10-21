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

        bool flipX = false;
        if(entity.hasComponent<AnimationComponent>())
        {
            flipX = entity.getComponent<AnimationComponent>().direction == AnimationComponent::Direction::Right;
        }

        auto& transform = entity.getComponent<TransformComponent>();

        sf::Vector2f screen_position = {static_cast<float>(transform.render_position.x - camera.x), static_cast<float>(transform.render_position.y - camera.y)};

        const float px = screen_position.x * unit_size;
        const float py = screen_position.y * unit_size;
        const float width  = component.size.x * unit_size;
        const float height = component.size.y * unit_size;

        sf::Color color = sf::Color::White;
        if(entity.hasComponent<HealthComponent>())
        {
            auto& health = entity.getComponent<HealthComponent>();

            if(health.since_last_damaged < HealthComponent::DAMAGE_FLASH_DURATION)
            {
                const float progress = health.since_last_damaged / HealthComponent::DAMAGE_FLASH_DURATION;
                const float intensity = 1.0f - std::clamp(progress, 0.0f, 1.0f);

                const uint8_t fade = static_cast<uint8_t>(std::round(255.0f * (1.0f - intensity)));

                color = sf::Color(255, fade, fade, 255);
            }
        }

        // Sub-pixel inset in X only: animation frames in an entity sheet are
        // packed back-to-back horizontally, so a UV sitting exactly on a
        // frame boundary can bleed into the neighbour once FP interpolation
        // nudges it past the integer edge. The inset is well below one texel
        // so it doesn't visibly truncate the sprite under upscaled rendering.
        // Y has no neighbouring content (frames span the full texture height)
        // so no Y inset is needed.
        constexpr float INSET = 1.0f / 128.0f;
        float u0 = static_cast<float>(component.uv.position.x) + INSET;
        float u1 = static_cast<float>(component.uv.position.x + component.uv.size.x) - INSET;
        const float v_top = static_cast<float>(component.uv.position.y);
        const float v_bot = static_cast<float>(component.uv.position.y + component.uv.size.y);

        if(flipX) std::swap(u0, u1);

        sf::VertexArray quad(sf::PrimitiveType::Triangles, 6);

        // Matches the old sf::Sprite with origin=(0,h) and scaleY=-|sy|:
        // the texture's bottom row maps to view-y = py (sprite base), the
        // top row to py + height. The view is Y-flipped so this reads
        // right-side-up on screen.
        sf::Vertex bl, br, tr, tl;
        bl.position = {px,         py};
        br.position = {px + width, py};
        tr.position = {px + width, py + height};
        tl.position = {px,         py + height};

        bl.texCoords = {u0, v_bot};
        br.texCoords = {u1, v_bot};
        tr.texCoords = {u1, v_top};
        tl.texCoords = {u0, v_top};

        bl.color = color;
        br.color = color;
        tr.color = color;
        tl.color = color;

        quad[0] = bl; quad[1] = br; quad[2] = tr;
        quad[3] = bl; quad[4] = tr; quad[5] = tl;

        sf::RenderStates states;
        states.texture = &AssetManager::getGameTexture(component.textureID);
        window.draw(quad, states);

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