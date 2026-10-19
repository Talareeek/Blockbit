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

#include <algorithm>
#include <cstring>

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

void ServerInventoryView::handleCraft(World& world, const ItemStack& requested)
{
    if (!recipes.contains(requested)) return;
    if (!world.doesEntityExist(owner)) return;

    auto& entity = world.getEntity(owner);
    if (!entity.hasComponent<InventoryComponent>()) return;
    auto& component = entity.getComponent<InventoryComponent>();
    auto& inventory = component.inventory;

    const auto& recipe = recipes[requested];

    for (const auto& ingredient : recipe.ingredients)
    {
        uint32_t available = 0;
        for (const auto& slot : inventory.slots)
        {
            if (slot.itemID == ingredient.itemID) available += slot.quantity;
            if (available >= ingredient.quantity) break;
        }
        if (available < ingredient.quantity) return;
    }

    for (const auto& ingredient : recipe.ingredients)
    {
        inventory.removeItemWithLeftover(ingredient.itemID, ingredient.quantity);
    }

    inventory.addItemWithLeftover(requested.itemID, requested.quantity);
    component.dirty = true;
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

    Entity drop(generateUUID());
    drop.addComponent(TransformComponent{transform.position + sf::Vector2<double>(0.0, 1.5), {0.5, 0.5}, sf::degrees(0.0f)});
    drop.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
    drop.addComponent(RenderComponent{itemDatabase[leftover.itemID].texture, {{0, 0}, {16, 16}}, {0.5f, 0.5f}});
    drop.addComponent(ItemComponent{leftover});

    world.addEntity(std::move(drop));
}

std::deque<SetContentData> ServerInventoryView::drainPendingContentUpdates()
{
    std::deque<SetContentData> drained;
    drained.swap(pending);
    return drained;
}

// ---------- ClientInventoryView ----------

namespace
{
    constexpr int PAGES = 2;
    constexpr unsigned int TEXTURE_RECT_WIDTH  = 202;
    constexpr unsigned int TEXTURE_RECT_HEIGHT = 117;

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
        Slot slot({0.0f, 0.0f}, {0.0f, 0.0f});
        slot.setItemStack(recipe_stack);
        slot.show_item_info = false;
        crafting_slots.push_back(slot);
    }

    crafting_button = Button({0.0f, 0.0f}, {0.0f, 0.0f}, sf::Color::Black, "Craft", [this](){ craft_clicked = true; });
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
        for (auto& slot : crafting_slots)
        {
            slot.handleEvent(event);

            if (slot.clicked)
            {
                slot.clicked = false;
                selected_recipe = slot.item_stack;
            }
        }

        crafting_button.handleEvent(event);

        if (craft_clicked && recipes.contains(selected_recipe))
        {
            if (hasIngredients(slot_stacks, INVENTORY_VIEW_SLOT_COUNT, recipes[selected_recipe]))
            {
                std::vector<uint8_t> payload(sizeof(ItemStack));
                std::memcpy(payload.data(), &selected_recipe, sizeof(ItemStack));
                pending.push_back(ViewEventData{0, static_cast<uint8_t>(InventoryViewAction::Craft), std::move(payload)});
            }

            craft_clicked = false;
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
        slot.background_color = craftable ? sf::Color::Green : sf::Color::Red;
    }

    crafting_button.update(dt);
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
        for (int i = static_cast<int>(crafting_slots.size()) - 1; i >= 0; --i)
        {
            int col = i % 4;
            int row = i / 4;

            sf::Vector2f slot_position = position + sf::Vector2f(
                border + col * (slot_size + gap),
                border + row * (slot_size + gap) + height_offset
            );

            crafting_slots[i].position = slot_position;
            crafting_slots[i].size     = {slot_size, slot_size};
            crafting_slots[i].render(window);
        }

        sf::Vector2f button_position =
        {
            position.x + size.x / 2.0f + border,
            position.y + height_offset + border
        };
        sf::Vector2f button_size =
        {
            size.x / 2.0f - border * 2.0f,
            size.x / 5.0f - border * 2.0f
        };

        crafting_button.position = button_position;
        crafting_button.size     = button_size;
        crafting_button.render(window);
    }

    renderCursor(window);

    if (hovered_slot >= 0)
    {
        renderItemInfo(window.mapPixelToCoords(sf::Mouse::getPosition(window)), slots[hovered_slot].item_stack, window);
    }
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
