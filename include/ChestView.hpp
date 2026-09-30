#ifndef CHEST_VIEW_HPP
#define CHEST_VIEW_HPP

#include "View.hpp"
#include "Item.hpp"
#include "UUID.hpp"
#include "Packet.hpp"

class BaseChestView
{
public:

    ItemStack chest_stacks[36];
    ItemStack inventory_stacks[36];
};

class 

ServerChestView : public BaseChestView, public ServerView
{
private:

    UUID inventory_owner;

    std::deque<SetContentData> pending;

    sf::Vector2i chest_position;

public:

    ServerChestView(sf::Vector2i chest_position, UUID inventory_owner);

    void handleViewEvent(const ViewEventData& event_data) override;
    void update(World& world) override;
    std::deque<SetContentData> pendingContentUpdates() override;

};

#endif // CHEST_VIEW_HPP