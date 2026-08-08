#include "../include/Item.hpp"
#include "../include/World.hpp"
#include "../include/Entity.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/PhysicsComponent.hpp"

std::unordered_map<ItemID, ItemData> itemDatabase =
{
    {ItemID::None, {"None", UINT32_MAX, 0, ItemRarity::Common, ItemCategory::Misc, false}},
    {ItemID::Stone, {"Stone", 1, 64, ItemRarity::Common, ItemCategory::Block, true}},
    {ItemID::Grass, {"Grass", 2, 64, ItemRarity::Common, ItemCategory::Block, false}},
    {ItemID::Dirt, {"Dirt", 3, 64, ItemRarity::Common, ItemCategory::Block, false}},
    {ItemID::Cobblestone, {"Cobblestone", 4, 64, ItemRarity::Common, ItemCategory::Block, true}},
    {ItemID::Obsidian, {"Obsidian", 5, 64, ItemRarity::Common, ItemCategory::Block, false}},
    {ItemID::Bedrock, {"Bedrock", 6, 64, ItemRarity::Common, ItemCategory::Block, false}},
    {ItemID::Dynamite, {"Dynamite", 12, 16, ItemRarity::Rare, ItemCategory::Misc, false, [](World& world, sf::Vector2f mouse, UUID user) -> bool
        {
            Entity& player = world.getEntity(user);

            if(!player.hasComponent<TransformComponent>()) return false;

            sf::Vector2<double> playerPos = player.getComponent<TransformComponent>().position;

            Entity explosiveEntity(generateUUID());
            explosiveEntity.addComponent(TransformComponent{playerPos, {1.0, 1.0}, sf::degrees(0.0f)});
            explosiveEntity.addComponent(ExplosiveComponent{3.0f});
            explosiveEntity.addComponent(RenderComponent{12, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});
            explosiveEntity.addComponent(PhysicsComponent{(mouse - sf::Vector2f(playerPos)) * 3.0f, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});

            world.addEntity(std::move(explosiveEntity));

            return true;
            
        }}},
    {ItemID::Iron_Ore, {"Iron Ore", 13, 64, ItemRarity::Common, ItemCategory::Block, false}},
    {ItemID::Gold_Ore, {"Gold Ore", 14, 64, ItemRarity::Common, ItemCategory::Block, false}},
    {ItemID::Diamond_Ore, {"Diamond Ore", 15, 64, ItemRarity::Common, ItemCategory::Block, false}},
    {ItemID::Oak_Log, {"Oak Log", 16, 64, ItemRarity::Common, ItemCategory::Block, true}},
    {ItemID::Oak_Leaves, {"Oak Leaves", 17, 64, ItemRarity::Common, ItemCategory::Block, true}},
    {ItemID::Bucket, {"Bucket", 19, 1, ItemRarity::Rare, ItemCategory::Misc, true, [](World& world, sf::Vector2f mouse, UUID user) -> bool
        {
            Entity& player = world.getEntity(user);

            if(!player.hasComponent<InventoryComponent>()) return false;

            auto& inventory = player.getComponent<InventoryComponent>().inventory;

            sf::Vector2i block_pos = {static_cast<int>(std::floor(mouse.x)), static_cast<int>(std::floor(mouse.y))};

            if(world.getBlock(block_pos.x, block_pos.y).id == BlockID::Water && world.getBlock(block_pos.x, block_pos.y).metadata == static_cast<uint8_t>(WaterLevel::SOURCE))
            {
                world.setBlock(block_pos.x, block_pos.y, {BlockID::Air, 0});

                inventory.removeItemWithLeftover(ItemID::Bucket, 1);

                inventory.addItemWithLeftover(ItemID::Water_Bucket, 1);
            }

            return false;
        }}},
    {ItemID::Water_Bucket, {"Water Bucket", 20, 1, ItemRarity::Rare, ItemCategory::Misc, false, [](World& world, sf::Vector2f mouse, UUID user)
        {
            Entity& player = world.getEntity(user);

            if(!player.hasComponent<InventoryComponent>()) return false;

            auto& inventory = player.getComponent<InventoryComponent>().inventory;

            sf::Vector2i block_pos = {static_cast<int>(std::floor(mouse.x)), static_cast<int>(std::floor(mouse.y))};

            if(world.getBlock(block_pos.x, block_pos.y).id == BlockID::Air || (world.getBlock(block_pos.x, block_pos.y).id == BlockID::Water && world.getBlock(block_pos.x, block_pos.y).metadata < static_cast<uint8_t>(WaterLevel::SOURCE)))
            {
                world.setBlock(block_pos.x, block_pos.y, {BlockID::Water, static_cast<uint8_t>(WaterLevel::SOURCE)});

                inventory.removeItemWithLeftover(ItemID::Water_Bucket, 1);

                inventory.addItemWithLeftover(ItemID::Bucket, 1);
            }

            return false;
        }}},
    {ItemID::Woodcutter, {"Woodcutter", 22, 64, ItemRarity::Common, ItemCategory::Block}},
    {ItemID::Lighter, {"Lighter", 24, 1, ItemRarity::Rare, ItemCategory::Misc, false, [](World& world, sf::Vector2f mouse, UUID user) -> bool
        {
            sf::Vector2i position = {static_cast<int>(std::floor(mouse.x)), static_cast<int>(std::floor(mouse.y))};

            if(world.getBlock(position.x, position.y).id == BlockID::Air && blockDatabase[world.getBlock(position.x, position.y - 1).id].solid)
            {
                world.setBlock(position.x, position.y, {BlockID::Fire, 0});
            }

            return false;
        }
    }},
    {ItemID::Sand, {"Sand", 26, 64, ItemRarity::Common, ItemCategory::Block, true}},
    {ItemID::Coarse_Dirt, {"Coarse Dirt", 27, 64, ItemRarity::Common, ItemCategory::Block, true}},
    {ItemID::Snow, {"Snow", 28, 64, ItemRarity::Common, ItemCategory::Block, true}},

    {ItemID::Wooden_Pickaxe, {"Wooden Pickaxe", 29, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Stone_Pickaxe, {"Stone Pickaxe", 30, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Gold_Pickaxe, {"Gold Pickaxe", 31, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Iron_Pickaxe, {"Iron Pickaxe", 32, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Diamond_Pickaxe, {"Diamond Pickaxe", 33, 1, ItemRarity::Common, ItemCategory::Tool, false}},

    {ItemID::Wooden_Axe, {"Wooden Axe", 34, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Stone_Axe, {"Stone Axe", 35, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Gold_Axe, {"Gold Axe", 36, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Iron_Axe, {"Iron Axe", 37, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Diamond_Axe, {"Diamond Axe", 38, 1, ItemRarity::Common, ItemCategory::Tool, false}},

    {ItemID::Wooden_Shovel, {"Wooden Shovel", 39, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Stone_Shovel, {"Stone Shovel", 40, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Gold_Shovel, {"Gold Shovel", 41, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Iron_Shovel, {"Iron Shovel", 42, 1, ItemRarity::Common, ItemCategory::Tool, false}},
    {ItemID::Diamond_Shovel, {"Diamond Shovel", 43, 1, ItemRarity::Common, ItemCategory::Tool, false}},
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

ItemStack Inventory::removeItemWithLeftover(ItemID itemID, uint32_t quantity)
{
    for(auto& slot : slots)
    {
        if(quantity == 0) return {ItemID::None, 0};

        if(slot.itemID == itemID)
        {
            uint32_t remove = std::min(slot.quantity, quantity);

            slot.quantity -= remove;
            quantity -= remove;

            if(slot.quantity == 0) slot.itemID = ItemID::None;
        }
    }

    if(quantity == 0) return {ItemID::None, 0};

    return {itemID, quantity};
}