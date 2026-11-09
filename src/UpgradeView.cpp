#include "../include/UpgradeView.hpp"

#include "../include/World.hpp"
#include "../include/Entity.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/ItemUtils.hpp"
#include "../include/ItemDataComponents.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Render.hpp"
#include "../include/Prefab.hpp"

#include <algorithm>

namespace
{
#ifndef HEADLESS_BUILD
    constexpr unsigned int TEXTURE_RECT_WIDTH  = 202;
    constexpr unsigned int TEXTURE_RECT_HEIGHT = 113;

    constexpr float SLOT_PX_SIZE = 20.0f;
    constexpr float SLOT_PX_GAP  = 2.0f;

    // Header slots are centred inside 14x14 cutouts at the pixel coords found
    // directly on upgrade_view.png: x = 40, 94, 148 and y = 4. We draw slots at
    // the texture's 20x20 convention anyway, centred on the cutout so the
    // in-slot item icon lines up visually with the cutout border.
    constexpr float INPUT_PX_X  = 37.0f;
    constexpr float RUBY_PX_X   = 91.0f;
    constexpr float OUTPUT_PX_X = 145.0f;
    constexpr float TOP_ROW_PX_Y = 1.0f;

    // The body (y 21..111) is the backdrop for the player inventory grid.
    // 9x4 of 20x20 slots with 2px gaps = 196x86, centred inside 200x90.
    constexpr float PLAYER_GRID_PX_X = 3.0f;
    constexpr float PLAYER_GRID_PX_Y = 23.0f;
#endif

    std::vector<uint8_t> encodeStack(const ItemStack& stack)
    {
        return stackToBytes(stack);
    }

    InventoryComponent* getInventoryComponent(World& world, UUID owner)
    {
        if (!world.doesEntityExist(owner)) return nullptr;

        Entity& entity = world.getEntity(owner);
        if (!entity.hasComponent<InventoryComponent>()) return nullptr;

        return &entity.getComponent<InventoryComponent>();
    }

    bool isUpgradableTool(ItemID id)
    {
        return id == ItemID::Ruby_Pickaxe || id == ItemID::Ruby_Axe || id == ItemID::Ruby_Shovel;
    }
}

// ---------- ServerUpgradeView ----------

ServerUpgradeView::ServerUpgradeView(sf::Vector2i table_position, UUID inventory_owner) : table_position{table_position}, inventory_owner{inventory_owner}
{
}

void ServerUpgradeView::pushSlotUpdate(uint16_t target, const ItemStack& stack)
{
    pending.push_back(SetContentData{target, encodeStack(stack)});
}

ItemStack ServerUpgradeView::computeOutputPreview() const
{
    // The output slot is a read-only mirror: it shows the input item at
    // level+1 when there is enough ruby in the catalyst slot. Nothing is
    // actually consumed until the player clicks the output.
    if (!isUpgradableTool(input_stack.itemID)) return {ItemID::None, 0};
    if (input_stack.quantity == 0) return {ItemID::None, 0};

    uint8_t current = getRubynessLevel(input_stack);
    if (current >= RUBYNESS_MAX_LEVEL) return {ItemID::None, 0};

    uint32_t cost = current;  // alpha(1)->beta = 1 ruby, beta(2)->gamma = 2, ...
    if (ruby_stack.itemID != ItemID::Ruby || ruby_stack.quantity < cost) return {ItemID::None, 0};

    // Preview stack: same item, quantity 1, no instanceID — the preview is
    // transient and shouldn't be confused with a real stacked item.
    return {input_stack.itemID, 1};
}

ItemStack* ServerUpgradeView::resolveSlotTarget(World& world, uint16_t slot)
{
    switch (slot)
    {
        case UPGRADE_VIEW_INPUT_SLOT:  return &input_stack;
        case UPGRADE_VIEW_RUBY_SLOT:   return &ruby_stack;
        case UPGRADE_VIEW_OUTPUT_SLOT: return nullptr;  // handled separately
        default: break;
    }

    InventoryComponent* inv = getInventoryComponent(world, inventory_owner);
    if (!inv) return nullptr;

    if (slot < UPGRADE_VIEW_PLAYER_OFFSET || slot >= UPGRADE_VIEW_CURSOR_SLOT) return nullptr;

    const uint16_t index = slot - UPGRADE_VIEW_PLAYER_OFFSET;
    if (index >= inv->inventory.slots.size()) return nullptr;
    return &inv->inventory.slots[index];
}

