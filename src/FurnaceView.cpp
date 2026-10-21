#include "../include/FurnaceView.hpp"
#include "../include/FurnaceBlockEntity.hpp"
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

    constexpr float SLOT_PX_SIZE = 20.0f;
    constexpr float SLOT_PX_GAP  = 2.0f;

    constexpr float FUEL_PX_X   = 20.0f;
    constexpr float INPUT_PX_X  = 91.0f;
    constexpr float OUTPUT_PX_X = 162.0f;
    constexpr float TOP_ROW_PX_Y = 31.0f;

    constexpr float PLAYER_GRID_PX_X = 3.0f;
    constexpr float PLAYER_GRID_PX_Y = 94.0f;

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

    std::vector<uint8_t> encodeProgress(float remaining, float total)
    {
        std::vector<uint8_t> buffer(sizeof(float) * 2);
        std::memcpy(buffer.data(),                 &remaining, sizeof(float));
        std::memcpy(buffer.data() + sizeof(float), &total,     sizeof(float));
        return buffer;
    }

    bool decodeProgress(const std::vector<uint8_t>& payload, float& remaining, float& total)
    {
        if (payload.size() != sizeof(float) * 2) return false;
        std::memcpy(&remaining, payload.data(),                 sizeof(float));
        std::memcpy(&total,     payload.data() + sizeof(float), sizeof(float));
        return true;
    }

    FurnaceBlockEntity* getFurnace(World& world, sf::Vector2i position)
    {
        return dynamic_cast<FurnaceBlockEntity*>(world.getBlockEntity(position.x, position.y));
    }

    InventoryComponent* getInventoryComponent(World& world, UUID owner)
    {
        if (!world.doesEntityExist(owner)) return nullptr;

        Entity& entity = world.getEntity(owner);
        if (!entity.hasComponent<InventoryComponent>()) return nullptr;

        return &entity.getComponent<InventoryComponent>();
    }
}

// ---------- ServerFurnaceView ----------

ServerFurnaceView::ServerFurnaceView(sf::Vector2i furnace_position, UUID inventory_owner) : furnace_position{furnace_position}, inventory_owner{inventory_owner}
{
}

void ServerFurnaceView::pushSlotUpdate(uint16_t slot, const ItemStack& stack)
{
    pending.push_back(SetContentData{slot, encodeStack(stack)});
}

void ServerFurnaceView::pushProgressUpdate(uint16_t slot, float remaining, float total)
{
    pending.push_back(SetContentData{slot, encodeProgress(remaining, total)});
}

ItemStack* ServerFurnaceView::resolveSlotTarget(World& world, uint16_t slot)
{
    FurnaceBlockEntity* furnace = getFurnace(world, furnace_position);
    InventoryComponent* inventory = getInventoryComponent(world, inventory_owner);
    if (!furnace || !inventory) return nullptr;

    switch (slot)
    {
        case FURNACE_VIEW_FUEL_SLOT:   return &furnace->fuel;
        case FURNACE_VIEW_INPUT_SLOT:  return &furnace->input;
        case FURNACE_VIEW_OUTPUT_SLOT: return &furnace->output;
        default: break;
    }

    if (slot < FURNACE_VIEW_PLAYER_OFFSET || slot >= FURNACE_VIEW_CURSOR_SLOT) return nullptr;

    const uint16_t index = slot - FURNACE_VIEW_PLAYER_OFFSET;
    if (index >= inventory->inventory.slots.size()) return nullptr;
    return &inventory->inventory.slots[index];
}

void ServerFurnaceView::handleLeftClick(World& world, uint16_t slot)
{
    ItemStack* target = resolveSlotTarget(world, slot);
    if (!target) return;

    ItemStack& stack = *target;

    if (cursor_stack.empty())
    {
        cursor_stack = stack;
        stack = {ItemID::None, 0};
    }
    else if (stack.empty())
    {
        // Can't place items into the output slot by hand.
        if (slot == FURNACE_VIEW_OUTPUT_SLOT) return;

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
        if (slot == FURNACE_VIEW_OUTPUT_SLOT) return;
        std::swap(stack, cursor_stack);
    }

    if (auto* inv = getInventoryComponent(world, inventory_owner)) inv->dirty = true;
}

void ServerFurnaceView::handleRightClick(World& world, uint16_t slot)
{
    ItemStack* target = resolveSlotTarget(world, slot);
    if (!target) return;

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
        if (slot == FURNACE_VIEW_OUTPUT_SLOT) return;

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
        if (slot == FURNACE_VIEW_OUTPUT_SLOT) return;
        std::swap(stack, cursor_stack);
    }

    if (auto* inv = getInventoryComponent(world, inventory_owner)) inv->dirty = true;
}

