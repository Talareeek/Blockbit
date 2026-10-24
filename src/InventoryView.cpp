#include "../include/InventoryView.hpp"

#include "../include/World.hpp"
#include "../include/Entity.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/InputManager.hpp"
#include "../include/Render.hpp"
#include "../include/Prefab.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
    std::vector<uint8_t> encodeStack(const ItemStack& stack)
    {
        std::vector<uint8_t> buffer(sizeof(ItemStack));
        std::memcpy(buffer.data(), &stack, sizeof(ItemStack));
        return buffer;
    }
}

// ---------- ServerInventoryView ----------

ServerInventoryView::ServerInventoryView(UUID owner) : owner{owner}
{
}

bool ServerInventoryView::decodeStack(const std::vector<uint8_t>& payload, ItemStack& out)
{
    if (payload.size() != sizeof(ItemStack)) return false;
    std::memcpy(&out, payload.data(), sizeof(ItemStack));
    return true;
}

void ServerInventoryView::pushSlotUpdate(uint16_t target, const ItemStack& stack)
{
    pending.push_back(SetContentData{target, encodeStack(stack)});
}

void ServerInventoryView::handleLeftClick(World& world, uint16_t slot)
{
    if (slot >= INVENTORY_VIEW_SLOT_COUNT) return;
    if (!world.doesEntityExist(owner)) return;

    auto& entity = world.getEntity(owner);
    if (!entity.hasComponent<InventoryComponent>()) return;
    auto& component = entity.getComponent<InventoryComponent>();
    auto& stack = component.inventory.slots[slot];

    if (cursor_stack.empty())
    {
        cursor_stack = stack;
        stack = {ItemID::None, 0};
    }
    else if (stack.empty())
    {
        stack = cursor_stack;
        cursor_stack = {ItemID::None, 0};
    }
    else if (canStack(stack, cursor_stack))
    {
        uint32_t max_stack = itemDatabase[stack.itemID].maxStackSize;
        uint32_t can_add = std::min(cursor_stack.quantity, (max_stack > stack.quantity) ? (max_stack - stack.quantity) : 0u);
        stack.quantity += can_add;
        cursor_stack.quantity -= can_add;
        if (cursor_stack.quantity == 0) cursor_stack = {ItemID::None, 0};
    }
    else
    {
        std::swap(stack, cursor_stack);
    }

    component.dirty = true;
}

void ServerInventoryView::handleRightClick(World& world, uint16_t slot)
{
    if (slot >= INVENTORY_VIEW_SLOT_COUNT) return;
    if (!world.doesEntityExist(owner)) return;

    auto& entity = world.getEntity(owner);
    if (!entity.hasComponent<InventoryComponent>()) return;
    auto& component = entity.getComponent<InventoryComponent>();
    auto& stack = component.inventory.slots[slot];

    if (cursor_stack.empty())
    {
        if (stack.empty()) return;
        if (stack.isUnique())
        {
            cursor_stack = stack;
            stack = {ItemID::None, 0};
        }
        else
        {
            uint32_t half = (stack.quantity + 1) / 2;
            cursor_stack = {stack.itemID, half};
            stack.quantity -= half;
            if (stack.quantity == 0) stack.itemID = ItemID::None;
        }
    }
    else if (stack.empty())
    {
        if (cursor_stack.isUnique())
        {
            stack = cursor_stack;
            cursor_stack = {ItemID::None, 0};
        }
        else
        {
            stack = {cursor_stack.itemID, 1};
            if (--cursor_stack.quantity == 0) cursor_stack = {ItemID::None, 0};
        }
    }
    else if (canStack(stack, cursor_stack))
    {
        uint32_t max_stack = itemDatabase[stack.itemID].maxStackSize;
        if (stack.quantity < max_stack)
        {
            stack.quantity++;
            if (--cursor_stack.quantity == 0) cursor_stack = {ItemID::None, 0};
        }
    }
    else
    {
        std::swap(stack, cursor_stack);
    }

    component.dirty = true;
}