void ServerUpgradeView::takeOutput(World& world)
{
    if (!isUpgradableTool(input_stack.itemID) || input_stack.quantity == 0) return;

    uint8_t current = getRubynessLevel(input_stack);
    if (current >= RUBYNESS_MAX_LEVEL) return;

    uint32_t cost = current;
    if (ruby_stack.itemID != ItemID::Ruby || ruby_stack.quantity < cost) return;

    // The upgraded tool has to be the one in the input slot (keeping its
    // durability / enchantments). We just bump its Rubyness level and hand
    // it to the cursor or merge with whatever is already there.
    ItemStack upgraded = input_stack;
    setRubynessLevel(upgraded, static_cast<uint8_t>(current + 1));

    if (cursor_stack.empty())
    {
        cursor_stack = upgraded;
    }
    else if (cursor_stack.itemID == upgraded.itemID && !cursor_stack.isUnique() && !upgraded.isUnique())
    {
        uint32_t max_stack = itemDatabase[upgraded.itemID].maxStackSize;
        uint32_t can_add = std::min(upgraded.quantity, (max_stack > cursor_stack.quantity) ? (max_stack - cursor_stack.quantity) : 0u);
        if (can_add == 0) return;
        cursor_stack.quantity += can_add;
    }
    else
    {
        return;  // cursor holds something incompatible; don't drop the upgrade
    }

    // Consume exactly one tool and the ruby cost.
    input_stack.quantity -= 1;
    if (input_stack.quantity == 0) input_stack = {ItemID::None, 0};

    ruby_stack.quantity -= cost;
    if (ruby_stack.quantity == 0) ruby_stack = {ItemID::None, 0};

    if (auto* inv = getInventoryComponent(world, inventory_owner)) inv->dirty = true;
}

