#include "../include/CraftingView.hpp"

#include "../include/World.hpp"
#include "../include/Block.hpp"
#include "../include/Entity.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Render.hpp"
#include "../include/Prefab.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
    constexpr unsigned int TEXTURE_RECT_WIDTH  = 202;
    constexpr unsigned int TEXTURE_RECT_HEIGHT = 183;

    // Measured directly off resources/textures/crafting_view.png.
    constexpr float RECIPE_GRID_PX_X   = 3.0f;
    constexpr float RECIPE_GRID_PX_Y   = 3.0f;
    constexpr int   RECIPE_GRID_COLS   = 4;

    constexpr float PLAYER_GRID_PX_X   = 3.0f;
    constexpr float PLAYER_GRID_PX_Y   = 94.0f;

    constexpr float SLOT_PX_SIZE       = 20.0f;
    constexpr float SLOT_PX_GAP        = 2.0f;

    constexpr float CRAFT_BTN_PX_X     = 97.0f;
    constexpr float CRAFT_BTN_PX_Y     = 76.0f;
    constexpr float CRAFT_BTN_PX_W     = 46.0f;
    constexpr float CRAFT_BTN_PX_H     = 10.0f;
    constexpr float CRAFT_ALL_BTN_PX_X = 150.0f;

    // Ingredient columns inside the open book. Positions mirror the inventory
    // view's book geometry (the book art is identical between the two sheets).
    constexpr float INGREDIENT_COL_X[2]   = {129.0f, 151.0f};
    constexpr float INGREDIENT_COL_WIDTH  = 12.0f;
    constexpr float INGREDIENT_COL_TOP_Y  = 14.0f;
    constexpr float INGREDIENT_COL_HEIGHT = 52.0f;
    constexpr int   INGREDIENT_SLOTS_PER_COL = 4;

    std::vector<uint8_t> encodeStack(const ItemStack& stack)
    {
        std::vector<uint8_t> buffer(sizeof(ItemStack));
        std::memcpy(buffer.data(), &stack, sizeof(ItemStack));
        return buffer;
    }

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

    bool hasIngredients(const std::array<ItemStack, 36>& inventory_stacks, const Recipe& recipe)
    {
        for (const auto& ingredient : recipe.ingredients)
        {
            uint32_t available = 0;
            for (const auto& stack : inventory_stacks)
            {
                if (stack.itemID == ingredient.itemID) available += stack.quantity;
            }
            if (available < ingredient.quantity) return false;
        }
        return true;
    }
}

// ---------- ServerCraftingView ----------

ServerCraftingView::ServerCraftingView(sf::Vector2i crafting_position, UUID inventory_owner) : crafting_position{crafting_position}, inventory_owner{inventory_owner}
{
}

bool ServerCraftingView::decodeStack(const std::vector<uint8_t>& payload, ItemStack& out)
{
    if (payload.size() != sizeof(ItemStack)) return false;
    std::memcpy(&out, payload.data(), sizeof(ItemStack));
    return true;
}

void ServerCraftingView::pushSlotUpdate(uint16_t target, const ItemStack& stack)
{
    pending.push_back(SetContentData{target, encodeStack(stack)});
}