namespace
{
    uint32_t availableOf(const Inventory& inventory, ItemID id)
    {
        uint32_t available = 0;
        for (const auto& slot : inventory.slots)
        {
            if (slot.itemID == id && !slot.instanceID.has_value()) available += slot.quantity;
        }
        return available;
    }

    uint32_t freeSpaceFor(const Inventory& inventory, ItemID id)
    {
        const uint32_t max_stack = itemDatabase[id].maxStackSize;
        uint32_t space = 0;
        for (const auto& slot : inventory.slots)
        {
            if (slot.empty())
            {
                space += max_stack;
            }
            else if (slot.itemID == id && !slot.instanceID.has_value() && slot.quantity < max_stack)
            {
                space += max_stack - slot.quantity;
            }
        }
        return space;
    }

    uint32_t maxCraftableByIngredients(const Inventory& inventory, const Recipe& recipe)
    {
        uint32_t cap = std::numeric_limits<uint32_t>::max();
        for (const auto& ingredient : recipe.ingredients)
        {
            if (ingredient.quantity == 0) continue;
            uint32_t available = availableOf(inventory, ingredient.itemID);
            cap = std::min(cap, available / ingredient.quantity);
            if (cap == 0) return 0;
        }
        return (cap == std::numeric_limits<uint32_t>::max()) ? 0 : cap;
    }

    void performCraftOnce(Inventory& inventory, const ItemStack& requested, const Recipe& recipe)
    {
        for (const auto& ingredient : recipe.ingredients)
        {
            inventory.removeItemWithLeftover(ingredient.itemID, ingredient.quantity);
        }
        inventory.addItemWithLeftover(requested.itemID, requested.quantity);
    }
}

void ServerInventoryView::handleCraft(World& world, const ItemStack& requested)
{
    if (!recipes.contains(requested)) return;
    if (recipes[requested].requires_crafting_table) return;
    if (!world.doesEntityExist(owner)) return;

    auto& entity = world.getEntity(owner);
    if (!entity.hasComponent<InventoryComponent>()) return;
    auto& component = entity.getComponent<InventoryComponent>();
    auto& inventory = component.inventory;

    const auto& recipe = recipes[requested];

    if (maxCraftableByIngredients(inventory, recipe) == 0) return;

    performCraftOnce(inventory, requested, recipe);
    component.dirty = true;
}

void ServerInventoryView::handleCraftAll(World& world, const ItemStack& requested)
{
    if (!recipes.contains(requested)) return;
    if (recipes[requested].requires_crafting_table) return;
    if (!world.doesEntityExist(owner)) return;

    auto& entity = world.getEntity(owner);
    if (!entity.hasComponent<InventoryComponent>()) return;
    auto& component = entity.getComponent<InventoryComponent>();
    auto& inventory = component.inventory;

    const auto& recipe = recipes[requested];

    uint32_t times = 0;
    while (true)
    {
        if (maxCraftableByIngredients(inventory, recipe) == 0) break;
        if (freeSpaceFor(inventory, requested.itemID) < requested.quantity) break;

        performCraftOnce(inventory, requested, recipe);
        ++times;
    }

    if (times > 0) component.dirty = true;
}

void ServerInventoryView::handleViewEvent(World& world, const ViewEventData& event_data)
{
    switch (static_cast<InventoryViewAction>(event_data.action))
    {
        case InventoryViewAction::LeftClick:
            handleLeftClick(world, event_data.sender);
            break;
        case InventoryViewAction::RightClick:
            handleRightClick(world, event_data.sender);
            break;
        case InventoryViewAction::Craft:
        {
            ItemStack requested{ItemID::None, 0};
            if (decodeStack(event_data.payload, requested))
            {
                handleCraft(world, requested);
            }
            break;
        }
        case InventoryViewAction::CraftAll:
        {
            ItemStack requested{ItemID::None, 0};
            if (decodeStack(event_data.payload, requested))
            {
                handleCraftAll(world, requested);
            }
            break;
        }
    }
}

