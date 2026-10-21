#include "../include/ChestView.hpp"
#include "../include/ChestBlockEntity.hpp"
#include "../include/World.hpp"
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

namespace
{
    constexpr unsigned int TEXTURE_RECT_WIDTH  = 202;
    constexpr unsigned int TEXTURE_RECT_HEIGHT = 183;

    constexpr float CHEST_GRID_PX_X  = 3.0f;
    constexpr float CHEST_GRID_PX_Y  = 3.0f;
    constexpr float PLAYER_GRID_PX_X = 3.0f;
    constexpr float PLAYER_GRID_PX_Y = 94.0f;

    constexpr float SLOT_PX_SIZE = 20.0f;
    constexpr float SLOT_PX_GAP  = 2.0f;

    std::vector<uint8_t> encodeStack(const ItemStack& stack)
    {
        std::vector<uint8_t> buffer(sizeof(ItemStack));
        std::memcpy(buffer.data(), &stack, sizeof(ItemStack));
        return buffer;
    }

    bool decodeStack(const std::vector<uint8_t>& payload, ItemStack& out)
    {
        if (payload.size() != sizeof(ItemStack)) return false;
        std::memcpy(&out, payload.data(), sizeof(ItemStack));
        return true;
    }

    ChestBlockEntity* getChest(World& world, sf::Vector2i position)
    {
        return dynamic_cast<ChestBlockEntity*>(world.getBlockEntity(position.x, position.y));
    }

    InventoryComponent* getInventoryComponent(World& world, UUID owner)
    {
        if (!world.doesEntityExist(owner)) return nullptr;

        Entity& entity = world.getEntity(owner);
        if (!entity.hasComponent<InventoryComponent>()) return nullptr;

        return &entity.getComponent<InventoryComponent>();
    }
}

// ---------- ServerChestView ----------

ServerChestView::ServerChestView(sf::Vector2i chest_position, UUID inventory_owner) : chest_position{chest_position}, inventory_owner{inventory_owner}
{
}

void ServerChestView::pushSlotUpdate(uint16_t slot, const ItemStack& stack)
{
    pending.push_back(SetContentData{slot, encodeStack(stack)});
}

void ServerChestView::handleLeftClick(World& world, uint16_t slot)
{
    ChestBlockEntity* chest = getChest(world, chest_position);
    InventoryComponent* inventory = getInventoryComponent(world, inventory_owner);
    if (!chest || !inventory) return;

    ItemStack* target = nullptr;
    if (slot < CHEST_VIEW_PLAYER_OFFSET)
    {
        if (slot >= chest->stacks.size()) return;
        target = &chest->stacks[slot];
    }
    else
    {
        const uint16_t index = slot - CHEST_VIEW_PLAYER_OFFSET;
        if (index >= inventory->inventory.slots.size()) return;
        target = &inventory->inventory.slots[index];
    }

    ItemStack& stack = *target;

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

    inventory->dirty = true;
}

void ServerChestView::handleRightClick(World& world, uint16_t slot)
{
    ChestBlockEntity* chest = getChest(world, chest_position);
    InventoryComponent* inventory = getInventoryComponent(world, inventory_owner);
    if (!chest || !inventory) return;

    ItemStack* target = nullptr;
    if (slot < CHEST_VIEW_PLAYER_OFFSET)
    {
        if (slot >= chest->stacks.size()) return;
        target = &chest->stacks[slot];
    }
    else
    {
        const uint16_t index = slot - CHEST_VIEW_PLAYER_OFFSET;
        if (index >= inventory->inventory.slots.size()) return;
        target = &inventory->inventory.slots[index];
    }

    ItemStack& stack = *target;

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

    inventory->dirty = true;
}

void ServerChestView::handleViewEvent(World& world, const ViewEventData& event_data)
{
    if (event_data.sender >= CHEST_VIEW_CURSOR_SLOT) return;

    switch (static_cast<ChestViewAction>(event_data.action))
    {
        case ChestViewAction::LeftClick:  handleLeftClick(world, event_data.sender);  break;
        case ChestViewAction::RightClick: handleRightClick(world, event_data.sender); break;
    }
}

