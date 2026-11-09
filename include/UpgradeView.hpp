#ifndef UPGRADE_VIEW_HPP
#define UPGRADE_VIEW_HPP

#include "View.hpp"
#include "Item.hpp"
#include "UUID.hpp"
#include "Slot.hpp"

#include <array>
#include <cstdint>
#include <deque>

// Upgrade view: three top slots (input ruby tool, ruby catalyst, output
// preview) layered over the player's 36 inventory slots. Target IDs:
//   0  input slot (Ruby_Pickaxe / Ruby_Axe / Ruby_Shovel)
//   1  ruby slot (Ruby material)
//   2  output slot (read-only preview; left-click to take)
//   3..38  player inventory
//   39     cursor stack
//
// Cost model: upgrading from level N to N+1 consumes N Rubies. One click on
// the output takes a single level-up. The output slot never holds real items;
// it is recomputed from the input + ruby slots every tick.

enum class UpgradeViewAction : uint8_t
{
    LeftClick  = 0,
    RightClick = 1
};

inline constexpr uint16_t UPGRADE_VIEW_INPUT_SLOT    = 0;
inline constexpr uint16_t UPGRADE_VIEW_RUBY_SLOT     = 1;
inline constexpr uint16_t UPGRADE_VIEW_OUTPUT_SLOT   = 2;
inline constexpr uint16_t UPGRADE_VIEW_PLAYER_OFFSET = 3;
inline constexpr uint16_t UPGRADE_VIEW_CURSOR_SLOT   = 39;

class ServerUpgradeView : public ServerView
{
private:

    sf::Vector2i table_position;
    UUID inventory_owner;

    ItemStack input_stack{ItemID::None, 0};
    ItemStack ruby_stack{ItemID::None, 0};
    ItemStack cursor_stack{ItemID::None, 0};

    ItemStack last_input{ItemID::None, 0};
    ItemStack last_ruby{ItemID::None, 0};
    ItemStack last_output{ItemID::None, 0};
    std::array<ItemStack, 36> last_inventory{};
    ItemStack last_cursor{ItemID::None, 0};

    std::deque<SetContentData> pending;

    void pushSlotUpdate(uint16_t target, const ItemStack& stack);

    ItemStack* resolveSlotTarget(World& world, uint16_t slot);
    ItemStack  computeOutputPreview() const;
    void       takeOutput(World& world);

    void handleLeftClick(World& world, uint16_t slot);
    void handleRightClick(World& world, uint16_t slot);

public:

    ServerUpgradeView(sf::Vector2i table_position, UUID inventory_owner);

    ViewType type() const override { return ViewType::Upgrade; }

    void handleViewEvent(World& world, const ViewEventData& event_data) override;
    void update(World& world) override;
    void onOpen(World& world) override;
    void onClose(World& world) override;
    bool shouldClose(World& world) const override;

    std::deque<SetContentData> drainPendingContentUpdates() override;
    void collectLiveInstanceIDs(std::unordered_set<UUID>& out) const override;
};

class ClientUpgradeView : public ClientView
{
private:

    ItemStack input_stack{ItemID::None, 0};
    ItemStack ruby_stack{ItemID::None, 0};
    ItemStack output_stack{ItemID::None, 0};
    std::array<ItemStack, 36> inventory_stacks{};
    ItemStack cursor_stack{ItemID::None, 0};

    std::deque<ViewEventData> pending;

    Slot slots[UPGRADE_VIEW_PLAYER_OFFSET + 36];

    void pushClick(uint16_t slot, UpgradeViewAction action, const ItemStack& stack);
    void renderCursor(sf::RenderWindow& window);

public:

    ClientUpgradeView();

    ViewType type() const override { return ViewType::Upgrade; }

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void handleContentUpdate(const SetContentData& set_content_data) override;

    std::deque<ViewEventData> drainPendingEvents() override;
};

#endif // UPGRADE_VIEW_HPP