void ServerInventoryView::update(World& world)
{
    if (!world.doesEntityExist(owner)) return;
    auto& entity = world.getEntity(owner);
    if (!entity.hasComponent<InventoryComponent>()) return;
    auto& inventory = entity.getComponent<InventoryComponent>().inventory;

    uint16_t count = static_cast<uint16_t>(std::min<size_t>(inventory.slots.size(), INVENTORY_VIEW_SLOT_COUNT));

    for (uint16_t i = 0; i < count; ++i)
    {
        if (inventory.slots[i].itemID != last_slots[i].itemID
            || inventory.slots[i].quantity != last_slots[i].quantity
            || !(inventory.slots[i].instanceID == last_slots[i].instanceID))
        {
            last_slots[i] = inventory.slots[i];
            pushSlotUpdate(i, inventory.slots[i]);
        }
    }

    if (cursor_stack.itemID != last_cursor.itemID
        || cursor_stack.quantity != last_cursor.quantity
        || !(cursor_stack.instanceID == last_cursor.instanceID))
    {
        last_cursor = cursor_stack;
        pushSlotUpdate(INVENTORY_VIEW_CURSOR_SLOT, cursor_stack);
    }
}

void ServerInventoryView::onClose(World& world)
{
    if (cursor_stack.empty()) return;
    if (!world.doesEntityExist(owner)) return;

    auto& player = world.getEntity(owner);
    if (!player.hasComponent<InventoryComponent>()) return;

    auto& inventory = player.getComponent<InventoryComponent>().inventory;
    ItemStack leftover = inventory.addStackWithLeftover(cursor_stack);
    player.getComponent<InventoryComponent>().dirty = true;

    cursor_stack = {ItemID::None, 0};

    if (leftover.empty() || !player.hasComponent<TransformComponent>()) return;

    const auto& transform = player.getComponent<TransformComponent>();

    spawnDroppedItem(world, transform.position + sf::Vector2<double>(0.0, 1.5), leftover, randomDropSpread(45.0f));
}

std::deque<SetContentData> ServerInventoryView::drainPendingContentUpdates()
{
    std::deque<SetContentData> drained;
    drained.swap(pending);
    return drained;
}

#ifndef HEADLESS_BUILD
// ---------- ClientInventoryView ----------

namespace
{
    constexpr int PAGES = 2;
    constexpr unsigned int TEXTURE_RECT_WIDTH  = 202;
    constexpr unsigned int TEXTURE_RECT_HEIGHT = 117;

    constexpr int RECIPE_GRID_COLS     = 4;
    constexpr int RECIPE_VISIBLE_ROWS  = 4;

    bool hasIngredients(const ItemStack recipe_stacks[], size_t count, const Recipe& recipe)
    {
        for (const auto& ingredient : recipe.ingredients)
        {
            uint32_t available = 0;
            for (size_t i = 0; i < count; ++i)
            {
                if (recipe_stacks[i].itemID == ingredient.itemID) available += recipe_stacks[i].quantity;
            }
            if (available < ingredient.quantity) return false;
        }
        return true;
    }
}

ClientInventoryView::ClientInventoryView()
{
    for (uint16_t i = 0; i < INVENTORY_VIEW_SLOT_COUNT; ++i)
    {
        slots[i].show_item_info = true;
    }

    for (const auto& [recipe_stack, recipe] : recipes)
    {
        if (recipe.requires_crafting_table) continue;

        Slot slot({0.0f, 0.0f}, {0.0f, 0.0f});
        slot.setItemStack(recipe_stack);
        slot.show_item_info = true;
        crafting_slots.push_back(slot);
    }
}

