#include "../include/InventoryWidget.hpp"
#include "../include/AssetManager.hpp"
#include "../include/InputManager.hpp"
#include "../include/Slot.hpp"

#include <iostream>
#include <cmath>

constexpr int PAGES = 2;

constexpr unsigned int TEXTURE_RECT_WIDTH = 202; 
constexpr unsigned int TEXTURE_RECT_HEIGHT = 117; 

constexpr unsigned int PAGE_WIDTH = 202; 
constexpr unsigned int PAGE_HEIGHT = 92; 

InventoryWidget::InventoryWidget(InventoryComponent* inventoryComponent) : UIElement({0.0f, 0.0f}, {0.0f, 0.0f})
{
    this->inventoryComponent = inventoryComponent;

    if(!inventoryComponent) return;

    for(int i = 0; i < 36; i++)
    {
        slots[i].setItemStack(inventoryComponent->inventory.slots[i]);
        slots[i].show_item_info = true;
    }

    for(auto& recipe : recipes)
    {
        Slot slot({0.0f, 0.0f}, {0.0f, 0.0f});
        slot.setItemStack(recipe.first);
        slot.show_item_info = false;
        crafting_slots.push_back(slot);
    }

    crafting_button = Button({0.0f, 0.0f}, {0.0f, 0.0f}, sf::Color::Black, "Craft", [this](){craft_clicked = true;});
}

void InventoryWidget::handleEvent(const sf::Event& event)
{
    if(page == 0)
    {
        for(int i = 0; i < 36; i++) slots[i].handleEvent(event);
    }
    else if(page == 1)
    {
        for(auto& slot : crafting_slots)
        {
            slot.handleEvent(event);

            if(slot.clicked)
            {
                slot.clicked = false;
                selected_stack = slot.item_stack;
            }
        }
        crafting_button.handleEvent(event);

        if(craft_clicked && recipes.contains(selected_stack))
        {            

            auto& recipe = recipes[selected_stack];

            bool craftable = true;

            for(auto& ingredient : recipe.ingredients)
            {
                bool found = false;

                for(auto& stack : inventoryComponent->inventory.slots)
                {
                    if(stack.empty()) continue;

                    if(stack.itemID == ingredient.itemID && stack.quantity >= ingredient.quantity)
                    {
                        found = true;

                        break;
                    }
                }

                if(!found)
                {
                    craftable = false;

                    break;
                }
            }

            if(craftable)
            {
                on_craft(selected_stack);
            }

            craft_clicked = false;
        }
    }
}

void InventoryWidget::update(float dt)
{
    if(InputManager::isLazyKeyPressed(sf::Keyboard::Key::Left) && page > 0) page--;
    if(InputManager::isLazyKeyPressed(sf::Keyboard::Key::Right) && page < PAGES - 1) page++;

    if(!inventoryComponent) return;

    for(int i = 0; i < 36; i++)
    {
        slots[i].setItemStack(inventoryComponent->inventory.slots[i]);
    }

    for(auto& slot : crafting_slots)
    {
        auto& recipe = recipes[slot.item_stack];

        bool craftable = true;

        for(auto& ingredient : recipe.ingredients)
        {
            bool found = false;

            for(auto& stack : inventoryComponent->inventory.slots)
            {
                if(stack.empty()) continue;

                if(stack.itemID == ingredient.itemID && stack.quantity >= ingredient.quantity)
                {
                    found = true;

                    break;
                }
            }

            if(!found)
            {
                craftable = false;

                break;
            }
        }

        slot.background_color = (craftable) ? sf::Color::Green : sf::Color::Red;
    }

    crafting_button.update(dt);
}

void InventoryWidget::render(sf::RenderWindow& window)
{
    if(!active || !inventoryComponent) return;

    size = {window.getSize().x * 0.75f, window.getSize().y * 0.75f};
    position = {window.getSize().x * 0.125f, window.getSize().y * 0.125f};

    float scale = std::min(size.x / static_cast<float>(TEXTURE_RECT_WIDTH), size.y / static_cast<float>(TEXTURE_RECT_HEIGHT));

    renderPages(window);

    auto& component = *inventoryComponent;

    auto& inventory_texture = AssetManager::getUITexture(AssetManager::UITextureID::Inventory);

    float texWidth = inventory_texture.getSize().x;

    float height_offset = 25.0f * scale;

    if(page == 0)
    {
        float border = 3.0f * scale;
        float gap    = 2.0f * scale;
        float slot_size = 20.0f * scale;

        float itemSize = slot_size * (16.0f / 20.0f);

        for(int i = 35; i >= 0; i--)
        {
            int col = i % 9;
            int row = i / 9;

            auto& item = component.inventory.slots[i];

            sf::Vector2f slotPosition = position + sf::Vector2f(
                border + col * (slot_size + gap),
                border + row * (slot_size + gap) + height_offset
            );

            slots[i].position = slotPosition;
            slots[i].size = {slot_size, slot_size};

            slots[i].render(window);
        }
    }
    else if(page == 1)
    {
        float border = 3.0f * scale;
        float gap    = 2.0f * scale;
        float slot_size = 20.0f * scale;

        float itemSize = slot_size * (16.0f / 20.0f);

        for(int i = crafting_slots.size() - 1; i >= 0; i--)
        {
            int col = i % 4;
            int row = crafting_scroll_value +  i / 4;

            sf::Vector2f slotPosition = position + sf::Vector2f(
                border + col * (slot_size + gap),
                border + row * (slot_size + gap) + height_offset
            );

            crafting_slots[i].position = slotPosition;
            crafting_slots[i].size = {slot_size, slot_size};

            crafting_slots[i].render(window);


            sf::Vector2f button_position = {position.x + size.x / 2.0f + border, position.x + height_offset + border};
            sf::Vector2f button_size = {size.x / 2.0f - border * 2.0f, size.x / 5.0f - border * 2.0f};

            crafting_button.position = button_position;
            crafting_button.size = button_size;

            crafting_button.render(window);
        }
    }

}

void InventoryWidget::renderPages(sf::RenderWindow& window)
{
    auto render_page = [&](int page)
    {
        auto& shader = AssetManager::getShader(AssetManager::ShaderID::Brightness);

        shader.setUniform("brightness", 0.5f);

        float scale = std::min(size.x / static_cast<float>(TEXTURE_RECT_WIDTH), size.y / static_cast<float>(TEXTURE_RECT_HEIGHT));

        auto& inventory_texture = AssetManager::getUITexture(AssetManager::UITextureID::Inventory);

        sf::RectangleShape background(sf::Vector2f{static_cast<float>(TEXTURE_RECT_WIDTH) * scale, static_cast<float>(TEXTURE_RECT_HEIGHT) * scale});
        background.setTexture(&inventory_texture);
        background.setTextureRect({{static_cast<int>(TEXTURE_RECT_WIDTH) * page, 0}, {static_cast<int>(TEXTURE_RECT_WIDTH), TEXTURE_RECT_HEIGHT}});
        background.setPosition(position);

        if(page == this->page)  window.draw(background);
        else window.draw(background, &shader);
    };

    for(int i = PAGES - 1; i >= 0; i--)
    {
        if(i == page) continue;

        render_page(i);
    }

    render_page(page);
}