#ifndef FURNACE_VIEW_HPP
#define FURNACE_VIEW_HPP

#include "View.hpp"
#include "Item.hpp"
#include "UUID.hpp"
#include "Slot.hpp"

#include <array>
#include <cstdint>
#include <deque>

// The furnace view presents the furnace's 3 top slots (fuel, input, output)
// stacked over the player's 36 inventory slots. Target IDs:
//   0..2    furnace fuel / input / output
//   3..38   player inventory
//   39      cursor stack
//   40      burn progress payload (two floats: remaining, total)
//   41      cook progress payload (two floats: remaining, total)

enum class FurnaceViewAction : uint8_t
{
    LeftClick  = 0,
    RightClick = 1
};

inline constexpr uint16_t FURNACE_VIEW_FUEL_SLOT     = 0;
inline constexpr uint16_t FURNACE_VIEW_INPUT_SLOT    = 1;
inline constexpr uint16_t FURNACE_VIEW_OUTPUT_SLOT   = 2;
inline constexpr uint16_t FURNACE_VIEW_PLAYER_OFFSET = 3;
inline constexpr uint16_t FURNACE_VIEW_CURSOR_SLOT   = 39;
inline constexpr uint16_t FURNACE_VIEW_BURN_PROGRESS = 40;
inline constexpr uint16_t FURNACE_VIEW_COOK_PROGRESS = 41;

class ServerFurnaceView : public ServerView
{
private:

    sf::Vector2i furnace_position;
    UUID inventory_owner;

    ItemStack cursor_stack{ItemID::None, 0};

    std::array<ItemStack, 3>  last_furnace{};
    std::array<ItemStack, 36> last_inventory{};
    ItemStack last_cursor{ItemID::None, 0};
    float last_burn_remaining = -1.0f;
    float last_burn_total     = -1.0f;
    float last_cook_remaining = -1.0f;
    float last_cook_total     = -1.0f;

    std::deque<SetContentData> pending;

    void pushSlotUpdate(uint16_t slot, const ItemStack& stack);
    void pushProgressUpdate(uint16_t slot, float remaining, float total);

    ItemStack* resolveSlotTarget(World& world, uint16_t slot);
    void handleLeftClick(World& world, uint16_t slot);
    void handleRightClick(World& world, uint16_t slot);

public:

    ServerFurnaceView(sf::Vector2i furnace_position, UUID inventory_owner);

    ViewType type() const override { return ViewType::Furnace; }

    void handleViewEvent(World& world, const ViewEventData& event_data) override;
    void update(World& world) override;
    void onOpen(World& world) override;
    void onClose(World& world) override;
    bool shouldClose(World& world) const override;

    std::deque<SetContentData> drainPendingContentUpdates() override;
};

class ClientFurnaceView : public ClientView
{
private:

    std::array<ItemStack, 3>  furnace_stacks{};
    std::array<ItemStack, 36> inventory_stacks{};
    ItemStack cursor_stack{ItemID::None, 0};

    float burn_remaining = 0.0f;
    float burn_total     = 0.0f;
    float cook_remaining = 0.0f;
    float cook_total     = 0.0f;

    std::deque<ViewEventData> pending;

    Slot slots[FURNACE_VIEW_PLAYER_OFFSET + 36];

    void pushClick(uint16_t slot, FurnaceViewAction action, const ItemStack& stack);
    void renderCursor(sf::RenderWindow& window);
    void renderProgress(sf::RenderWindow& window, float scale);

public:

    ClientFurnaceView();

    ViewType type() const override { return ViewType::Furnace; }

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void handleContentUpdate(const SetContentData& set_content_data) override;

    std::deque<ViewEventData> drainPendingEvents() override;
};

#endif // FURNACE_VIEW_HPP