void ServerFurnaceView::handleViewEvent(World& world, const ViewEventData& event_data)
{
    if (event_data.sender >= FURNACE_VIEW_CURSOR_SLOT) return;

    switch (static_cast<FurnaceViewAction>(event_data.action))
    {
        case FurnaceViewAction::LeftClick:  handleLeftClick(world, event_data.sender);  break;
        case FurnaceViewAction::RightClick: handleRightClick(world, event_data.sender); break;
    }
}

void ServerFurnaceView::update(World& world)
{
    FurnaceBlockEntity* furnace = getFurnace(world, furnace_position);
    InventoryComponent* inventory = getInventoryComponent(world, inventory_owner);

    if (furnace)
    {
        const ItemStack furnace_slots[3] = {furnace->fuel, furnace->input, furnace->output};
        const uint16_t slot_ids[3] = {FURNACE_VIEW_FUEL_SLOT, FURNACE_VIEW_INPUT_SLOT, FURNACE_VIEW_OUTPUT_SLOT};
        for (int i = 0; i < 3; ++i)
        {
            if (!(last_furnace[i] == furnace_slots[i]))
            {
                last_furnace[i] = furnace_slots[i];
                pushSlotUpdate(slot_ids[i], furnace_slots[i]);
            }
        }

        if (last_burn_remaining != furnace->burn_time_remaining || last_burn_total != furnace->burn_time_total)
        {
            last_burn_remaining = furnace->burn_time_remaining;
            last_burn_total     = furnace->burn_time_total;
            pushProgressUpdate(FURNACE_VIEW_BURN_PROGRESS, last_burn_remaining, last_burn_total);
        }

        if (last_cook_remaining != furnace->cook_time_remaining || last_cook_total != furnace->cook_time_total)
        {
            last_cook_remaining = furnace->cook_time_remaining;
            last_cook_total     = furnace->cook_time_total;
            pushProgressUpdate(FURNACE_VIEW_COOK_PROGRESS, last_cook_remaining, last_cook_total);
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
                pushSlotUpdate(FURNACE_VIEW_PLAYER_OFFSET + i, slots[i]);
            }
        }
    }

    if (!(last_cursor == cursor_stack))
    {
        last_cursor = cursor_stack;
        pushSlotUpdate(FURNACE_VIEW_CURSOR_SLOT, cursor_stack);
    }
}

void ServerFurnaceView::onOpen(World& world)
{
    FurnaceBlockEntity* furnace = getFurnace(world, furnace_position);
    InventoryComponent* inventory = getInventoryComponent(world, inventory_owner);

    if (furnace)
    {
        ++furnace->viewers;

        last_furnace[0] = furnace->fuel;
        last_furnace[1] = furnace->input;
        last_furnace[2] = furnace->output;
        pushSlotUpdate(FURNACE_VIEW_FUEL_SLOT,   furnace->fuel);
        pushSlotUpdate(FURNACE_VIEW_INPUT_SLOT,  furnace->input);
        pushSlotUpdate(FURNACE_VIEW_OUTPUT_SLOT, furnace->output);

        last_burn_remaining = furnace->burn_time_remaining;
        last_burn_total     = furnace->burn_time_total;
        last_cook_remaining = furnace->cook_time_remaining;
        last_cook_total     = furnace->cook_time_total;
        pushProgressUpdate(FURNACE_VIEW_BURN_PROGRESS, last_burn_remaining, last_burn_total);
        pushProgressUpdate(FURNACE_VIEW_COOK_PROGRESS, last_cook_remaining, last_cook_total);
    }

    if (inventory)
    {
        const auto& slots = inventory->inventory.slots;
        for (uint16_t i = 0; i < 36; ++i)
        {
            ItemStack stack = (i < slots.size()) ? slots[i] : ItemStack{ItemID::None, 0};
            last_inventory[i] = stack;
            pushSlotUpdate(FURNACE_VIEW_PLAYER_OFFSET + i, stack);
        }
    }

    last_cursor = cursor_stack;
    pushSlotUpdate(FURNACE_VIEW_CURSOR_SLOT, cursor_stack);
}

