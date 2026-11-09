#include "../include/Slot.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Render.hpp"
#include "../include/ItemUtils.hpp"
#include "../include/ItemInstance.hpp"
#include "../include/ItemDataComponents.hpp"
#include "../include/EffectRegistry.hpp"

#include <cstdint>
#include <string>
#include <algorithm>

namespace
{
    // Reads the bottle's effect off the stack's instance data and returns the
    // registry's liquid colour. No MagicEffectItemComponent → white (blank
    // bottle), which still looks fine with the frame overlay on top.
    sf::Color bottleLiquidColor(const ItemStack& stack)
    {
        if (!stack.instanceID.has_value()) return sf::Color::White;
        const ItemInstanceData* data = findInstance(*stack.instanceID);
        if (!data) return sf::Color::White;
        const MagicEffectItemComponent* bottle = data->get<MagicEffectItemComponent>();
        if (!bottle || !EffectRegistry::contains(bottle->effect)) return sf::Color::White;
        return EffectRegistry::get(bottle->effect).liquid_color;
    }
}

namespace
{
    void drawDurabilityBar(const ItemStack& stack, sf::Vector2f slot_position, sf::Vector2f slot_size, sf::RenderTarget& target)
    {
        const uint32_t max_durability = getMaxDurability(stack.itemID);
        if (max_durability == 0) return;

        const uint32_t cur_durability = getCurrentDurability(stack);
        if (cur_durability >= max_durability) return;

        const float factor = static_cast<float>(cur_durability) / static_cast<float>(max_durability);

        const float bar_width  = slot_size.x * 0.8f;
        const float bar_height = std::max(2.0f, slot_size.y * 0.08f);
        const sf::Vector2f bar_pos = slot_position + sf::Vector2f(slot_size.x * 0.1f, slot_size.y * 0.8f);

        sf::RectangleShape bg({bar_width, bar_height});
        bg.setPosition(bar_pos);
        bg.setFillColor(sf::Color(0, 0, 0, 200));
        target.draw(bg);

        const sf::Color fill_color(
            static_cast<std::uint8_t>((1.0f - factor) * 255.0f),
            static_cast<std::uint8_t>(factor * 255.0f),
            0,
            255
        );

        sf::RectangleShape fill({bar_width * factor, bar_height});
        fill.setPosition(bar_pos);
        fill.setFillColor(fill_color);
        target.draw(fill);
    }
}

Slot::Slot() : UIElement(sf::Vector2f(0.0f, 0.0f), sf::Vector2f(20.0f, 20.0f))
{
    
}

Slot::Slot(const sf::Vector2f& relative_position, const float size, bool show_item_info) : UIElement(UIElement::ScreenRelative{relative_position, sf::Vector2f(size, size), UIElement::ScreenRelative::ScaleMode::UniformByWidth}), show_item_info{show_item_info}
{
    
}

void Slot::setItemStack(const ItemStack& stack)
{
    item_stack = stack;
}

void Slot::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseMoved>())
    {
        sf::FloatRect bounds = {position, size};
        hovered = bounds.contains(static_cast<sf::Vector2f>(event.getIf<sf::Event::MouseMoved>()->position));

        mouse_pos = static_cast<sf::Vector2f>(event.getIf<sf::Event::MouseMoved>()->position);
    }
    else if(event.is<sf::Event::MouseButtonPressed>())
    {
        if(hovered)
        {
            auto button = event.getIf<sf::Event::MouseButtonPressed>()->button;
            if(button == sf::Mouse::Button::Left)       clicked = true;
            else if(button == sf::Mouse::Button::Right) right_clicked = true;
        }
    }
}

void Slot::update(float dt)
{
    
}

