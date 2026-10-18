#include "../include/ChestView.hpp"

#include <cstring>

namespace
{
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
}

// ---------- ServerChestView ----------

ServerChestView::ServerChestView(sf::Vector2i chest_position, UUID inventory_owner) : chest_position{chest_position}, inventory_owner{inventory_owner}
{
    for(uint16_t i = 0; i < 36; ++i)
    {
        chest_stacks[i]     = ItemStack{ItemID::None, 0};
        inventory_stacks[i] = ItemStack{ItemID::None, 0};

        pushSlotUpdate(CHEST_VIEW_CHEST_OFFSET  + i, chest_stacks[i]);
        pushSlotUpdate(CHEST_VIEW_PLAYER_OFFSET + i, inventory_stacks[i]);
    }
}

void ServerChestView::pushSlotUpdate(uint16_t slot, const ItemStack& stack)
{
    pending.push_back(SetContentData{slot, encodeStack(stack)});
}

void ServerChestView::handleViewEvent(World& world, const ViewEventData& event_data)
{
    // No gameplay logic yet. The view simply echoes the click back as a
    // slot refresh so the end-to-end plumbing can be exercised.
    if (event_data.sender >= CHEST_VIEW_SLOT_COUNT) return;

    if (event_data.sender < CHEST_VIEW_PLAYER_OFFSET)
    {
        pushSlotUpdate(event_data.sender, chest_stacks[event_data.sender]);
    }
    else
    {
        uint16_t index = event_data.sender - CHEST_VIEW_PLAYER_OFFSET;
        pushSlotUpdate(event_data.sender, inventory_stacks[index]);
    }
}

void ServerChestView::update(World& world)
{
}

void ServerChestView::onClose(World& world)
{
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
    for (uint16_t i = 0; i < CHEST_VIEW_SLOT_COUNT; ++i)
    {
        slots[i].show_item_info = true;
    }
}

void ClientChestView::handleEvent(const sf::Event& event)
{
    for (uint16_t i = 0; i < CHEST_VIEW_SLOT_COUNT; ++i)
    {
        bool was_clicked = slots[i].clicked;
        slots[i].handleEvent(event);

        if (!was_clicked && slots[i].clicked)
        {
            slots[i].clicked = false;

            const ItemStack& stack = (i < CHEST_VIEW_PLAYER_OFFSET) ? chest_stacks[i] : inventory_stacks[i - CHEST_VIEW_PLAYER_OFFSET];

            std::vector<uint8_t> payload(sizeof(ItemStack));
            std::memcpy(payload.data(), &stack, sizeof(ItemStack));

            pending.push_back(ViewEventData{i, static_cast<uint8_t>(ChestViewAction::LeftClick), std::move(payload)});
        }
    }
}

void ClientChestView::update(float dt)
{
    for (uint16_t i = 0; i < CHEST_VIEW_PLAYER_OFFSET; ++i)      slots[i].setItemStack(chest_stacks[i]);
    for (uint16_t i = 0; i < 36; ++i)                            slots[CHEST_VIEW_PLAYER_OFFSET + i].setItemStack(inventory_stacks[i]);
}

void ClientChestView::render(sf::RenderWindow& window)
{
    sf::Vector2u window_size = window.getSize();

    size     = {window_size.x * 0.5f, window_size.y * 0.75f};
    position = {(window_size.x - size.x) * 0.5f, (window_size.y - size.y) * 0.5f};

    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setFillColor(sf::Color(0, 0, 0, 192));
    background.setOutlineColor(sf::Color::White);
    background.setOutlineThickness(2.0f);
    window.draw(background);

    const float border = 0.03f * size.x;
    const float gap    = 0.01f * size.x;
    const float slot_size = (size.x - border * 2.0f - gap * 8.0f) / 9.0f;

    auto layoutGrid = [&](uint16_t offset, float grid_top)
    {
        for (uint16_t i = 0; i < 36; ++i)
        {
            int col = i % 9;
            int row = i / 9;

            sf::Vector2f slot_position =
            {
                position.x + border + col * (slot_size + gap),
                grid_top         + row * (slot_size + gap)
            };

            slots[offset + i].position = slot_position;
            slots[offset + i].size     = {slot_size, slot_size};
            slots[offset + i].render(window);
        }
    };

    float chest_top  = position.y + border;
    float player_top = chest_top + 4.0f * (slot_size + gap) + border * 2.0f;

    layoutGrid(CHEST_VIEW_CHEST_OFFSET,  chest_top);
    layoutGrid(CHEST_VIEW_PLAYER_OFFSET, player_top);
}

void ClientChestView::handleContentUpdate(const SetContentData& set_content_data)
{
    if (set_content_data.target >= CHEST_VIEW_SLOT_COUNT) return;

    ItemStack stack{ItemID::None, 0};
    if (!decodeStack(set_content_data.payload, stack)) return;

    if (set_content_data.target < CHEST_VIEW_PLAYER_OFFSET)
    {
        chest_stacks[set_content_data.target] = stack;
    }
    else
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
