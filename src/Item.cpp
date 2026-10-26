#include "../include/Item.hpp"
#include "../include/World.hpp"

#include <cctype>
#include "../include/Entity.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/Chunk.hpp"
#include "../include/FurnaceBlockEntity.hpp"

std::unordered_map<ItemID, ItemData> itemDatabase =
{
    {
        ItemID::None,
        {
            .name = "None",
            .maxStackSize = 0,
            .category = ItemCategory::Misc
        }
    },

    {
        ItemID::Stone,
        {
            .name = "Stone",
            .texture = AssetManager::GameTextureID::Stone,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Grass,
        {
            .name = "Grass",
            .texture = AssetManager::GameTextureID::Grass,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Dirt,
        {
            .name = "Dirt",
            .texture = AssetManager::GameTextureID::Dirt,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Cobblestone,
        {
            .name = "Cobblestone",
            .texture = AssetManager::GameTextureID::Cobblestone,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Obsidian,
        {
            .name = "Obsidian",
            .texture = AssetManager::GameTextureID::Obsidian,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Bedrock,
        {
            .name = "Bedrock",
            .texture = AssetManager::GameTextureID::Bedrock,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Dynamite,
        {
            .name = "Dynamite",
            .texture = AssetManager::GameTextureID::Dynamite,
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
                        AssetManager::GameTextureID::Dynamite,
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
        ItemID::StabShot,
        {
            .name = "Stab Shot",
            .texture = AssetManager::GameTextureID::Bedrock,
            .maxStackSize = 16,
            .rarity = ItemRarity::Epic,
            .category = ItemCategory::Misc,
            .onUse = [](World& world, sf::Vector2f mouse, UUID user) -> bool
            {
                (void)user;

                int column_x = static_cast<int>(std::floor(mouse.x));

                for(int y = 1; y < CHUNK_HEIGHT; y++)
                {
                    Entity explosive(generateUUID());

                    explosive.addComponent(
                        TransformComponent{
                            {static_cast<double>(column_x) + 0.5, static_cast<double>(y) + 0.5},
                            {1.0, 1.0},
                            sf::degrees(0.0f)
                        }
                    );

                    explosive.addComponent(ExplosiveComponent{3.0f, true, true, 0.05f, 0.0f});

                    explosive.addComponent(
                        RenderComponent{
                            AssetManager::GameTextureID::Dynamite,
                            {{0, 0}, {16, 16}},
                            {1.0f, 1.0f}
                        }
                    );

                    explosive.addComponent(
                        PhysicsComponent{
                            {0.0f, 0.0f},
                            {0.0f, 0.0f},
                            {0.0f, 0.0f},
                            1.0f,
                            true,
                            false,
                            true,
                            false
                        }
                    );

                    world.addEntity(std::move(explosive));
                }

                return true;
            }
        }
    },
    {
        ItemID::NukeShot,
        {
            .name = "Nuke Shot",
            .texture = AssetManager::GameTextureID::Bedrock,
            .maxStackSize = 16,
            .rarity = ItemRarity::Mythic,
            .category = ItemCategory::Misc,
            .onUse = [](World& world, sf::Vector2f mouse, UUID user) -> bool
            {
                (void)user;

                constexpr int NUKE_COUNT = 100;
                constexpr float NUKE_SPREAD_SPEED = 100.0f;
                constexpr int NUKE_HEIGHT_OFFSET = 25;

                sf::Vector2<double> spawn_pos = {
                    std::floor(mouse.x) + 0.5,
                    std::floor(mouse.y) + 0.5 + static_cast<double>(NUKE_HEIGHT_OFFSET)
                };

                for(int i = 0; i < NUKE_COUNT; i++)
                {
                    float t = (static_cast<float>(i) / static_cast<float>(NUKE_COUNT - 1)) * 2.0f - 1.0f;
                    float vx = t * NUKE_SPREAD_SPEED;

                    Entity explosive(generateUUID());

                    explosive.addComponent(
                        TransformComponent{
                            spawn_pos,
                            {1.0, 1.0},
                            sf::degrees(0.0f)
                        }
                    );

                    explosive.addComponent(ExplosiveComponent{3.0f});

                    explosive.addComponent(
                        RenderComponent{
                            AssetManager::GameTextureID::Dynamite,
                            {{0, 0}, {16, 16}},
                            {1.0f, 1.0f}
                        }
                    );

                    explosive.addComponent(
                        PhysicsComponent{
                            {vx, 0.0f},
                            {0.0f, 0.0f},
                            {0.0f, 0.0f},
                            1.0f,
                            false,
                            true,
                            false,
                            true
                        }
                    );

                    world.addEntity(std::move(explosive));
                }

                return true;
            }
        }
    },
    {
        ItemID::Coal_Ore,
        {
            .name = "Coal Ore",
            .texture = AssetManager::GameTextureID::Coal_Ore,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Iron_Ore,
        {
            .name = "Iron Ore",
            .texture = AssetManager::GameTextureID::Iron_Ore,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Gold_Ore,
        {
            .name = "Gold Ore",
            .texture = AssetManager::GameTextureID::Gold_Ore,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Diamond_Ore,
        {
            .name = "Diamond Ore",
            .texture = AssetManager::GameTextureID::Diamond_Ore,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Iron_Block,
        {
            .name = "Iron Block",
            .texture = AssetManager::GameTextureID::Iron_Block,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Gold_Block,
        {
            .name = "Gold Block",
            .texture = AssetManager::GameTextureID::Gold_Block,
            .maxStackSize = 64,
            .rarity = ItemRarity::Rare,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Ruby_Block,
        {
            .name = "Ruby Block",
            .texture = AssetManager::GameTextureID::Ruby_Block,
            .maxStackSize = 64,
            .rarity = ItemRarity::Epic,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Coal_Block,
        {
            .name = "Coal Block",
            .texture = AssetManager::GameTextureID::Coal_Block,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Coal,
        {
            .name = "Coal",
            .texture = AssetManager::GameTextureID::Coal,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Misc
        }
    },
    {
        ItemID::Raw_Iron,
        {
            .name = "Raw Iron",
            .texture = AssetManager::GameTextureID::Raw_Iron,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Misc
        }
    },
    {
        ItemID::Raw_Gold,
        {
            .name = "Raw Gold",
            .texture = AssetManager::GameTextureID::Raw_Gold,
            .maxStackSize = 64,
            .rarity = ItemRarity::Rare,
            .category = ItemCategory::Misc
        }
    },
    {
        ItemID::Diamond,
        {
            .name = "Diamond",
            .texture = AssetManager::GameTextureID::Diamond,
            .maxStackSize = 64,
            .rarity = ItemRarity::Rare,
            .category = ItemCategory::Misc
        }
    },
    {
        ItemID::Oak_Log,
        {
            .name = "Oak Log",
            .texture = AssetManager::GameTextureID::Oak_Log,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Oak_Leaves,
        {
            .name = "Oak Leaves",
            .texture = AssetManager::GameTextureID::Oak_Leaves,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Oak_Planks,
        {
            .name = "Oak Planks",
            .texture = AssetManager::GameTextureID::Oak_Planks,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Bucket,
        {
            .name = "Bucket",
            .texture = AssetManager::GameTextureID::Bucket,
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
                    WaterState::unpack(world.getBlock(block_pos.x, block_pos.y).metadata).level ==
                        WaterState::SOURCE
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
            .texture = AssetManager::GameTextureID::Water_Bucket,
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
                        WaterState::unpack(world.getBlock(block_pos.x, block_pos.y).metadata).level <
                            WaterState::SOURCE
                    )
                )
                {
                    world.setBlock(
                        block_pos.x,
                        block_pos.y,
                        {
                            BlockID::Water,
                            WaterState{WaterState::SOURCE}.pack()
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
            .texture = AssetManager::GameTextureID::Woodcutter,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Lighter,
        {
            .name = "Lighter",
            .texture = AssetManager::GameTextureID::Lighter,
            .maxStackSize = 1,
            .rarity = ItemRarity::Rare,
            .category = ItemCategory::Misc,
            .onUse = [](World& world, sf::Vector2f mouse, UUID user) -> bool
            {
                (void)user;
                sf::Vector2i position = {
                    static_cast<int>(std::floor(mouse.x)),
                    static_cast<int>(std::floor(mouse.y))
                };

                Block target = world.getBlock(position.x, position.y);

                if(target.id == BlockID::Furnace)
                {
                    auto* furnace = dynamic_cast<FurnaceBlockEntity*>(world.getBlockEntity(position.x, position.y));
                    if(!furnace) return false;
                    if(furnace->isLit()) return false;
                    if(furnace->fuel.empty() || furnace->input.empty()) return false;

                    auto fuel_it = fuelBurnTimes.find(furnace->fuel.itemID);
                    if(fuel_it == fuelBurnTimes.end()) return false;

                    auto smelt_it = smeltingRecipes.find(furnace->input.itemID);
                    if(smelt_it == smeltingRecipes.end()) return false;

                    furnace->lit = true;
                    furnace->burn_time_total     = fuel_it->second;
                    furnace->burn_time_remaining = fuel_it->second;
                    furnace->cook_time_total     = smelt_it->second.cook_time;
                    furnace->cook_time_remaining = smelt_it->second.cook_time;

                    if(--furnace->fuel.quantity == 0) furnace->fuel = {ItemID::None, 0};

                    if(!FurnaceState::unpack(target.metadata).lit)
                    {
                        world.setBlock(position.x, position.y, {BlockID::Furnace, FurnaceState{true}.pack()});
                    }

                    return false;
                }

                if(
                    target.id == BlockID::Air &&
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
            .texture = AssetManager::GameTextureID::Sand,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Coarse_Dirt,
        {
            .name = "Coarse Dirt",
            .texture = AssetManager::GameTextureID::Coarse_Dirt,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Snow,
        {
            .name = "Snow",
            .texture = AssetManager::GameTextureID::Snow,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },

    {
        ItemID::Wooden_Pickaxe,
        {
            .name = "Wooden Pickaxe",
            .texture = AssetManager::GameTextureID::Wooden_Pickaxe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 1},
            .maxDurability = 59
        }
    },
    {
        ItemID::Stone_Pickaxe,
        {
            .name = "Stone Pickaxe",
            .texture = AssetManager::GameTextureID::Stone_Pickaxe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 2},
            .maxDurability = 131
        }
    },
    {
        ItemID::Gold_Pickaxe,
        {
            .name = "Gold Pickaxe",
            .texture = AssetManager::GameTextureID::Gold_Pickaxe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 3},
            .maxDurability = 32
        }
    },
    {
        ItemID::Iron_Pickaxe,
        {
            .name = "Iron Pickaxe",
            .texture = AssetManager::GameTextureID::Iron_Pickaxe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 4},
            .maxDurability = 250
        }
    },
    {
        ItemID::Diamond_Pickaxe,
        {
            .name = "Diamond Pickaxe",
            .texture = AssetManager::GameTextureID::Diamond_Pickaxe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 5},
            .maxDurability = 1561
        }
    },
    {
        ItemID::Ruby_Pickaxe,
        {
            .name = "Ruby Pickaxe",
            .texture = AssetManager::GameTextureID::Ruby_Pickaxe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Epic,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Pickaxe, 6},
            .maxDurability = 2048
        }
    },

    {
        ItemID::Wooden_Axe,
        {
            .name = "Wooden Axe",
            .texture = AssetManager::GameTextureID::Wooden_Axe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 1},
            .maxDurability = 59
        }
    },
    {
        ItemID::Stone_Axe,
        {
            .name = "Stone Axe",
            .texture = AssetManager::GameTextureID::Stone_Axe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 2},
            .maxDurability = 131
        }
    },
    {
        ItemID::Gold_Axe,
        {
            .name = "Gold Axe",
            .texture = AssetManager::GameTextureID::Gold_Axe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 3},
            .maxDurability = 32
        }
    },
    {
        ItemID::Iron_Axe,
        {
            .name = "Iron Axe",
            .texture = AssetManager::GameTextureID::Iron_Axe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 4},
            .maxDurability = 250
        }
    },
    {
        ItemID::Diamond_Axe,
        {
            .name = "Diamond Axe",
            .texture = AssetManager::GameTextureID::Diamond_Axe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 5},
            .maxDurability = 1561
        }
    },
    {
        ItemID::Ruby_Axe,
        {
            .name = "Ruby Axe",
            .texture = AssetManager::GameTextureID::Ruby_Axe,
            .maxStackSize = 1,
            .rarity = ItemRarity::Epic,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Axe, 6},
            .maxDurability = 2048
        }
    },

    {
        ItemID::Wooden_Shovel,
        {
            .name = "Wooden Shovel",
            .texture = AssetManager::GameTextureID::Wooden_Shovel,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 1},
            .maxDurability = 59
        }
    },
    {
        ItemID::Stone_Shovel,
        {
            .name = "Stone Shovel",
            .texture = AssetManager::GameTextureID::Stone_Shovel,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 2},
            .maxDurability = 131
        }
    },
    {
        ItemID::Gold_Shovel,
        {
            .name = "Gold Shovel",
            .texture = AssetManager::GameTextureID::Gold_Shovel,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 3},
            .maxDurability = 32
        }
    },
    {
        ItemID::Iron_Shovel,
        {
            .name = "Iron Shovel",
            .texture = AssetManager::GameTextureID::Iron_Shovel,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 4},
            .maxDurability = 250
        }
    },
    {
        ItemID::Diamond_Shovel,
        {
            .name = "Diamond Shovel",
            .texture = AssetManager::GameTextureID::Diamond_Shovel,
            .maxStackSize = 1,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 5},
            .maxDurability = 1561
        }
    },
    {
        ItemID::Ruby_Shovel,
        {
            .name = "Ruby Shovel",
            .texture = AssetManager::GameTextureID::Ruby_Shovel,
            .maxStackSize = 1,
            .rarity = ItemRarity::Epic,
            .category = ItemCategory::Tool,
            .tool = ToolProperties{ToolType::Shovel, 6},
            .maxDurability = 2048
        }
    },
    {
        ItemID::Ruby_Ore,
        {
            .name = "Ruby Ore",
            .texture = AssetManager::GameTextureID::Ruby_Ore,
            .maxStackSize = 64,
            .category = ItemCategory::Block,
        }
    },
    {
        ItemID::Ruby,
        {
            .name = "Ruby",
            .texture = AssetManager::GameTextureID::Ruby,
            .maxStackSize = 64,
            .category = ItemCategory::Misc,
        }
    },
    {
        ItemID::Stick,
        {
            .name = "Stick",
            .texture = AssetManager::GameTextureID::Stick,
            .maxStackSize = 64,
            .category = ItemCategory::Misc,
        }
    },
    {
        ItemID::Chest,
        {
            .name = "Chest",
            .texture = AssetManager::GameTextureID::Chest,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Bricks,
        {
            .name = "Bricks",
            .texture = AssetManager::GameTextureID::Bricks,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Furnace,
        {
            .name = "Furnace",
            .texture = AssetManager::GameTextureID::Furnace,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
    {
        ItemID::Iron_Ingot,
        {
            .name = "Iron Ingot",
            .texture = AssetManager::GameTextureID::Iron_Ingot,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Misc
        }
    },
    {
        ItemID::Gold_Ingot,
        {
            .name = "Gold Ingot",
            .texture = AssetManager::GameTextureID::Gold_Ingot,
            .maxStackSize = 64,
            .rarity = ItemRarity::Rare,
            .category = ItemCategory::Misc
        }
    },
    {
        ItemID::Workbench,
        {
            .name = "Workbench",
            .texture = AssetManager::GameTextureID::Workbench,
            .maxStackSize = 64,
            .rarity = ItemRarity::Common,
            .category = ItemCategory::Block
        }
    },
};

bool ItemStack::empty() const
{
    return itemID == ItemID::None || quantity == 0;
}

bool canStack(const ItemStack& a, const ItemStack& b)
{
    if (a.instanceID.has_value() || b.instanceID.has_value()) return false;
    return a.itemID == b.itemID;
}

Inventory::Inventory(size_t size)
{
    slots.resize(size, {ItemID::None, 0});
}

std::optional<ItemID> findItemIdByName(const std::string& name)
{
    auto normalize = [](const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            if (c == '_') c = ' ';
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    };

    const std::string needle = normalize(name);

    for (const auto& [id, data] : itemDatabase)
    {
        if (normalize(data.name) == needle) return id;
    }
    return std::nullopt;
}

bool addItem(Inventory& inventory, ItemID itemID, uint32_t quantity)
{
    auto& data = itemDatabase[itemID];

    for(auto& slot : inventory.slots)
    {
        if(slot.itemID == itemID && !slot.instanceID.has_value() && slot.quantity < data.maxStackSize)
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
        if(slot.itemID == itemID && !slot.instanceID.has_value() && slot.quantity < data.maxStackSize)
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

ItemStack Inventory::addStackWithLeftover(ItemStack stack)
{
    if (stack.empty()) return {ItemID::None, 0};

    if (stack.instanceID.has_value())
    {
        for (auto& slot : slots)
        {
            if (slot.empty())
            {
                slot = stack;
                return {ItemID::None, 0};
            }
        }
        return stack;
    }

    ItemStack leftover = addItemWithLeftover(stack.itemID, stack.quantity);
    return leftover;
}

ItemStack Inventory::removeItemWithLeftover(ItemID itemID, uint32_t quantity)
{
    for(auto& slot : slots)
    {
        if(quantity == 0) return {ItemID::None, 0};

        if(slot.itemID == itemID && !slot.instanceID.has_value())
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

std::unordered_map<ItemStack, Recipe> recipes
{
    {{.itemID = ItemID::Oak_Planks, .quantity = 4}, {{{ItemID::Oak_Log, 1}}, false}},
    {{.itemID = ItemID::Stick, .quantity = 4}, {{{ItemID::Oak_Planks, 2}}, false}},
    {{.itemID = ItemID::Workbench, .quantity = 1}, {{{ItemID::Oak_Planks, 4}}, false}},
    {{.itemID = ItemID::Furnace,   .quantity = 1}, {{{ItemID::Cobblestone, 8}}, true}},

    {{.itemID = ItemID::Wooden_Pickaxe,  .quantity = 1}, {{{ItemID::Oak_Planks, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Stone_Pickaxe,   .quantity = 1}, {{{ItemID::Cobblestone, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Iron_Pickaxe,    .quantity = 1}, {{{ItemID::Iron_Ingot, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Gold_Pickaxe,    .quantity = 1}, {{{ItemID::Gold_Ingot, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Diamond_Pickaxe, .quantity = 1}, {{{ItemID::Diamond, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Ruby_Pickaxe,    .quantity = 1}, {{{ItemID::Ruby, 3}, {ItemID::Stick, 2}}, true}},

    {{.itemID = ItemID::Wooden_Axe,  .quantity = 1}, {{{ItemID::Oak_Planks, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Stone_Axe,   .quantity = 1}, {{{ItemID::Cobblestone, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Iron_Axe,    .quantity = 1}, {{{ItemID::Iron_Ingot, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Gold_Axe,    .quantity = 1}, {{{ItemID::Gold_Ingot, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Diamond_Axe, .quantity = 1}, {{{ItemID::Diamond, 3}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Ruby_Axe,    .quantity = 1}, {{{ItemID::Ruby, 3}, {ItemID::Stick, 2}}, true}},

    {{.itemID = ItemID::Wooden_Shovel,  .quantity = 1}, {{{ItemID::Oak_Planks, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Stone_Shovel,   .quantity = 1}, {{{ItemID::Cobblestone, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Iron_Shovel,    .quantity = 1}, {{{ItemID::Iron_Ingot, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Gold_Shovel,    .quantity = 1}, {{{ItemID::Gold_Ingot, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Diamond_Shovel, .quantity = 1}, {{{ItemID::Diamond, 1}, {ItemID::Stick, 2}}, true}},
    {{.itemID = ItemID::Ruby_Shovel,    .quantity = 1}, {{{ItemID::Ruby, 1}, {ItemID::Stick, 2}}, true}},

    {{.itemID = ItemID::Iron_Block, .quantity = 1}, {{{ItemID::Iron_Ingot, 9}}, true}},
    {{.itemID = ItemID::Gold_Block, .quantity = 1}, {{{ItemID::Gold_Ingot, 9}}, true}},
    {{.itemID = ItemID::Ruby_Block, .quantity = 1}, {{{ItemID::Ruby, 9}}, true}},
    {{.itemID = ItemID::Coal_Block, .quantity = 1}, {{{ItemID::Coal, 9}}, true}},

    {{.itemID = ItemID::Iron_Ingot, .quantity = 9}, {{{ItemID::Iron_Block, 1}}, true}},
    {{.itemID = ItemID::Gold_Ingot, .quantity = 9}, {{{ItemID::Gold_Block, 1}}, true}},
    {{.itemID = ItemID::Ruby,       .quantity = 9}, {{{ItemID::Ruby_Block, 1}}, true}},
    {{.itemID = ItemID::Coal,       .quantity = 9}, {{{ItemID::Coal_Block, 1}}, true}},
};

std::unordered_map<ItemID, SmeltRecipe> smeltingRecipes
{
    {ItemID::Cobblestone, {ItemID::Stone,      10.0f}},
    {ItemID::Raw_Iron,    {ItemID::Iron_Ingot, 10.0f}},
    {ItemID::Raw_Gold,    {ItemID::Gold_Ingot, 10.0f}},
};

std::unordered_map<ItemID, float> fuelBurnTimes
{
    {ItemID::Coal,       80.0f},
    {ItemID::Oak_Log,    15.0f},
    {ItemID::Oak_Planks, 15.0f},
    {ItemID::Oak_Leaves,  1.0f},
    {ItemID::Woodcutter, 15.0f},
    {ItemID::Stick,       5.0f},
};