void ServerChestView::update(World& world)
{
    ChestBlockEntity* chest = getChest(world, chest_position);
    InventoryComponent* inventory = getInventoryComponent(world, inventory_owner);

    if (chest)
    {
        const auto& stacks = chest->stacks;
        for (uint16_t i = 0; i < 36 && i < stacks.size(); ++i)
        {
            if (!(last_chest[i] == stacks[i]))
            {
                last_chest[i] = stacks[i];
                chest_stacks[i] = stacks[i];
                pushSlotUpdate(CHEST_VIEW_CHEST_OFFSET + i, stacks[i]);
            }
        }
    }

    if (inventory)
    {
        const auto& slots = inventory->inventory.slots;
        for (uint16_t i = 0; i < 36 && i < slots.size(); ++i)
        {
            if (!(last_inventory[i] == slots[i]))
            {
                last_inventory[i] = slots[i];
                inventory_stacks[i] = slots[i];
                pushSlotUpdate(CHEST_VIEW_PLAYER_OFFSET + i, slots[i]);
            }
        }
    }

    if (!(last_cursor == cursor_stack))
    {
        last_cursor = cursor_stack;
        pushSlotUpdate(CHEST_VIEW_CURSOR_SLOT, cursor_stack);
    }
}

void ServerChestView::onOpen(World& world)
{
    ChestBlockEntity* chest = getChest(world, chest_position);
    InventoryComponent* inventory = getInventoryComponent(world, inventory_owner);

    if (chest)
    {
        ++chest->viewers;

        // Mirror open state onto the block's metadata so clients can render it
        // without needing to know about block entities.
        Block current = world.getBlock(chest_position.x, chest_position.y);
        if (current.id == BlockID::Chest && current.metadata != 1)
        {
            world.setBlock(chest_position.x, chest_position.y, {BlockID::Chest, 1});
        }

        for (uint16_t i = 0; i < 36 && i < chest->stacks.size(); ++i)
        {
            last_chest[i] = chest->stacks[i];
            chest_stacks[i] = chest->stacks[i];
            pushSlotUpdate(CHEST_VIEW_CHEST_OFFSET + i, chest->stacks[i]);
        }
    }

    if (inventory)
    {
        const auto& slots = inventory->inventory.slots;
        for (uint16_t i = 0; i < 36; ++i)
        {
            ItemStack stack = (i < slots.size()) ? slots[i] : ItemStack{ItemID::None, 0};
            last_inventory[i] = stack;
            inventory_stacks[i] = stack;
            pushSlotUpdate(CHEST_VIEW_PLAYER_OFFSET + i, stack);
        }
    }

    last_cursor = cursor_stack;
    pushSlotUpdate(CHEST_VIEW_CURSOR_SLOT, cursor_stack);
}