void ServerFurnaceView::onClose(World& world)
{
    if (FurnaceBlockEntity* furnace = getFurnace(world, furnace_position))
    {
        if (furnace->viewers > 0) --furnace->viewers;
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

bool ServerFurnaceView::shouldClose(World& world) const
{
    constexpr double MAX_INTERACTION_DISTANCE = 5.0;

    if (!world.doesEntityExist(inventory_owner)) return true;

    const Entity& player = world.getEntity(inventory_owner);
    if (!player.hasComponent<TransformComponent>()) return false;

    const auto player_center = player.getComponent<TransformComponent>().center();
    const sf::Vector2<double> furnace_center(furnace_position.x + 0.5, furnace_position.y + 0.5);
    const sf::Vector2<double> delta = player_center - furnace_center;

    return (delta.x * delta.x + delta.y * delta.y) > (MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE);
}

std::deque<SetContentData> ServerFurnaceView::drainPendingContentUpdates()
{
    std::deque<SetContentData> drained;
    drained.swap(pending);
    return drained;
}

// ---------- ClientFurnaceView ----------

ClientFurnaceView::ClientFurnaceView()
{
    for (uint16_t i = 0; i < FURNACE_VIEW_PLAYER_OFFSET + 36; ++i)
    {
        slots[i].show_item_info = true;
    }
}

void ClientFurnaceView::pushClick(uint16_t slot, FurnaceViewAction action, const ItemStack& stack)
{
    std::vector<uint8_t> payload(sizeof(ItemStack));
    std::memcpy(payload.data(), &stack, sizeof(ItemStack));

    pending.push_back(ViewEventData{slot, static_cast<uint8_t>(action), std::move(payload)});
}

void ClientFurnaceView::handleEvent(const sf::Event& event)
{
    const uint16_t total_slots = FURNACE_VIEW_PLAYER_OFFSET + 36;

    for (uint16_t i = 0; i < total_slots; ++i) slots[i].handleEvent(event);

    if (auto button = event.getIf<sf::Event::MouseButtonPressed>())
    {
        for (uint16_t i = 0; i < total_slots; ++i)
        {
            if (!slots[i].getHovered()) continue;

            ItemStack stack;
            if (i < FURNACE_VIEW_PLAYER_OFFSET)
            {
                stack = furnace_stacks[i];
            }
            else
            {
                stack = inventory_stacks[i - FURNACE_VIEW_PLAYER_OFFSET];
            }

            if (button->button == sf::Mouse::Button::Left)
            {
                pushClick(i, FurnaceViewAction::LeftClick, stack);
            }
            else if (button->button == sf::Mouse::Button::Right)
            {
                pushClick(i, FurnaceViewAction::RightClick, stack);
            }

            slots[i].clicked = false;
            slots[i].right_clicked = false;
        }
    }
}

void ClientFurnaceView::update(float)
{
    slots[FURNACE_VIEW_FUEL_SLOT].setItemStack(furnace_stacks[0]);
    slots[FURNACE_VIEW_INPUT_SLOT].setItemStack(furnace_stacks[1]);
    slots[FURNACE_VIEW_OUTPUT_SLOT].setItemStack(furnace_stacks[2]);

    for (uint16_t i = 0; i < 36; ++i)
    {
        slots[FURNACE_VIEW_PLAYER_OFFSET + i].setItemStack(inventory_stacks[i]);
    }
}

void ClientFurnaceView::render(sf::RenderWindow& window)
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
    background.setTexture(&AssetManager::getUITexture(AssetManager::UITextureID::Furnace));
    background.setTextureRect({{0, 0}, {static_cast<int>(TEXTURE_RECT_WIDTH), static_cast<int>(TEXTURE_RECT_HEIGHT)}});
    window.draw(background);

    const float slot_size = SLOT_PX_SIZE * scale;
    const float gap       = SLOT_PX_GAP  * scale;

    auto placeTopSlot = [&](uint16_t slot_index, float px_x)
    {
        sf::Vector2f slot_position = position + sf::Vector2f(px_x * scale, TOP_ROW_PX_Y * scale);
        slots[slot_index].position = slot_position;
        slots[slot_index].size     = {slot_size, slot_size};
        const bool prev_show = slots[slot_index].show_item_info;
        slots[slot_index].show_item_info = false;
        slots[slot_index].render(window);
        slots[slot_index].show_item_info = prev_show;
    };

    placeTopSlot(FURNACE_VIEW_FUEL_SLOT,   FUEL_PX_X);
    placeTopSlot(FURNACE_VIEW_INPUT_SLOT,  INPUT_PX_X);
    placeTopSlot(FURNACE_VIEW_OUTPUT_SLOT, OUTPUT_PX_X);

    for (int i = 35; i >= 0; --i)
    {
        int col = i % 9;
        int row = i / 9;

        sf::Vector2f slot_position = position + sf::Vector2f(
            PLAYER_GRID_PX_X * scale + col * (slot_size + gap),
            PLAYER_GRID_PX_Y * scale + row * (slot_size + gap)
        );

        slots[FURNACE_VIEW_PLAYER_OFFSET + i].position = slot_position;
        slots[FURNACE_VIEW_PLAYER_OFFSET + i].size     = {slot_size, slot_size};

        const bool prev_show = slots[FURNACE_VIEW_PLAYER_OFFSET + i].show_item_info;
        slots[FURNACE_VIEW_PLAYER_OFFSET + i].show_item_info = false;
        slots[FURNACE_VIEW_PLAYER_OFFSET + i].render(window);
        slots[FURNACE_VIEW_PLAYER_OFFSET + i].show_item_info = prev_show;
    }

    renderProgress(window, scale);
    renderCursor(window);

    const uint16_t total_slots = FURNACE_VIEW_PLAYER_OFFSET + 36;
    for (uint16_t i = 0; i < total_slots; ++i)
    {
        if (slots[i].show_item_info && slots[i].getHovered() && !slots[i].item_stack.empty())
        {
            renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), slots[i].item_stack, window);
            break;
        }
    }
}

