#ifndef CRAFTING_VIEW_HPP
#define CRAFTING_VIEW_HPP

#include "View.hpp"
#include "Item.hpp"
#include "UUID.hpp"
#include "Slot.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

// Crafting view: chest-style layout (full-UI panel on top of the player's 36
// inventory slots) that drives the same recipes map as the inventory
// crafting page. Targets:
//   0..35   player inventory
//   36      cursor stack

enum class CraftingViewAction : uint8_t
{
    LeftClick  = 0,
    RightClick = 1,
    Craft      = 2,
    CraftAll   = 3
};

inline constexpr uint16_t CRAFTING_VIEW_PLAYER_OFFSET = 0;
inline constexpr uint16_t CRAFTING_VIEW_CURSOR_SLOT   = 36;

class ServerCraftingView : public ServerView
{
private:

    sf::Vector2i crafting_position;
    UUID inventory_owner;

    ItemStack cursor_stack{ItemID::None, 0};

    std::array<ItemStack, 36> last_inventory{};
    ItemStack last_cursor{ItemID::None, 0};

    std::deque<SetContentData> pending;

    void pushSlotUpdate(uint16_t target, const ItemStack& stack);

    void handleLeftClick(World& world, uint16_t slot);
    void handleRightClick(World& world, uint16_t slot);
    void handleCraft(World& world, const ItemStack& requested);
    void handleCraftAll(World& world, const ItemStack& requested);

    static bool decodeStack(const std::vector<uint8_t>& payload, ItemStack& out);

public:

    ServerCraftingView(sf::Vector2i crafting_position, UUID inventory_owner);

    ViewType type() const override { return ViewType::Crafting; }

    void handleViewEvent(World& world, const ViewEventData& event_data) override;
    void update(World& world) override;
    void onOpen(World& world) override;
    void onClose(World& world) override;
    bool shouldClose(World& world) const override;

    std::deque<SetContentData> drainPendingContentUpdates() override;
};

class ClientCraftingView : public ClientView
{
private:

    std::array<ItemStack, 36> inventory_stacks{};
    ItemStack cursor_stack{ItemID::None, 0};

    Slot slots[36];

    std::vector<Slot> crafting_slots;
    ItemStack selected_recipe{ItemID::None, 0};

    sf::FloatRect craft_button_bounds{};
    sf::FloatRect craft_all_button_bounds{};

    bool craft_hovered     = false;
    bool craft_all_hovered = false;
    bool craft_pressed     = false;
    bool craft_all_pressed = false;

    std::deque<ViewEventData> pending;

    void renderCursor(sf::RenderWindow& window);
    void renderIngredientList(sf::RenderWindow& window, float scale);
    void renderCraftButtonOverlays(sf::RenderWindow& window);

    void pushClick(uint16_t slot, CraftingViewAction action, const ItemStack& stack);
    void sendCraftRequest(CraftingViewAction action);

public:

    ClientCraftingView();

    ViewType type() const override { return ViewType::Crafting; }

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void handleContentUpdate(const SetContentData& set_content_data) override;

    std::deque<ViewEventData> drainPendingEvents() override;
};

#endif // CRAFTING_VIEW_HPP