void ClientInventoryView::sendCraftRequest(InventoryViewAction action)
{
    if (!recipes.contains(selected_recipe)) return;
    if (!hasIngredients(slot_stacks, INVENTORY_VIEW_SLOT_COUNT, recipes[selected_recipe])) return;

    std::vector<uint8_t> payload(sizeof(ItemStack));
    std::memcpy(payload.data(), &selected_recipe, sizeof(ItemStack));
    pending.push_back(ViewEventData{0, static_cast<uint8_t>(action), std::move(payload)});
}

void ClientInventoryView::pushClick(uint16_t slot, InventoryViewAction action, const ItemStack& stack)
{
    std::vector<uint8_t> payload(sizeof(ItemStack));
    std::memcpy(payload.data(), &stack, sizeof(ItemStack));

    pending.push_back(ViewEventData{slot, static_cast<uint8_t>(action), std::move(payload)});
}

void ClientInventoryView::handleEvent(const sf::Event& event)
{
    if (page == 0)
    {
        for (uint16_t i = 0; i < INVENTORY_VIEW_SLOT_COUNT; ++i) slots[i].handleEvent(event);

        if (auto button = event.getIf<sf::Event::MouseButtonPressed>())
        {
            for (uint16_t i = 0; i < INVENTORY_VIEW_SLOT_COUNT; ++i)
            {
                if (!slots[i].getHovered()) continue;

                if (button->button == sf::Mouse::Button::Left)
                {
                    pushClick(i, InventoryViewAction::LeftClick, slot_stacks[i]);
                }
                else if (button->button == sf::Mouse::Button::Right)
                {
                    pushClick(i, InventoryViewAction::RightClick, slot_stacks[i]);
                }

                slots[i].clicked = false;
            }
        }
    }
    else if (page == 1)
    {
        const int total_rows    = (static_cast<int>(crafting_slots.size()) + RECIPE_GRID_COLS - 1) / RECIPE_GRID_COLS;
        const int max_scroll    = std::max(0, total_rows - RECIPE_VISIBLE_ROWS);
        const int first_visible = recipe_scroll_offset * RECIPE_GRID_COLS;
        const int last_visible  = std::min(static_cast<int>(crafting_slots.size()), first_visible + RECIPE_VISIBLE_ROWS * RECIPE_GRID_COLS);

        if (auto wheel = event.getIf<sf::Event::MouseWheelScrolled>())
        {
            sf::Vector2f mouse_pos(static_cast<float>(wheel->position.x), static_cast<float>(wheel->position.y));
            if (recipe_area_bounds.contains(mouse_pos))
            {
                if (wheel->delta > 0.0f) recipe_scroll_offset = std::max(0, recipe_scroll_offset - 1);
                else if (wheel->delta < 0.0f) recipe_scroll_offset = std::min(max_scroll, recipe_scroll_offset + 1);
            }
        }

        for (int i = first_visible; i < last_visible; ++i)
        {
            auto& slot = crafting_slots[i];
            slot.handleEvent(event);

            if (slot.clicked)
            {
                slot.clicked = false;
                selected_recipe = slot.item_stack;
            }
        }

        if (auto moved = event.getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mouse_pos(static_cast<float>(moved->position.x), static_cast<float>(moved->position.y));
            craft_hovered     = craft_button_bounds.contains(mouse_pos);
            craft_all_hovered = craft_all_button_bounds.contains(mouse_pos);
        }

        if (auto pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mouse_pos(static_cast<float>(pressed->position.x), static_cast<float>(pressed->position.y));
                if (craft_button_bounds.contains(mouse_pos))     craft_pressed = true;
                if (craft_all_button_bounds.contains(mouse_pos)) craft_all_pressed = true;
            }
        }

        if (auto released = event.getIf<sf::Event::MouseButtonReleased>())
        {
            if (released->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mouse_pos(static_cast<float>(released->position.x), static_cast<float>(released->position.y));
                if (craft_pressed && craft_button_bounds.contains(mouse_pos))
                {
                    sendCraftRequest(InventoryViewAction::Craft);
                }
                if (craft_all_pressed && craft_all_button_bounds.contains(mouse_pos))
                {
                    sendCraftRequest(InventoryViewAction::CraftAll);
                }
                craft_pressed = false;
                craft_all_pressed = false;
            }
        }
    }
}

