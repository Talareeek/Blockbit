#ifndef INVENTORY_VIEW_HPP
#define INVENTORY_VIEW_HPP

#include "View.hpp"
#include "Item.hpp"
#include "UUID.hpp"
#include "Slot.hpp"
#include "Button.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

// The inventory view presents the player's own 36 slots plus a "cursor
// stack" (slot index 36) that follows the mouse, Minecraft-style. The
// payload for each SetContent / ViewEvent is the raw bytes of an ItemStack
// (same convention as ChestView).

enum class InventoryViewAction : uint8_t
{
    LeftClick  = 0,
    RightClick = 1,
    Craft      = 2
};

inline constexpr uint16_t INVENTORY_VIEW_SLOT_COUNT  = 36;
inline constexpr uint16_t INVENTORY_VIEW_CURSOR_SLOT = 36;

class ServerInventoryView : public ServerView
{
private:

    UUID owner;

    ItemStack cursor_stack{ItemID::None, 0};

    std::array<ItemStack, INVENTORY_VIEW_SLOT_COUNT> last_slots{};
    ItemStack last_cursor{ItemID::None, 0};

    std::deque<SetContentData> pending;

    void pushSlotUpdate(uint16_t target, const ItemStack& stack);

    void handleLeftClick(World& world, uint16_t slot);
    void handleRightClick(World& world, uint16_t slot);
    void handleCraft(World& world, const ItemStack& requested);

    static bool decodeStack(const std::vector<uint8_t>& payload, ItemStack& out);

public:

    explicit ServerInventoryView(UUID owner);

    ViewType type() const override { return ViewType::Inventory; }

    void handleViewEvent(World& world, const ViewEventData& event_data) override;
    void update(World& world) override;
    void onClose(World& world) override;

    std::deque<SetContentData> drainPendingContentUpdates() override;
};

class ClientInventoryView : public ClientView
{
private:

    ItemStack slot_stacks[INVENTORY_VIEW_SLOT_COUNT]{};
    ItemStack cursor_stack{ItemID::None, 0};

    Slot slots[INVENTORY_VIEW_SLOT_COUNT];

    std::vector<Slot> crafting_slots;
    ItemStack selected_recipe{ItemID::None, 0};

    Button crafting_button;
    bool craft_clicked = false;

    uint8_t page = 0;

    std::deque<ViewEventData> pending;

    void renderPagesBackground(sf::RenderWindow& window);
    void renderCursor(sf::RenderWindow& window);

    void pushClick(uint16_t slot, InventoryViewAction action, const ItemStack& stack);

public:

    ClientInventoryView();

    ViewType type() const override { return ViewType::Inventory; }

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void handleContentUpdate(const SetContentData& set_content_data) override;

    std::deque<ViewEventData> drainPendingEvents() override;
};

#endif // INVENTORY_VIEW_HPP
