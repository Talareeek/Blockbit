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
    {
        ItemID::None,
        {
            .name = "None",
            .texture = UINT32_MAX,
            .maxStackSize = 0,
            .category = ItemCategory::Misc
        }
    },

    {
        ItemID::Stone,
        {
            .name = "Stone",
            .texture = 1,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Grass,
        {
            .name = "Grass",
            .texture = 2,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Dirt,
        {
            .name = "Dirt",
            .texture = 3,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Cobblestone,
        {
            .name = "Cobblestone",
            .texture = 4,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Obsidian,
        {
            .name = "Obsidian",
            .texture = 5,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Bedrock,
        {
            .name = "Bedrock",
            .texture = 6,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Dynamite,
        {
            .name = "Dynamite",
            .texture = 12,
            .maxStackSize = 16,
            .rarity = ItemRarity::Rare,
            .category = ItemCategory::Misc,
            .onUse = [](World& world, sf::Vector2f mouse, UUID user) -> bool
            {
                Entity& player = world.getEntity(user);

                if(!player.hasComponent<TransformComponent>())
                    return false;

                sf::Vector2<double> playerPos =
                    player.getComponent<TransformComponent>().position;

                Entity explosiveEntity(generateUUID());

                explosiveEntity.addComponent(
                    TransformComponent{
                        playerPos,
                        {1.0, 1.0},
                        sf::degrees(0.0f)
                    }
                );

                explosiveEntity.addComponent(ExplosiveComponent{3.0f});

                explosiveEntity.addComponent(
                    RenderComponent{
                        12,
                        {{0, 0}, {16, 16}},
                        {1.0f, 1.0f}
                    }
                );

                explosiveEntity.addComponent(
                    PhysicsComponent{
                        (mouse - sf::Vector2f(playerPos)) * 3.0f,
                        {0.0f, 0.0f},
                        {0.0f, 0.0f},
                        1.0f,
                        true,
                        true,
                        false,
                        true
                    }
                );

                world.addEntity(std::move(explosiveEntity));

                return true;
            }
        }
    },
    {
        ItemID::Iron_Ore,
        {
            .name = "Iron Ore",
            .texture = 13,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Gold_Ore,
        {
            .name = "Gold Ore",
            .texture = 14,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Diamond_Ore,
        {
            .name = "Diamond Ore",
            .texture = 15,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Oak_Log,
        {
            .name = "Oak Log",
            .texture = 16,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Oak_Leaves,
        {
            .name = "Oak Leaves",
            .texture = 17,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Bucket,
        {
            .name = "Bucket",
            .texture = 19,
            .maxStackSize = 1,
            .rarity = ItemRarity::Rare,
            .category = ItemCategory::Misc,
            .onUse = [](World& world, sf::Vector2f mouse, UUID user) -> bool
            {
                Entity& player = world.getEntity(user);

                if(!player.hasComponent<InventoryComponent>())
                    return false;

                auto& inventory =
                    player.getComponent<InventoryComponent>().inventory;

                sf::Vector2i block_pos = {
                    static_cast<int>(std::floor(mouse.x)),
                    static_cast<int>(std::floor(mouse.y))
                };

                if(
                    world.getBlock(block_pos.x, block_pos.y).id == BlockID::Water &&
                    world.getBlock(block_pos.x, block_pos.y).metadata ==
                        static_cast<uint8_t>(WaterLevel::SOURCE)
                )
                {
                    world.setBlock(
                        block_pos.x,
                        block_pos.y,
                        {BlockID::Air, 0}
                    );

                    inventory.removeItemWithLeftover(ItemID::Bucket, 1);
                    inventory.addItemWithLeftover(ItemID::Water_Bucket, 1);
                }

                return false;
            }
        }
    },
    {
        ItemID::Water_Bucket,
        {
            .name = "Water Bucket",
            .texture = 20,
            .maxStackSize = 1,
            .rarity = ItemRarity::Rare,
            .category = ItemCategory::Misc,
            .onUse = [](World& world, sf::Vector2f mouse, UUID user)
            {
                Entity& player = world.getEntity(user);

                if(!player.hasComponent<InventoryComponent>())
                    return false;

                auto& inventory =
                    player.getComponent<InventoryComponent>().inventory;

                sf::Vector2i block_pos = {
                    static_cast<int>(std::floor(mouse.x)),
                    static_cast<int>(std::floor(mouse.y))
                };

                if(
                    world.getBlock(block_pos.x, block_pos.y).id == BlockID::Air ||
                    (
                        world.getBlock(block_pos.x, block_pos.y).id == BlockID::Water &&
                        world.getBlock(block_pos.x, block_pos.y).metadata <
                            static_cast<uint8_t>(WaterLevel::SOURCE)
                    )
                )
                {
                    world.setBlock(
                        block_pos.x,
                        block_pos.y,
                        {
                            BlockID::Water,
                            static_cast<uint8_t>(WaterLevel::SOURCE)
                        }
                    );

                    inventory.removeItemWithLeftover(ItemID::Water_Bucket, 1);
                    inventory.addItemWithLeftover(ItemID::Bucket, 1);
                }

                return false;
            }
        }
    },
    {
        ItemID::Woodcutter,
        {
            .name = "Woodcutter",
            .texture = 22,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Lighter,
        {
            .name = "Lighter",
            .texture = 24,
            .maxStackSize = 1,
            .rarity = ItemRarity::Rare,
            .category = ItemCategory::Misc,
            .onUse = [](World& world, sf::Vector2f mouse, UUID user) -> bool
            {
                sf::Vector2i position = {
                    static_cast<int>(std::floor(mouse.x)),
                    static_cast<int>(std::floor(mouse.y))
                };

                if(
                    world.getBlock(position.x, position.y).id == BlockID::Air &&
                    blockDatabase[
                        world.getBlock(position.x, position.y - 1).id
                    ].solid
                )
                {
                    world.setBlock(
                        position.x,
                        position.y,
                        {BlockID::Fire, 0}
                    );
                }

                return false;
            }
        }
    },
    {
        ItemID::Sand,
        {
            .name = "Sand",
            .texture = 26,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Coarse_Dirt,
        {
            .name = "Coarse Dirt",
            .texture = 27,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Snow,
        {
            .name = "Snow",
            .texture = 28,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },

    {
        ItemID::Wooden_Pickaxe,
        {
            .name = "Wooden Pickaxe",
            .texture = 29,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 1}
        }
    },
    {
        ItemID::Stone_Pickaxe,
        {
            .name = "Stone Pickaxe",
            .texture = 30,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 2}
        }
    },
    {
        ItemID::Gold_Pickaxe,
        {
            .name = "Gold Pickaxe",
            .texture = 31,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 3}
        }
    },
    {
        ItemID::Iron_Pickaxe,
        {
            .name = "Iron Pickaxe",
            .texture = 32,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 4}
        }
    },
    {
        ItemID::Diamond_Pickaxe,
        {
            .name = "Diamond Pickaxe",
            .texture = 33,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 5}
        }
    },

    {
        ItemID::Wooden_Axe,
        {
            .name = "Wooden Axe",
            .texture = 34,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 1}
        }
    },
    {
        ItemID::Stone_Axe,
        {
            .name = "Stone Axe",
            .texture = 35,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 2}
        }
    },
    {
        ItemID::Gold_Axe,
        {
            .name = "Gold Axe",
            .texture = 36,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 3}
        }
    },
    {
        ItemID::Iron_Axe,
        {
            .name = "Iron Axe",
            .texture = 37,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 4}
        }
    },
    {
        ItemID::Diamond_Axe,
        {
            .name = "Diamond Axe",
            .texture = 38,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 5}
        }
    },

    {
        ItemID::Wooden_Shovel,
        {
            .name = "Wooden Shovel",
            .texture = 39,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 1}
        }
    },
    {
        ItemID::Stone_Shovel,
        {
            .name = "Stone Shovel",
            .texture = 40,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 2}
        }
    },
    {
        ItemID::Gold_Shovel,
        {
            .name = "Gold Shovel",
            .texture = 41,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 3}
        }
    },
    {
        ItemID::Iron_Shovel,
        {
            .name = "Iron Shovel",
            .texture = 42,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 4}
        }
    },
    {
        ItemID::Diamond_Shovel,
        {
            .name = "Diamond Shovel",
            .texture = 43,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 5}
        }
    },
    {
        ItemID::Ruby_Ore,
        {
            .name = "Ruby Ore",
            .texture = 44,
            .maxStackSize = 64,
            .category = ItemCategory::Block,
        }
    },
    {
        ItemID::Ruby,
        {
            .name = "Ruby",
            .texture = 45,
            .maxStackSize = 64,
            .category = ItemCategory::Misc,
        }
    }
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