void ClientInventoryView::update(float dt)
{
    if (InputManager::isLazyKeyPressed(sf::Keyboard::Key::Left)  && page > 0) page--;
    if (InputManager::isLazyKeyPressed(sf::Keyboard::Key::Right) && page < PAGES - 1) page++;

    for (uint16_t i = 0; i < INVENTORY_VIEW_SLOT_COUNT; ++i)
    {
        slots[i].setItemStack(slot_stacks[i]);
    }

    for (auto& slot : crafting_slots)
    {
        bool craftable = recipes.contains(slot.item_stack) && hasIngredients(slot_stacks, INVENTORY_VIEW_SLOT_COUNT, recipes[slot.item_stack]);
        bool selected  = (slot.item_stack == selected_recipe);

        sf::Color base = craftable ? sf::Color(40, 160, 40, 170) : sf::Color(160, 40, 40, 170);
        slot.background_color = selected ? sf::Color(10, 10, 10, 220) : base;
    }
}

void ClientInventoryView::render(sf::RenderWindow& window)
{
    sf::Vector2u window_size = window.getSize();

    size     = {window_size.x * 0.75f, window_size.y * 0.75f};
    position = {window_size.x * 0.125f, window_size.y * 0.125f};

    renderPagesBackground(window);

    const float scale        = std::min(size.x / static_cast<float>(TEXTURE_RECT_WIDTH), size.y / static_cast<float>(TEXTURE_RECT_HEIGHT));
    const float border       = 3.0f  * scale;
    const float gap          = 2.0f  * scale;
    const float slot_size    = 20.0f * scale;
    const float height_offset = 25.0f * scale;

    int hovered_slot = -1;

    if (page == 0)
    {
        for (int i = 35; i >= 0; --i)
        {
            int col = i % 9;
            int row = i / 9;

            sf::Vector2f slot_position = position + sf::Vector2f(
                border + col * (slot_size + gap),
                border + row * (slot_size + gap) + height_offset
            );

            slots[i].position = slot_position;
            slots[i].size     = {slot_size, slot_size};

            const bool prev_show = slots[i].show_item_info;
            slots[i].show_item_info = false;
            slots[i].render(window);
            slots[i].show_item_info = prev_show;

            if (prev_show && slots[i].getHovered() && !slots[i].item_stack.empty())
            {
                hovered_slot = i;
            }
        }
    }
    else
    {
        const int total_rows    = (static_cast<int>(crafting_slots.size()) + RECIPE_GRID_COLS - 1) / RECIPE_GRID_COLS;
        const int max_scroll    = std::max(0, total_rows - RECIPE_VISIBLE_ROWS);
        recipe_scroll_offset    = std::clamp(recipe_scroll_offset, 0, max_scroll);
        const int first_visible = recipe_scroll_offset * RECIPE_GRID_COLS;
        const int last_visible  = std::min(static_cast<int>(crafting_slots.size()), first_visible + RECIPE_VISIBLE_ROWS * RECIPE_GRID_COLS);

        recipe_area_bounds = sf::FloatRect(
            {position.x + border, position.y + border + height_offset},
            {RECIPE_GRID_COLS * slot_size + (RECIPE_GRID_COLS - 1) * gap,
             RECIPE_VISIBLE_ROWS * slot_size + (RECIPE_VISIBLE_ROWS - 1) * gap}
        );

        int hovered_recipe = -1;
        for (int i = last_visible - 1; i >= first_visible; --i)
        {
            int col         = i % RECIPE_GRID_COLS;
            int visible_row = (i / RECIPE_GRID_COLS) - recipe_scroll_offset;

            sf::Vector2f slot_position = position + sf::Vector2f(
                border + col * (slot_size + gap),
                border + visible_row * (slot_size + gap) + height_offset
            );

            crafting_slots[i].position = slot_position;
            crafting_slots[i].size     = {slot_size, slot_size};

            const bool prev_show = crafting_slots[i].show_item_info;
            crafting_slots[i].show_item_info = false;
            crafting_slots[i].render(window);
            crafting_slots[i].show_item_info = prev_show;

            if (prev_show && crafting_slots[i].getHovered() && !crafting_slots[i].item_stack.empty())
            {
                hovered_recipe = i;
            }
        }

        renderIngredientList(window, scale);

        // Craft button bounds live in texture pixel space on page 2.
        // Page 2 starts at texture x=202, so subtract that to get local offsets.
        const float page2_origin_x = position.x;
        const float page2_origin_y = position.y;

        craft_button_bounds = sf::FloatRect(
            {page2_origin_x + (299.0f - 202.0f) * scale, page2_origin_y + 101.0f * scale},
            {(345.0f - 299.0f) * scale, (111.0f - 101.0f) * scale}
        );
        craft_all_button_bounds = sf::FloatRect(
            {page2_origin_x + (352.0f - 202.0f) * scale, page2_origin_y + 101.0f * scale},
            {(398.0f - 352.0f) * scale, (111.0f - 101.0f) * scale}
        );

        renderCraftButtonOverlays(window);

        if (hovered_recipe >= 0)
        {
            renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), crafting_slots[hovered_recipe].item_stack, window);
        }
    }

    renderCursor(window);

    if (hovered_slot >= 0)
    {
        renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), slots[hovered_slot].item_stack, window);
    }
}