void ClientFurnaceView::renderProgress(sf::RenderWindow& window, float scale)
{
    const float slot_size = SLOT_PX_SIZE * scale;

    // Flame indicator under the fuel slot.
    {
        float fuel_fraction = (burn_total > 0.0f) ? std::clamp(burn_remaining / burn_total, 0.0f, 1.0f) : 0.0f;

        sf::Vector2f bar_size  = {slot_size * 0.6f, slot_size * 0.25f};
        sf::Vector2f bar_pos   = position + sf::Vector2f(
            (FUEL_PX_X + (SLOT_PX_SIZE - 12.0f) * 0.5f) * scale,
            (TOP_ROW_PX_Y + SLOT_PX_SIZE + 2.0f) * scale
        );

        sf::RectangleShape backdrop(bar_size);
        backdrop.setPosition(bar_pos);
        backdrop.setFillColor(sf::Color(0, 0, 0, 160));
        backdrop.setOutlineThickness(1.0f);
        backdrop.setOutlineColor(sf::Color::Black);
        window.draw(backdrop);

        sf::RectangleShape fill({bar_size.x * fuel_fraction, bar_size.y});
        fill.setPosition(bar_pos);
        fill.setFillColor(sf::Color(255, 160, 32));
        window.draw(fill);
    }

    // Cook arrow between input and output slots.
    {
        float cook_fraction = (cook_total > 0.0f && cook_remaining > 0.0f) ? std::clamp(1.0f - cook_remaining / cook_total, 0.0f, 1.0f) : 0.0f;

        const float bar_px_x = INPUT_PX_X + SLOT_PX_SIZE + 2.0f;
        const float bar_px_w = OUTPUT_PX_X - (INPUT_PX_X + SLOT_PX_SIZE) - 4.0f;
        const float bar_px_h = 6.0f;
        const float bar_px_y = TOP_ROW_PX_Y + (SLOT_PX_SIZE - bar_px_h) * 0.5f;

        sf::Vector2f bar_size = {bar_px_w * scale, bar_px_h * scale};
        sf::Vector2f bar_pos  = position + sf::Vector2f(bar_px_x * scale, bar_px_y * scale);

        sf::RectangleShape backdrop(bar_size);
        backdrop.setPosition(bar_pos);
        backdrop.setFillColor(sf::Color(0, 0, 0, 160));
        backdrop.setOutlineThickness(1.0f);
        backdrop.setOutlineColor(sf::Color::Black);
        window.draw(backdrop);

        sf::RectangleShape fill({bar_size.x * cook_fraction, bar_size.y});
        fill.setPosition(bar_pos);
        fill.setFillColor(sf::Color(220, 220, 220));
        window.draw(fill);
    }
}

void ClientFurnaceView::renderCursor(sf::RenderWindow& window)
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

void ClientFurnaceView::handleContentUpdate(const SetContentData& set_content_data)
{
    if (set_content_data.target == FURNACE_VIEW_BURN_PROGRESS)
    {
        decodeProgress(set_content_data.payload, burn_remaining, burn_total);
        return;
    }
    if (set_content_data.target == FURNACE_VIEW_COOK_PROGRESS)
    {
        decodeProgress(set_content_data.payload, cook_remaining, cook_total);
        return;
    }

    ItemStack stack{ItemID::None, 0};
    if (!decodeStack(set_content_data.payload, stack)) return;

    if (set_content_data.target == FURNACE_VIEW_CURSOR_SLOT)
    {
        cursor_stack = stack;
    }
    else if (set_content_data.target < FURNACE_VIEW_PLAYER_OFFSET)
    {
        furnace_stacks[set_content_data.target] = stack;
    }
    else if (set_content_data.target < FURNACE_VIEW_CURSOR_SLOT)
    {
        inventory_stacks[set_content_data.target - FURNACE_VIEW_PLAYER_OFFSET] = stack;
    }
}

std::deque<ViewEventData> ClientFurnaceView::drainPendingEvents()
{
    std::deque<ViewEventData> drained;
    drained.swap(pending);
    return drained;
}