void ServerCraftingView::handleLeftClick(World& world, uint16_t slot)
{
    if (slot >= 36) return;
    if (!world.doesEntityExist(inventory_owner)) return;

    auto& entity = world.getEntity(inventory_owner);
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

void ServerCraftingView::handleRightClick(World& world, uint16_t slot)
{
    if (slot >= 36) return;
    if (!world.doesEntityExist(inventory_owner)) return;

    auto& entity = world.getEntity(inventory_owner);
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

void ServerCraftingView::handleCraft(World& world, const ItemStack& requested)
{
    if (!recipes.contains(requested)) return;
    if (!world.doesEntityExist(inventory_owner)) return;

    auto& entity = world.getEntity(inventory_owner);
    if (!entity.hasComponent<InventoryComponent>()) return;
    auto& component = entity.getComponent<InventoryComponent>();
    auto& inventory = component.inventory;

    const auto& recipe = recipes[requested];

    if (maxCraftableByIngredients(inventory, recipe) == 0) return;

    performCraftOnce(inventory, requested, recipe);
    component.dirty = true;
}

void ServerCraftingView::handleCraftAll(World& world, const ItemStack& requested)
{
    if (!recipes.contains(requested)) return;
    if (!world.doesEntityExist(inventory_owner)) return;

    auto& entity = world.getEntity(inventory_owner);
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

void ServerCraftingView::handleViewEvent(World& world, const ViewEventData& event_data)
{
    switch (static_cast<CraftingViewAction>(event_data.action))
    {
        case CraftingViewAction::LeftClick:
            if (event_data.sender < CRAFTING_VIEW_CURSOR_SLOT) handleLeftClick(world, event_data.sender);
            break;
        case CraftingViewAction::RightClick:
            if (event_data.sender < CRAFTING_VIEW_CURSOR_SLOT) handleRightClick(world, event_data.sender);
            break;
        case CraftingViewAction::Craft:
        {
            ItemStack requested{ItemID::None, 0};
            if (decodeStack(event_data.payload, requested)) handleCraft(world, requested);
            break;
        }
        case CraftingViewAction::CraftAll:
        {
            ItemStack requested{ItemID::None, 0};
            if (decodeStack(event_data.payload, requested)) handleCraftAll(world, requested);
            break;
        }
    }
}

void ServerCraftingView::update(World& world)
{
    if (!world.doesEntityExist(inventory_owner)) return;
    auto& entity = world.getEntity(inventory_owner);
    if (!entity.hasComponent<InventoryComponent>()) return;
    auto& inventory = entity.getComponent<InventoryComponent>().inventory;

    const uint16_t count = static_cast<uint16_t>(std::min<size_t>(inventory.slots.size(), 36));

    for (uint16_t i = 0; i < count; ++i)
    {
        if (!(last_inventory[i] == inventory.slots[i]))
        {
            last_inventory[i] = inventory.slots[i];
            pushSlotUpdate(CRAFTING_VIEW_PLAYER_OFFSET + i, inventory.slots[i]);
        }
    }

    if (!(last_cursor == cursor_stack))
    {
        last_cursor = cursor_stack;
        pushSlotUpdate(CRAFTING_VIEW_CURSOR_SLOT, cursor_stack);
    }
}

void ServerCraftingView::onOpen(World& world)
{
    if (!world.doesEntityExist(inventory_owner)) return;
    auto& entity = world.getEntity(inventory_owner);
    if (!entity.hasComponent<InventoryComponent>()) return;

    const auto& slots = entity.getComponent<InventoryComponent>().inventory.slots;
    for (uint16_t i = 0; i < 36; ++i)
    {
        ItemStack stack = (i < slots.size()) ? slots[i] : ItemStack{ItemID::None, 0};
        last_inventory[i] = stack;
        pushSlotUpdate(CRAFTING_VIEW_PLAYER_OFFSET + i, stack);
    }

    last_cursor = cursor_stack;
    pushSlotUpdate(CRAFTING_VIEW_CURSOR_SLOT, cursor_stack);
}

void ServerCraftingView::onClose(World& world)
{
    if (cursor_stack.empty()) return;
    if (!world.doesEntityExist(inventory_owner)) return;

    auto& player = world.getEntity(inventory_owner);
    if (!player.hasComponent<InventoryComponent>()) return;

    auto& inventory = player.getComponent<InventoryComponent>().inventory;
    ItemStack leftover = inventory.addStackWithLeftover(cursor_stack);
    player.getComponent<InventoryComponent>().dirty = true;

    cursor_stack = {ItemID::None, 0};

    if (leftover.empty() || !player.hasComponent<TransformComponent>()) return;

    const auto& transform = player.getComponent<TransformComponent>();

    spawnDroppedItem(world, transform.position + sf::Vector2<double>(0.0, 1.5), leftover, randomDropSpread(45.0f));
}

bool ServerCraftingView::shouldClose(World& world) const
{
    constexpr double MAX_INTERACTION_DISTANCE = 5.0;

    if (!world.doesEntityExist(inventory_owner)) return true;

    const Entity& player = world.getEntity(inventory_owner);
    if (!player.hasComponent<TransformComponent>()) return false;

    const auto player_center = player.getComponent<TransformComponent>().center();
    const sf::Vector2<double> block_center(crafting_position.x + 0.5, crafting_position.y + 0.5);
    const sf::Vector2<double> delta = player_center - block_center;

    return (delta.x * delta.x + delta.y * delta.y) > (MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE);
}

std::deque<SetContentData> ServerCraftingView::drainPendingContentUpdates()
{
    std::deque<SetContentData> drained;
    drained.swap(pending);
    return drained;
}

// ---------- ClientCraftingView ----------

ClientCraftingView::ClientCraftingView()
{
    for (uint16_t i = 0; i < 36; ++i)
    {
        slots[i].show_item_info = true;
    }

    for (const auto& [recipe_stack, recipe] : recipes)
    {
        Slot slot({0.0f, 0.0f}, {0.0f, 0.0f});
        slot.setItemStack(recipe_stack);
        slot.show_item_info = true;
        crafting_slots.push_back(slot);
    }
}

void ClientCraftingView::sendCraftRequest(CraftingViewAction action)
{
    if (!recipes.contains(selected_recipe)) return;
    if (!hasIngredients(inventory_stacks, recipes[selected_recipe])) return;

    std::vector<uint8_t> payload(sizeof(ItemStack));
    std::memcpy(payload.data(), &selected_recipe, sizeof(ItemStack));
    pending.push_back(ViewEventData{0, static_cast<uint8_t>(action), std::move(payload)});
}

void ClientCraftingView::pushClick(uint16_t slot, CraftingViewAction action, const ItemStack& stack)
{
    std::vector<uint8_t> payload(sizeof(ItemStack));
    std::memcpy(payload.data(), &stack, sizeof(ItemStack));

    pending.push_back(ViewEventData{slot, static_cast<uint8_t>(action), std::move(payload)});
}

void ClientCraftingView::handleEvent(const sf::Event& event)
{
    for (uint16_t i = 0; i < 36; ++i) slots[i].handleEvent(event);

    for (auto& slot : crafting_slots)
    {
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
            if (craft_button_bounds.contains(mouse_pos))     craft_pressed     = true;
            if (craft_all_button_bounds.contains(mouse_pos)) craft_all_pressed = true;
        }

        for (uint16_t i = 0; i < 36; ++i)
        {
            if (!slots[i].getHovered()) continue;

            if (pressed->button == sf::Mouse::Button::Left)
            {
                pushClick(i, CraftingViewAction::LeftClick, inventory_stacks[i]);
            }
            else if (pressed->button == sf::Mouse::Button::Right)
            {
                pushClick(i, CraftingViewAction::RightClick, inventory_stacks[i]);
            }

            slots[i].clicked = false;
            slots[i].right_clicked = false;
        }
    }

    if (auto released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (released->button == sf::Mouse::Button::Left)
        {
            sf::Vector2f mouse_pos(static_cast<float>(released->position.x), static_cast<float>(released->position.y));
            if (craft_pressed && craft_button_bounds.contains(mouse_pos))
            {
                sendCraftRequest(CraftingViewAction::Craft);
            }
            if (craft_all_pressed && craft_all_button_bounds.contains(mouse_pos))
            {
                sendCraftRequest(CraftingViewAction::CraftAll);
            }
            craft_pressed     = false;
            craft_all_pressed = false;
        }
    }
}

void ClientCraftingView::update(float)
{
    for (uint16_t i = 0; i < 36; ++i) slots[i].setItemStack(inventory_stacks[i]);

    for (auto& slot : crafting_slots)
    {
        bool craftable = recipes.contains(slot.item_stack) && hasIngredients(inventory_stacks, recipes[slot.item_stack]);
        bool selected  = (slot.item_stack == selected_recipe);

        sf::Color base = craftable ? sf::Color(40, 160, 40, 170) : sf::Color(160, 40, 40, 170);
        slot.background_color = selected ? sf::Color(10, 10, 10, 220) : base;
    }
}

void ClientCraftingView::render(sf::RenderWindow& window)
{
    sf::Vector2u window_size = window.getSize();

    const float scale = std::min(
        (window_size.x * 0.75f) / static_cast<float>(TEXTURE_RECT_WIDTH),
        (window_size.y * 0.9f)  / static_cast<float>(TEXTURE_RECT_HEIGHT)
    );

    size     = {static_cast<float>(TEXTURE_RECT_WIDTH) * scale, static_cast<float>(TEXTURE_RECT_HEIGHT) * scale};
    position = {(window_size.x - size.x) * 0.5f, (window_size.y - size.y) * 0.5f};

    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setTexture(&AssetManager::getUITexture(AssetManager::UITextureID::Crafting));
    background.setTextureRect({{0, 0}, {static_cast<int>(TEXTURE_RECT_WIDTH), static_cast<int>(TEXTURE_RECT_HEIGHT)}});
    window.draw(background);

    const float slot_size = SLOT_PX_SIZE * scale;
    const float gap       = SLOT_PX_GAP  * scale;

    int hovered_recipe = -1;
    for (int i = static_cast<int>(crafting_slots.size()) - 1; i >= 0; --i)
    {
        int col = i % RECIPE_GRID_COLS;
        int row = i / RECIPE_GRID_COLS;

        sf::Vector2f slot_position = position + sf::Vector2f(
            RECIPE_GRID_PX_X * scale + col * (slot_size + gap),
            RECIPE_GRID_PX_Y * scale + row * (slot_size + gap)
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

    int hovered_player = -1;
    for (int i = 35; i >= 0; --i)
    {
        int col = i % 9;
        int row = i / 9;

        sf::Vector2f slot_position = position + sf::Vector2f(
            PLAYER_GRID_PX_X * scale + col * (slot_size + gap),
            PLAYER_GRID_PX_Y * scale + row * (slot_size + gap)
        );

        slots[i].position = slot_position;
        slots[i].size     = {slot_size, slot_size};

        const bool prev_show = slots[i].show_item_info;
        slots[i].show_item_info = false;
        slots[i].render(window);
        slots[i].show_item_info = prev_show;

        if (prev_show && slots[i].getHovered() && !slots[i].item_stack.empty())
        {
            hovered_player = i;
        }
    }

    renderIngredientList(window, scale);

    craft_button_bounds = sf::FloatRect(
        {position.x + CRAFT_BTN_PX_X     * scale, position.y + CRAFT_BTN_PX_Y * scale},
        {CRAFT_BTN_PX_W * scale, CRAFT_BTN_PX_H * scale}
    );
    craft_all_button_bounds = sf::FloatRect(
        {position.x + CRAFT_ALL_BTN_PX_X * scale, position.y + CRAFT_BTN_PX_Y * scale},
        {CRAFT_BTN_PX_W * scale, CRAFT_BTN_PX_H * scale}
    );

    renderCraftButtonOverlays(window);

    renderCursor(window);

    if (hovered_recipe >= 0)
    {
        renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), crafting_slots[hovered_recipe].item_stack, window);
    }
    else if (hovered_player >= 0)
    {
        renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), slots[hovered_player].item_stack, window);
    }
}