void ClientInventoryView::renderIngredientList(sf::RenderWindow& window, float scale)
{
    if (!recipes.contains(selected_recipe)) return;

    const Recipe& recipe = recipes[selected_recipe];
    if (recipe.ingredients.empty()) return;

    // Two columns of ingredient slots. Column rects (texture pixel coords):
    //   col 0: x 331-343, y 36-88
    //   col 1: x 353-365, y 36-88
    constexpr float COLUMN_X[2]    = {331.0f, 353.0f};
    constexpr float COLUMN_WIDTH   = 12.0f;
    constexpr float COLUMN_TOP_Y   = 36.0f;
    constexpr float COLUMN_HEIGHT  = 52.0f;
    constexpr int   SLOTS_PER_COL  = 4;

    const float slot_px     = COLUMN_WIDTH;
    const float slot_gap_px = (COLUMN_HEIGHT - SLOTS_PER_COL * slot_px) / std::max(1, SLOTS_PER_COL - 1);

    const float page2_origin_x = position.x;
    const float page2_origin_y = position.y;

    const sf::Vector2f mouse_pos = static_cast<sf::Vector2f>(sf::Mouse::getPosition(window));
    int hovered = -1;

    for (size_t idx = 0; idx < recipe.ingredients.size() && idx < 2 * SLOTS_PER_COL; ++idx)
    {
        int col = static_cast<int>(idx) / SLOTS_PER_COL;
        int row = static_cast<int>(idx) % SLOTS_PER_COL;

        sf::Vector2f pos{
            page2_origin_x + (COLUMN_X[col] - 202.0f) * scale,
            page2_origin_y + (COLUMN_TOP_Y + row * (slot_px + slot_gap_px)) * scale
        };
        sf::Vector2f size{slot_px * scale, slot_px * scale};

        Slot ingredient;
        ingredient.position = pos;
        ingredient.size     = size;
        ingredient.setItemStack(recipe.ingredients[idx]);
        ingredient.background_color = sf::Color(0, 0, 0, 0);
        ingredient.outline_color    = sf::Color(0, 0, 0, 0);
        ingredient.show_item_info   = false;
        ingredient.render(window);

        if (sf::FloatRect(pos, size).contains(mouse_pos)) hovered = static_cast<int>(idx);
    }

    if (hovered >= 0)
    {
        renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), recipe.ingredients[hovered], window);
    }
}

