#ifndef CHEST_VIEW_HPP
#define CHEST_VIEW_HPP

#include "View.hpp"
#include "Item.hpp"
#include "UUID.hpp"
#include "Slot.hpp"

// A chest view presents two 36-slot grids on top of each other: the chest's
// own stacks (targets 0..35) and the player's inventory (targets 36..71).
// Each SetContent/ViewEvent payload for this view type is simply the raw
// bytes of an ItemStack. The sender of a ViewEvent is the slot index that
// was clicked and the action is one of ChestViewAction below.

enum class ChestViewAction : uint8_t
{
    LeftClick  = 0,
    RightClick = 1
};

inline constexpr uint16_t CHEST_VIEW_SLOT_COUNT    = 72;
inline constexpr uint16_t CHEST_VIEW_CHEST_OFFSET  = 0;
inline constexpr uint16_t CHEST_VIEW_PLAYER_OFFSET = 36;

class BaseChestView
{
public:

    ItemStack chest_stacks[36]     = {};
    ItemStack inventory_stacks[36] = {};
};

class ServerChestView : public BaseChestView, public ServerView
{
private:

    UUID inventory_owner;

    std::deque<SetContentData> pending;

    sf::Vector2i chest_position;

    void pushSlotUpdate(uint16_t slot, const ItemStack& stack);

public:

    ServerChestView(sf::Vector2i chest_position, UUID inventory_owner);

    ViewType type() const override { return ViewType::Chest; }

    void handleViewEvent(World& world, const ViewEventData& event_data) override;
    void update(World& world) override;
    void onClose(World& world) override;

    std::deque<SetContentData> drainPendingContentUpdates() override;
};

class ClientChestView : public BaseChestView, public ClientView
{
private:

    std::deque<ViewEventData> pending;

    Slot slots[CHEST_VIEW_SLOT_COUNT];

public:

    ClientChestView();

    ViewType type() const override { return ViewType::Chest; }

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void handleContentUpdate(const SetContentData& set_content_data) override;

    std::deque<ViewEventData> drainPendingEvents() override;
};

#endif // CHEST_VIEW_HPP