void ServerChestView::onClose(World& world)
{
    ChestBlockEntity* chest = getChest(world, chest_position);
    if (chest)
    {
        if (chest->viewers > 0) --chest->viewers;

        if (chest->viewers == 0)
        {
            Block current = world.getBlock(chest_position.x, chest_position.y);
            if (current.id == BlockID::Chest && current.metadata != 0)
            {
                world.setBlock(chest_position.x, chest_position.y, {BlockID::Chest, 0});
            }
        }
    }

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

bool ServerChestView::shouldClose(World& world) const
{
    constexpr double MAX_INTERACTION_DISTANCE = 5.0;

    if (!world.doesEntityExist(inventory_owner)) return true;

    const Entity& player = world.getEntity(inventory_owner);
    if (!player.hasComponent<TransformComponent>()) return false;

    const auto player_center = player.getComponent<TransformComponent>().center();
    const sf::Vector2<double> chest_center(chest_position.x + 0.5, chest_position.y + 0.5);
    const sf::Vector2<double> delta = player_center - chest_center;

    return (delta.x * delta.x + delta.y * delta.y) > (MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE);
}

std::deque<SetContentData> ServerChestView::drainPendingContentUpdates()
{
    std::deque<SetContentData> drained;
    drained.swap(pending);
    return drained;
}

// ---------- ClientChestView ----------

ClientChestView::ClientChestView()
{
    for (uint16_t i = 0; i < CHEST_VIEW_PLAYER_OFFSET + 36; ++i)
    {
        slots[i].show_item_info = true;
    }
}

void ClientChestView::pushClick(uint16_t slot, ChestViewAction action, const ItemStack& stack)
{
    std::vector<uint8_t> payload(sizeof(ItemStack));
    std::memcpy(payload.data(), &stack, sizeof(ItemStack));

    pending.push_back(ViewEventData{slot, static_cast<uint8_t>(action), std::move(payload)});
}

void ClientChestView::handleEvent(const sf::Event& event)
{
    const uint16_t total_slots = CHEST_VIEW_PLAYER_OFFSET + 36;

    for (uint16_t i = 0; i < total_slots; ++i) slots[i].handleEvent(event);

    if (auto button = event.getIf<sf::Event::MouseButtonPressed>())
    {
        for (uint16_t i = 0; i < total_slots; ++i)
        {
            if (!slots[i].getHovered()) continue;

            const ItemStack& stack = (i < CHEST_VIEW_PLAYER_OFFSET)
                ? chest_stacks[i]
                : inventory_stacks[i - CHEST_VIEW_PLAYER_OFFSET];

            if (button->button == sf::Mouse::Button::Left)
            {
                pushClick(i, ChestViewAction::LeftClick, stack);
            }
            else if (button->button == sf::Mouse::Button::Right)
            {
                pushClick(i, ChestViewAction::RightClick, stack);
            }

            slots[i].clicked = false;
            slots[i].right_clicked = false;
        }
    }
}

void ClientChestView::update(float dt)
{
    for (uint16_t i = 0; i < CHEST_VIEW_PLAYER_OFFSET; ++i) slots[i].setItemStack(chest_stacks[i]);
    for (uint16_t i = 0; i < 36; ++i)                      slots[CHEST_VIEW_PLAYER_OFFSET + i].setItemStack(inventory_stacks[i]);
}

void ClientChestView::render(sf::RenderWindow& window)
{
    sf::Vector2u window_size = window.getSize();

    const float scale = std::min(
        (window_size.x * 0.75f) / static_cast<float>(TEXTURE_RECT_WIDTH),
        (window_size.y * 0.9f)  / static_cast<float>(TEXTURE_RECT_HEIGHT)
    );

    size = {static_cast<float>(TEXTURE_RECT_WIDTH) * scale, static_cast<float>(TEXTURE_RECT_HEIGHT) * scale};
    position = {(window_size.x - size.x) * 0.5f, (window_size.y - size.y) * 0.5f};

    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setTexture(&AssetManager::getUITexture(AssetManager::UITextureID::Chest));
    background.setTextureRect({{0, 0}, {static_cast<int>(TEXTURE_RECT_WIDTH), static_cast<int>(TEXTURE_RECT_HEIGHT)}});
    window.draw(background);

    const float slot_size = SLOT_PX_SIZE * scale;
    const float gap       = SLOT_PX_GAP  * scale;

    auto layoutGrid = [&](uint16_t offset, uint16_t count, int cols, float px_x, float px_y)
    {
        for (int i = count - 1; i >= 0; --i)
        {
            int col = i % cols;
            int row = i / cols;

            sf::Vector2f slot_position = position + sf::Vector2f(
                px_x * scale + col * (slot_size + gap),
                px_y * scale + row * (slot_size + gap)
            );

            slots[offset + i].position = slot_position;
            slots[offset + i].size     = {slot_size, slot_size};

            const bool prev_show = slots[offset + i].show_item_info;
            slots[offset + i].show_item_info = false;
            slots[offset + i].render(window);
            slots[offset + i].show_item_info = prev_show;
        }
    };

    layoutGrid(CHEST_VIEW_CHEST_OFFSET,  36, 9, CHEST_GRID_PX_X,  CHEST_GRID_PX_Y);
    layoutGrid(CHEST_VIEW_PLAYER_OFFSET, 36, 9, PLAYER_GRID_PX_X, PLAYER_GRID_PX_Y);

    renderCursor(window);

    const uint16_t total_slots = CHEST_VIEW_PLAYER_OFFSET + 36;
    for (uint16_t i = 0; i < total_slots; ++i)
    {
        if (slots[i].show_item_info && slots[i].getHovered() && !slots[i].item_stack.empty())
        {
            renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), slots[i].item_stack, window);
            break;
        }
    }
}

void ClientChestView::renderCursor(sf::RenderWindow& window)
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

void ClientChestView::handleContentUpdate(const SetContentData& set_content_data)
{
    ItemStack stack{ItemID::None, 0};
    if (!decodeStack(set_content_data.payload, stack)) return;

    if (set_content_data.target == CHEST_VIEW_CURSOR_SLOT)
    {
        cursor_stack = stack;
    }
    else if (set_content_data.target < CHEST_VIEW_PLAYER_OFFSET)
    {
        chest_stacks[set_content_data.target] = stack;
    }
    else if (set_content_data.target < CHEST_VIEW_CURSOR_SLOT)
    {
        inventory_stacks[set_content_data.target - CHEST_VIEW_PLAYER_OFFSET] = stack;
    }
}

std::deque<ViewEventData> ClientChestView::drainPendingEvents()
{
    std::deque<ViewEventData> drained;
    drained.swap(pending);
    return drained;
}
