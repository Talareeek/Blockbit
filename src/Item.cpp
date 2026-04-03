#include "../include/Item.hpp"
#include "../include/World.hpp"
#include "../include/Entity.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/TransformComponent.hpp"

std::unordered_map<ItemID, ItemData> itemDatabase =
{
    {ItemID::None, {"None", UINT32_MAX, 0, ItemCategory::Misc}},
    {ItemID::Stone, {"Stone", 1, 64, ItemCategory::Block}},
    {ItemID::Grass, {"Grass", 2, 64, ItemCategory::Block}},
    {ItemID::Dirt, {"Dirt", 3, 64, ItemCategory::Block}},
    {ItemID::Cobblestone, {"Cobblestone", 4, 64, ItemCategory::Block}},
    {ItemID::Obsidian, {"Obsidian", 5, 64, ItemCategory::Block}},
    {ItemID::Bedrock, {"Bedrock", 6, 64, ItemCategory::Block}},
    {ItemID::Dynamite, {"Dynamite", 12, 16, ItemCategory::Misc, [](World& world, sf::Vector2f mouse, uint32_t user)
        {
            auto entityWithID = [&world](uint32_t id) -> Entity&
            {
                for(auto& entity : world.getEntities())
                {
                    if(entity.getID() == id)
                        return entity;
                }
                throw std::runtime_error("Entity with ID " + std::to_string(id) + " does not exist(entityWithID(int, World&))");
            };

            Entity& player = entityWithID(user);

            if(!player.hasComponent<TransformComponent>()) return;

            world.getEntities().push_back(Entity(world.getPossibleID()));
            auto& explosiveEntity = world.getEntities().back();
            explosiveEntity.addComponent(TransformComponent{player.getComponent<TransformComponent>().position, {1.0f, 1.0f}, sf::degrees(0.0f)});
            explosiveEntity.addComponent(ExplosiveComponent{3.0f});
            explosiveEntity.addComponent(RenderComponent{12, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});
            explosiveEntity.addComponent(PhysicsComponent{mouse - player.getComponent<TransformComponent>().position, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
            
        }}},
    {ItemID::Iron_Ore, {"Iron Ore", 13, 64, ItemCategory::Block}},
    {ItemID::Gold_Ore, {"Gold Ore", 14, 64, ItemCategory::Block}},
    {ItemID::Diamond_Ore, {"Diamond Ore", 15, 64, ItemCategory::Block}},
    {ItemID::Oak_Log, {"Oak Log", 16, 64, ItemCategory::Block}},
    {ItemID::Oak_Leaves, {"Oak Leaves", 17, 64, ItemCategory::Block}}
};

bool ItemStack::empty() const
{
    return itemID == ItemID::None || quantity == 0;
}

Inventory::Inventory(size_t size)
{
    slots.resize(size, {ItemID::None, 0});
}

bool addItem(Inventory& inventory, ItemID itemID, uint32_t quantity)
{
    auto& data = itemDatabase[itemID];

    for(auto& slot : inventory.slots)
    {
        if(slot.itemID == itemID && slot.quantity < data.maxStackSize)
        {
            uint32_t space = data.maxStackSize - slot.quantity;
            uint32_t add = std::min(space, quantity);

            slot.quantity += add;
            quantity -= add;

            if(quantity == 0)
                return true;
        }
    }

    for(auto& slot : inventory.slots)
    {
        if(slot.empty())
        {
            slot.itemID = itemID;
            slot.quantity = std::min(quantity, data.maxStackSize);

            quantity -= slot.quantity;

            if(quantity == 0)
                return true;
        }
    }

    return false;
}


ItemStack Inventory::addItemWithLeftover(ItemID itemID, uint32_t quantity)
{
    auto& data = itemDatabase[itemID];

    for(auto& slot : slots)
    {
        if(slot.itemID == itemID && slot.quantity < data.maxStackSize)
        {
            uint32_t space = data.maxStackSize - slot.quantity;
            uint32_t add = std::min(space, quantity);

            slot.quantity += add;
            quantity -= add;

            if(quantity == 0)
                return {ItemID::None, 0};
        }
    }

    for(auto& slot : slots)
    {
        if(slot.empty())
        {
            slot.itemID = itemID;
            slot.quantity = std::min(quantity, data.maxStackSize);

            quantity -= slot.quantity;

            if(quantity == 0)
                return {ItemID::None, 0};
        }
    }

    return {itemID, quantity};
}