void ClientCraftingView::renderIngredientList(sf::RenderWindow& window, float scale)
{
    if (!recipes.contains(selected_recipe)) return;

    const Recipe& recipe = recipes[selected_recipe];
    if (recipe.ingredients.empty()) return;

    const float slot_px     = INGREDIENT_COL_WIDTH;
    const float slot_gap_px = (INGREDIENT_COL_HEIGHT - INGREDIENT_SLOTS_PER_COL * slot_px) / std::max(1, INGREDIENT_SLOTS_PER_COL - 1);

    const sf::Vector2f mouse_pos = static_cast<sf::Vector2f>(sf::Mouse::getPosition(window));
    int hovered = -1;

    for (size_t idx = 0; idx < recipe.ingredients.size() && idx < 2 * INGREDIENT_SLOTS_PER_COL; ++idx)
    {
        int col = static_cast<int>(idx) / INGREDIENT_SLOTS_PER_COL;
        int row = static_cast<int>(idx) % INGREDIENT_SLOTS_PER_COL;

        sf::Vector2f slot_pos{
            position.x + INGREDIENT_COL_X[col] * scale,
            position.y + (INGREDIENT_COL_TOP_Y + row * (slot_px + slot_gap_px)) * scale
        };
        sf::Vector2f slot_size{slot_px * scale, slot_px * scale};

        Slot ingredient;
        ingredient.position = slot_pos;
        ingredient.size     = slot_size;
        ingredient.setItemStack(recipe.ingredients[idx]);
        ingredient.background_color = sf::Color(0, 0, 0, 0);
        ingredient.outline_color    = sf::Color(0, 0, 0, 0);
        ingredient.show_item_info   = false;
        ingredient.render(window);

        if (sf::FloatRect(slot_pos, slot_size).contains(mouse_pos)) hovered = static_cast<int>(idx);
    }

    if (hovered >= 0)
    {
        renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), recipe.ingredients[hovered], window);
    }
}