void ServerUpgradeView::handleLeftClick(World& world, uint16_t slot)
{
    if (slot == UPGRADE_VIEW_OUTPUT_SLOT)
    {
        takeOutput(world);
        return;
    }

    ItemStack* target = resolveSlotTarget(world, slot);
    if (!target) return;

    // Slot-type guards: input slot only accepts ruby tools, ruby slot only
    // accepts Ruby material. Non-conforming swaps are refused silently.
    if (slot == UPGRADE_VIEW_INPUT_SLOT
        && !cursor_stack.empty()
        && !isUpgradableTool(cursor_stack.itemID))
    {
        return;
    }
    if (slot == UPGRADE_VIEW_RUBY_SLOT
        && !cursor_stack.empty()
        && cursor_stack.itemID != ItemID::Ruby)
    {
        return;
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

    if (auto* inv = getInventoryComponent(world, inventory_owner)) inv->dirty = true;
}

void ServerUpgradeView::handleRightClick(World& world, uint16_t slot)
{
    if (slot == UPGRADE_VIEW_OUTPUT_SLOT) return;

    ItemStack* target = resolveSlotTarget(world, slot);
    if (!target) return;

    if (slot == UPGRADE_VIEW_INPUT_SLOT
        && !cursor_stack.empty()
        && !isUpgradableTool(cursor_stack.itemID))
    {
        return;
    }
    if (slot == UPGRADE_VIEW_RUBY_SLOT
        && !cursor_stack.empty()
        && cursor_stack.itemID != ItemID::Ruby)
    {
        return;
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

    if (auto* inv = getInventoryComponent(world, inventory_owner)) inv->dirty = true;
}

void ServerUpgradeView::handleViewEvent(World& world, const ViewEventData& event_data)
{
    if (event_data.sender >= UPGRADE_VIEW_CURSOR_SLOT) return;

    switch (static_cast<UpgradeViewAction>(event_data.action))
    {
        case UpgradeViewAction::LeftClick:  handleLeftClick(world, event_data.sender);  break;
        case UpgradeViewAction::RightClick: handleRightClick(world, event_data.sender); break;
    }
}

void ServerUpgradeView::update(World& world)
{
    if (!(last_input == input_stack))
    {
        last_input = input_stack;
        pushSlotUpdate(UPGRADE_VIEW_INPUT_SLOT, input_stack);
    }
    if (!(last_ruby == ruby_stack))
    {
        last_ruby = ruby_stack;
        pushSlotUpdate(UPGRADE_VIEW_RUBY_SLOT, ruby_stack);
    }

    ItemStack output = computeOutputPreview();
    if (!(last_output == output))
    {
        last_output = output;
        pushSlotUpdate(UPGRADE_VIEW_OUTPUT_SLOT, output);
    }

    if (InventoryComponent* inv = getInventoryComponent(world, inventory_owner))
    {
        const auto& slots = inv->inventory.slots;
        for (uint16_t i = 0; i < 36 && i < slots.size(); ++i)
        {
            if (!(last_inventory[i] == slots[i]))
            {
                last_inventory[i] = slots[i];
                pushSlotUpdate(UPGRADE_VIEW_PLAYER_OFFSET + i, slots[i]);
            }
        }
    }

    if (!(last_cursor == cursor_stack))
    {
        last_cursor = cursor_stack;
        pushSlotUpdate(UPGRADE_VIEW_CURSOR_SLOT, cursor_stack);
    }
}

void ServerUpgradeView::onOpen(World& world)
{
    last_input  = input_stack;
    last_ruby   = ruby_stack;
    last_output = computeOutputPreview();
    pushSlotUpdate(UPGRADE_VIEW_INPUT_SLOT,  input_stack);
    pushSlotUpdate(UPGRADE_VIEW_RUBY_SLOT,   ruby_stack);
    pushSlotUpdate(UPGRADE_VIEW_OUTPUT_SLOT, last_output);

    if (InventoryComponent* inv = getInventoryComponent(world, inventory_owner))
    {
        const auto& slots = inv->inventory.slots;
        for (uint16_t i = 0; i < 36; ++i)
        {
            ItemStack stack = (i < slots.size()) ? slots[i] : ItemStack{ItemID::None, 0};
            last_inventory[i] = stack;
            pushSlotUpdate(UPGRADE_VIEW_PLAYER_OFFSET + i, stack);
        }
    }

    last_cursor = cursor_stack;
    pushSlotUpdate(UPGRADE_VIEW_CURSOR_SLOT, cursor_stack);
}

void ServerUpgradeView::onClose(World& world)
{
    if (!world.doesEntityExist(inventory_owner)) return;

    auto& player = world.getEntity(inventory_owner);
    if (!player.hasComponent<InventoryComponent>()) return;
    auto& inventory = player.getComponent<InventoryComponent>().inventory;

    // Give the player back anything left in the view: input, ruby, cursor.
    auto reclaim = [&](ItemStack& stack)
    {
        if (stack.empty()) return;
        ItemStack leftover = inventory.addStackWithLeftover(stack);
        stack = {ItemID::None, 0};
        if (!leftover.empty() && player.hasComponent<TransformComponent>())
        {
            const auto& transform = player.getComponent<TransformComponent>();
            spawnDroppedItem(world, transform.position + sf::Vector2<double>(0.0, 1.5), leftover, randomDropSpread(45.0f));
        }
    };

    reclaim(input_stack);
    reclaim(ruby_stack);
    reclaim(cursor_stack);

    player.getComponent<InventoryComponent>().dirty = true;
}

bool ServerUpgradeView::shouldClose(World& world) const
{
    constexpr double MAX_INTERACTION_DISTANCE = 5.0;

    if (!world.doesEntityExist(inventory_owner)) return true;

    const Entity& player = world.getEntity(inventory_owner);
    if (!player.hasComponent<TransformComponent>()) return false;

    const auto player_center = player.getComponent<TransformComponent>().center();
    const sf::Vector2<double> block_center(table_position.x + 0.5, table_position.y + 0.5);
    const sf::Vector2<double> delta = player_center - block_center;

    return (delta.x * delta.x + delta.y * delta.y) > (MAX_INTERACTION_DISTANCE * MAX_INTERACTION_DISTANCE);
}

std::deque<SetContentData> ServerUpgradeView::drainPendingContentUpdates()
{
    std::deque<SetContentData> drained;
    drained.swap(pending);
    return drained;
}

void ServerUpgradeView::collectLiveInstanceIDs(std::unordered_set<UUID>& out) const
{
    if (cursor_stack.instanceID.has_value()) out.insert(*cursor_stack.instanceID);
    if (input_stack.instanceID.has_value())  out.insert(*input_stack.instanceID);
    if (ruby_stack.instanceID.has_value())   out.insert(*ruby_stack.instanceID);
}

#ifndef HEADLESS_BUILD
// ---------- ClientUpgradeView ----------

ClientUpgradeView::ClientUpgradeView()
{
    for (uint16_t i = 0; i < UPGRADE_VIEW_PLAYER_OFFSET + 36; ++i)
    {
        slots[i].show_item_info = true;
    }
}

void ClientUpgradeView::pushClick(uint16_t slot, UpgradeViewAction action, const ItemStack& stack)
{
    pending.push_back(ViewEventData{slot, static_cast<uint8_t>(action), stackToBytes(stack)});
}

void ClientUpgradeView::handleEvent(const sf::Event& event)
{
    const uint16_t total_slots = UPGRADE_VIEW_PLAYER_OFFSET + 36;

    for (uint16_t i = 0; i < total_slots; ++i) slots[i].handleEvent(event);

    if (auto button = event.getIf<sf::Event::MouseButtonPressed>())
    {
        for (uint16_t i = 0; i < total_slots; ++i)
        {
            if (!slots[i].getHovered()) continue;

            ItemStack stack;
            switch (i)
            {
                case UPGRADE_VIEW_INPUT_SLOT:  stack = input_stack;  break;
                case UPGRADE_VIEW_RUBY_SLOT:   stack = ruby_stack;   break;
                case UPGRADE_VIEW_OUTPUT_SLOT: stack = output_stack; break;
                default: stack = inventory_stacks[i - UPGRADE_VIEW_PLAYER_OFFSET]; break;
            }

            if (button->button == sf::Mouse::Button::Left)
            {
                pushClick(i, UpgradeViewAction::LeftClick, stack);
            }
            else if (button->button == sf::Mouse::Button::Right)
            {
                pushClick(i, UpgradeViewAction::RightClick, stack);
            }

            slots[i].clicked = false;
            slots[i].right_clicked = false;
        }
    }
}

void ClientUpgradeView::update(float)
{
    slots[UPGRADE_VIEW_INPUT_SLOT].setItemStack(input_stack);
    slots[UPGRADE_VIEW_RUBY_SLOT].setItemStack(ruby_stack);
    slots[UPGRADE_VIEW_OUTPUT_SLOT].setItemStack(output_stack);

    for (uint16_t i = 0; i < 36; ++i)
    {
        slots[UPGRADE_VIEW_PLAYER_OFFSET + i].setItemStack(inventory_stacks[i]);
    }
}

void ClientUpgradeView::render(sf::RenderWindow& window)
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
    background.setTexture(&AssetManager::getUITexture(AssetManager::UITextureID::Upgrade));
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

    placeTopSlot(UPGRADE_VIEW_INPUT_SLOT,  INPUT_PX_X);
    placeTopSlot(UPGRADE_VIEW_RUBY_SLOT,   RUBY_PX_X);
    placeTopSlot(UPGRADE_VIEW_OUTPUT_SLOT, OUTPUT_PX_X);

    for (int i = 35; i >= 0; --i)
    {
        int col = i % 9;
        int row = i / 9;

        sf::Vector2f slot_position = position + sf::Vector2f(
            PLAYER_GRID_PX_X * scale + col * (slot_size + gap),
            PLAYER_GRID_PX_Y * scale + row * (slot_size + gap)
        );

        slots[UPGRADE_VIEW_PLAYER_OFFSET + i].position = slot_position;
        slots[UPGRADE_VIEW_PLAYER_OFFSET + i].size     = {slot_size, slot_size};

        const bool prev_show = slots[UPGRADE_VIEW_PLAYER_OFFSET + i].show_item_info;
        slots[UPGRADE_VIEW_PLAYER_OFFSET + i].show_item_info = false;
        slots[UPGRADE_VIEW_PLAYER_OFFSET + i].render(window);
        slots[UPGRADE_VIEW_PLAYER_OFFSET + i].show_item_info = prev_show;
    }

    renderCursor(window);

    const uint16_t total_slots = UPGRADE_VIEW_PLAYER_OFFSET + 36;
    for (uint16_t i = 0; i < total_slots; ++i)
    {
        if (slots[i].show_item_info && slots[i].getHovered() && !slots[i].item_stack.empty())
        {
            renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), slots[i].item_stack, window);
            break;
        }
    }
}

