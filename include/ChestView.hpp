#ifndef CHEST_VIEW_HPP
#define CHEST_VIEW_HPP

#include "View.hpp"
#include "Item.hpp"
#include "UUID.hpp"
#include "Slot.hpp"

#include <array>
#include <cstdint>
#include <deque>

// A chest view presents two grids on top of each other: the chest's own 36
// stacks (targets 0..35) and the player's inventory (targets 36..71). Target
// 72 is the cursor stack that follows the mouse, Minecraft-style. The
// payload for every SetContent / ViewEvent is the raw bytes of an ItemStack.

enum class ChestViewAction : uint8_t
{
    LeftClick  = 0,
    RightClick = 1
};

inline constexpr uint16_t CHEST_VIEW_CHEST_OFFSET  = 0;
inline constexpr uint16_t CHEST_VIEW_PLAYER_OFFSET = 36;
inline constexpr uint16_t CHEST_VIEW_CURSOR_SLOT   = 72;
inline constexpr uint16_t CHEST_VIEW_SLOT_COUNT    = 73;

class BaseChestView
{
public:

    std::array<ItemStack, 36> chest_stacks{};
    std::array<ItemStack, 36> inventory_stacks{};
};

class ServerChestView : public BaseChestView, public ServerView
{
private:

    sf::Vector2i chest_position;
    UUID inventory_owner;

    ItemStack cursor_stack{ItemID::None, 0};

    std::array<ItemStack, 36> last_chest{};
    std::array<ItemStack, 36> last_inventory{};
    ItemStack last_cursor{ItemID::None, 0};

    std::deque<SetContentData> pending;

    void pushSlotUpdate(uint16_t slot, const ItemStack& stack);

    void handleLeftClick(World& world, uint16_t slot);
    void handleRightClick(World& world, uint16_t slot);

public:

    ServerChestView(sf::Vector2i chest_position, UUID inventory_owner);

    ViewType type() const override { return ViewType::Chest; }

    void handleViewEvent(World& world, const ViewEventData& event_data) override;
    void update(World& world) override;
    void onOpen(World& world) override;
    void onClose(World& world) override;
    bool shouldClose(World& world) const override;

    std::deque<SetContentData> drainPendingContentUpdates() override;
};

class ClientChestView : public BaseChestView, public ClientView
{
private:

    ItemStack cursor_stack{ItemID::None, 0};

    std::deque<ViewEventData> pending;

    Slot slots[CHEST_VIEW_CHEST_OFFSET + 36 + 36];

    void pushClick(uint16_t slot, ChestViewAction action, const ItemStack& stack);
    void renderCursor(sf::RenderWindow& window);

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