void ClientCraftingView::renderCraftButtonOverlays(sf::RenderWindow& window)
{
    const bool craftable = recipes.contains(selected_recipe)
                        && hasIngredients(inventory_stacks, recipes[selected_recipe]);

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

void ClientCraftingView::renderCursor(sf::RenderWindow& window)
{
    if (cursor_stack.empty()) return;

    const float scale = std::min(
        size.x / static_cast<float>(TEXTURE_RECT_WIDTH),
        size.y / static_cast<float>(TEXTURE_RECT_HEIGHT)
    );
    const float slot_size = SLOT_PX_SIZE * scale;

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

void ClientCraftingView::handleContentUpdate(const SetContentData& set_content_data)
{
    if (set_content_data.payload.size() != sizeof(ItemStack)) return;

    ItemStack stack{ItemID::None, 0};
    std::memcpy(&stack, set_content_data.payload.data(), sizeof(ItemStack));

    if (set_content_data.target == CRAFTING_VIEW_CURSOR_SLOT)
    {
        cursor_stack = stack;
    }
    else if (set_content_data.target < 36)
    {
        inventory_stacks[set_content_data.target] = stack;
    }
}

std::deque<ViewEventData> ClientCraftingView::drainPendingEvents()
{
    std::deque<ViewEventData> drained;
    drained.swap(pending);
    return drained;
}
