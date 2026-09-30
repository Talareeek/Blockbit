#include "../include/ChestView.hpp"

ServerChestView::ServerChestView(sf::Vector2i chest_position, UUID inventory_owner) : chest_position{chest_position}, inventory_owner{inventory_owner}
{
    for(int i = 0; i < 36; ++i)
    {
        chest_stacks[i] = ItemStack{ItemID::Bedrock, 64};
        inventory_stacks[i] = ItemStack{ItemID::Bedrock, 64};

        pending.push_back(SetContentData{static_cast<uint16_t>(i), chest_stacks[i]});
        pending.push_back(SetContentData{static_cast<uint16_t>(i + 36), inventory_stacks[i]});
    }
}

void ServerChestView::handleViewEvent(const ViewEventData& event_data)
{
    
}

void ServerChestView::update(World& world)
{
    
}
    
std::deque<SetContentData> ServerChestView::pendingContentUpdates()
{
    return pending;
}