void Slot::render(sf::RenderWindow& window)
{
    sf::RectangleShape background(size * 0.9f);
    background.setPosition(position + size * 0.05f);
    background.setFillColor(background_color);
    background.setOutlineColor((hovered) ? sf::Color::White : outline_color);
    background.setOutlineThickness(size.x / 20.0f);
    window.draw(background);

    if(item_stack.empty())
    {
        return;
    }

    const auto& data = itemDatabase[item_stack.itemID];
    sf::Sprite sprite(AssetManager::getGameTexture(data.texture));
    sprite.setTextureRect({{0, 0}, {16, 16}});
    sprite.setPosition(position + sf::Vector2f(size.x / 10.0f, size.y / 10.0f));
    sprite.setScale({size.x * 0.8f / sprite.getLocalBounds().size.x, size.y * 0.8f / sprite.getLocalBounds().size.y});

    if(data.is_bottle)
    {
        // Two-pass bottle render: tinted liquid (x=16..32) first, then opaque
        // frame (x=0..16) on top. Colour comes from the stack's instance
        // data so one bottle.png + one ItemID covers every potion variant.
        sprite.setTextureRect({{16, 0}, {16, 16}});
        sprite.setColor(bottleLiquidColor(item_stack));
        window.draw(sprite);

        sprite.setTextureRect({{0, 0}, {16, 16}});
        sprite.setColor(sf::Color::White);
        window.draw(sprite);
    }
    else
    {
        window.draw(sprite);
    }


    sf::Text text(AssetManager::getFont(AssetManager::FontID::PressStart2P), std::to_string(item_stack.quantity), static_cast<unsigned int>(size.x / 3.0f));

    text.setPosition(position + sf::Vector2f(size.x / 8.0f, size.y / 8.0f));

    text.setFillColor(sf::Color::White);

    window.draw(text);

    drawDurabilityBar(item_stack, position, size, window);

    if(hovered && show_item_info)
    {
        renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), item_stack, window);
    }
}

void Slot::render(sf::RenderTarget& window)
{
    sf::RectangleShape background(size * 0.9f);
    background.setPosition(position + size * 0.05f);
    background.setFillColor(background_color);
    background.setOutlineColor((hovered) ? sf::Color::White : outline_color);
    background.setOutlineThickness(size.x / 20.0f);
    window.draw(background);

    if(item_stack.empty())
    {
        return;
    }

    const auto& data = itemDatabase[item_stack.itemID];
    sf::Sprite sprite(AssetManager::getGameTexture(data.texture));
    sprite.setTextureRect({{0, 0}, {16, 16}});
    sprite.setPosition(position + sf::Vector2f(size.x / 10.0f, size.y / 10.0f));
    sprite.setScale({size.x * 0.8f / sprite.getLocalBounds().size.x, size.y * 0.8f / sprite.getLocalBounds().size.y});

    if(data.is_bottle)
    {
        // Two-pass bottle render: tinted liquid (x=16..32) first, then opaque
        // frame (x=0..16) on top. Colour comes from the stack's instance
        // data so one bottle.png + one ItemID covers every potion variant.
        sprite.setTextureRect({{16, 0}, {16, 16}});
        sprite.setColor(bottleLiquidColor(item_stack));
        window.draw(sprite);

        sprite.setTextureRect({{0, 0}, {16, 16}});
        sprite.setColor(sf::Color::White);
        window.draw(sprite);
    }
    else
    {
        window.draw(sprite);
    }


    sf::Text text(AssetManager::getFont(AssetManager::FontID::PressStart2P), std::to_string(item_stack.quantity), static_cast<unsigned int>(size.x / 3.0f));

    text.setPosition(position + sf::Vector2f(size.x / 8.0f, size.y / 8.0f));

    text.setFillColor(sf::Color::White);

    window.draw(text);

    drawDurabilityBar(item_stack, position, size, window);

    if(hovered && show_item_info)
    {
        //renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), item_stack, window);
    }
}


void Slot::setHovered(bool hovered)
{
    this->hovered = hovered;
}

bool Slot::getHovered() const
{
    return hovered;
}