void ClientUpgradeView::renderCursor(sf::RenderWindow& window)
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

void ClientUpgradeView::handleContentUpdate(const SetContentData& set_content_data)
{
    auto parsed = stackFromBytes(set_content_data.payload, /*write_instance_data=*/true);
    if (!parsed) return;

    ItemStack stack = *parsed;

    switch (set_content_data.target)
    {
        case UPGRADE_VIEW_INPUT_SLOT:  input_stack  = stack; break;
        case UPGRADE_VIEW_RUBY_SLOT:   ruby_stack   = stack; break;
        case UPGRADE_VIEW_OUTPUT_SLOT: output_stack = stack; break;
        case UPGRADE_VIEW_CURSOR_SLOT: cursor_stack = stack; break;
        default:
            if (set_content_data.target >= UPGRADE_VIEW_PLAYER_OFFSET
             && set_content_data.target < UPGRADE_VIEW_CURSOR_SLOT)
            {
                inventory_stacks[set_content_data.target - UPGRADE_VIEW_PLAYER_OFFSET] = stack;
            }
            break;
    }
}

std::deque<ViewEventData> ClientUpgradeView::drainPendingEvents()
{
    std::deque<ViewEventData> drained;
    drained.swap(pending);
    return drained;
}
#endif // HEADLESS_BUILD