void ClientInventoryView::renderCraftButtonOverlays(sf::RenderWindow& window)
{
    const bool craftable = recipes.contains(selected_recipe)
                        && hasIngredients(slot_stacks, INVENTORY_VIEW_SLOT_COUNT, recipes[selected_recipe]);

    auto drawOverlay = [&](const sf::FloatRect& bounds, bool pressed, bool hovered, bool disabled)
    {
        sf::Color fill;
        if (disabled)     fill = sf::Color(0, 0, 0, 160);
        else if (pressed) fill = sf::Color(0, 0, 0, 110);
        else if (hovered) fill = sf::Color(255, 255, 255, 40);
        else              return;

        sf::RectangleShape overlay(bounds.size);
        overlay.setPosition(bounds.position);
        overlay.setFillColor(fill);
        window.draw(overlay);
    };

    drawOverlay(craft_button_bounds,     craft_pressed,     craft_hovered,     !craftable);
    drawOverlay(craft_all_button_bounds, craft_all_pressed, craft_all_hovered, !craftable);
}

void ClientInventoryView::renderPagesBackground(sf::RenderWindow& window)
{
    const float scale = std::min(size.x / static_cast<float>(TEXTURE_RECT_WIDTH), size.y / static_cast<float>(TEXTURE_RECT_HEIGHT));
    auto& inventory_texture = AssetManager::getUITexture(AssetManager::UITextureID::Inventory);

    auto draw_page = [&](int page_index)
    {
        sf::RectangleShape background({static_cast<float>(TEXTURE_RECT_WIDTH) * scale, static_cast<float>(TEXTURE_RECT_HEIGHT) * scale});
        background.setTexture(&inventory_texture);
        background.setTextureRect({{static_cast<int>(TEXTURE_RECT_WIDTH) * page_index, 0}, {static_cast<int>(TEXTURE_RECT_WIDTH), TEXTURE_RECT_HEIGHT}});
        background.setPosition(position);

        if (page_index == page)
        {
            window.draw(background);
        }
        else
        {
            auto& shader = AssetManager::getShader(AssetManager::ShaderID::Brightness);
            shader.setUniform("brightness", 0.5f);
            window.draw(background, &shader);
        }
    };

    for (int i = PAGES - 1; i >= 0; --i)
    {
        if (i == page) continue;
        draw_page(i);
    }

    draw_page(page);
}

void ClientInventoryView::renderCursor(sf::RenderWindow& window)
{
    if (cursor_stack.empty()) return;

    const float scale     = std::min(size.x / static_cast<float>(TEXTURE_RECT_WIDTH), size.y / static_cast<float>(TEXTURE_RECT_HEIGHT));
    const float slot_size = 20.0f * scale;

    sf::Vector2f live_mouse = static_cast<sf::Vector2f>(sf::Mouse::getPosition(window));

    Slot cursor;
    cursor.size = {slot_size, slot_size};
    cursor.position = live_mouse - cursor.size * 0.5f;
    cursor.setItemStack(cursor_stack);
    cursor.show_item_info = false;
    cursor.background_color = sf::Color(0, 0, 0, 0);
    cursor.outline_color    = sf::Color(0, 0, 0, 0);
    cursor.render(window);
}

void ClientInventoryView::handleContentUpdate(const SetContentData& set_content_data)
{
    if (set_content_data.payload.size() != sizeof(ItemStack)) return;

    ItemStack stack{ItemID::None, 0};
    std::memcpy(&stack, set_content_data.payload.data(), sizeof(ItemStack));

    if (set_content_data.target == INVENTORY_VIEW_CURSOR_SLOT)
    {
        cursor_stack = stack;
    }
    else if (set_content_data.target < INVENTORY_VIEW_SLOT_COUNT)
    {
        slot_stacks[set_content_data.target] = stack;
    }
}

std::deque<ViewEventData> ClientInventoryView::drainPendingEvents()
{
    std::deque<ViewEventData> drained;
    drained.swap(pending);
    return drained;
}
#endif // HEADLESS_BUILD
