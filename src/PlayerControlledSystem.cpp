#include "../include/PlayerControlledSystem.hpp"
#include "../include/GameCommon.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/ChestView.hpp"
#include "../include/CraftingView.hpp"
#include "../include/FurnaceView.hpp"
#include "../include/Prefab.hpp"
#include "../include/ItemUtils.hpp"

void PlayerControlledSystem(World& world, float dt)
{
    static std::random_device rd;
    static std::mt19937 rng(rd());

    for(auto& [id, entity] : world.getEntities())
    {
        if(!entity.hasComponent<PlayerControlledComponent>() || !entity.hasComponent<TransformComponent>() || !entity.hasComponent<InventoryComponent>()) continue;


        auto& player = entity.getComponent<PlayerControlledComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& inventory = entity.getComponent<InventoryComponent>();

        if (entity.hasComponent<AnimationComponent>() && entity.hasComponent<PhysicsComponent>())
        {
            auto& animation = entity.getComponent<AnimationComponent>();
            const auto& physics = entity.getComponent<PhysicsComponent>();

            AnimationState desired = std::abs(physics.velocity.x) > 0.5f ? AnimationState::Walking : AnimationState::Idle;

            if (animation.currentState != desired)
            {
                animation.currentState = desired;
                animation.currentFrame = 0;
                animation.timer = 0.0f;
                animation.dirty = true;
            }
        }


        auto block_from_position = [](sf::Vector2<double> position) -> sf::Vector2i
        {
            return {static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y))};
        };

        if(player.mid_attack)
        {
            if(player.mining_block == block_from_position(player.cursor_position))
            {
                player.mining_time += dt;
            }
            else
            {
                player.mining_block = block_from_position(player.cursor_position);
                player.mining_time = 0.0f;
            }

            player.dirty = true;

            sf::Vector2i block_position = player.mining_block;

            Block block = world.getBlock(block_position.x, block_position.y);

            ItemStack item = inventory.inventory.slots[inventory.selectedSlot];

            if(blockDatabase[block.id].mining.has_value())
            {
                auto& block_data = blockDatabase[block.id];
                auto& item_data  = itemDatabase[item.itemID];

                bool correct_type = item_data.tool.has_value() && item_data.tool->tool_type == block_data.mining->desired_tool.tool_type;
                int tool_level  = correct_type ? item_data.tool->level : 0;
                bool proper_tool = correct_type && item_data.tool->level >= block_data.mining->desired_tool.level;

                float speed = level_based_speed[tool_level];
                // Using the wrong tool type adds a time penalty on top of the hand-speed baseline,
                // so a correct-but-low-level tool is always better than a wrong tool.
                float wrong_tool_penalty = correct_type ? 1.0f : 1.5f;
                float block_mining_time = (block_data.hardness < 0.0f) ? std::numeric_limits<float>::infinity() : (block_data.hardness * wrong_tool_penalty) / speed;

                if(player.mining_time >= block_mining_time && world.getBlock(block_position.x, block_position.y).id != BlockID::Air && blockDatabase[world.getBlock(block_position.x, block_position.y).id].breakable && isBlockInRange(transform, block_position, 4.0f))
                {
                    player.dirty = true;

                    world.setBlock(block_position.x, block_position.y, {BlockID::Air, 0});

                    if(block_data.drop.has_value() && (proper_tool || block_data.drop->always_drop))
                    {
                        std::uniform_int_distribution drop_range(block_data.drop->min_amount, block_data.drop->max_amount);
                        ItemStack drop = {block_data.drop->drop, drop_range(rng)};
                        spawnDroppedItem(world, {block_position.x + 0.25, block_position.y + 0.25}, drop, randomDropSpread(60.0f));
                    }

                    if(item_data.tool.has_value() && item_data.maxDurability.has_value())
                    {
                        damageItem(inventory.inventory.slots[inventory.selectedSlot], 1);
                        inventory.dirty = true;
                    }

                    player.mining_time = 0.0f;
                }
            }
        }

        player.since_last_use += dt;
        player.dirty = true;

        if(player.mid_usage && player.since_last_use >= 0.2f)
        {
            sf::Vector2i block_position = block_from_position(player.cursor_position);

            if(world.getBlock(block_position.x, block_position.y).id == BlockID::Chest && isBlockInRange(transform, block_position, 4.0f))
            {
                player.pending_view_opens.push_back(std::make_unique<ServerChestView>(block_position, id));
                player.mid_usage = false;
                player.since_last_use = 0.0f;
                player.dirty = true;
                continue;
            }

            if(world.getBlock(block_position.x, block_position.y).id == BlockID::Workbench && isBlockInRange(transform, block_position, 4.0f))
            {
                player.pending_view_opens.push_back(std::make_unique<ServerCraftingView>(block_position, id));
                player.mid_usage = false;
                player.since_last_use = 0.0f;
                player.dirty = true;
                continue;
            }

            if(world.getBlock(block_position.x, block_position.y).id == BlockID::Furnace && isBlockInRange(transform, block_position, 4.0f))
            {
                auto& selected = inventory.inventory.slots[inventory.selectedSlot];

                // Clicking a furnace with a Lighter in hand should still light
                // it, not pop open the UI. Fall through to the item-use path.
                if(!(!selected.empty() && selected.itemID == ItemID::Lighter))
                {
                    player.pending_view_opens.push_back(std::make_unique<ServerFurnaceView>(block_position, id));
                    player.mid_usage = false;
                    player.since_last_use = 0.0f;
                    player.dirty = true;
                    continue;
                }
            }

            auto& selected = inventory.inventory.slots[inventory.selectedSlot];

            if(selected.empty()) break;

            if((world.getBlock(block_position.x, block_position.y).id == BlockID::Air || world.getBlock(block_position.x, block_position.y).id == BlockID::Water) && isBlockInRange(transform, block_position, 4.0f) && itemDatabase[selected.itemID].category == ItemCategory::Block)
            {
                bool collides = false;


                sf::Vector2<double> position = {static_cast<double>(block_position.x), static_cast<double>(block_position.y)};
                sf::Vector2<double> size = {1.0f, 1.0f};

                for(auto& [uuid, entity] : world.getEntities())
                {
                    if(!entity.hasComponent<TransformComponent>()) continue;

                    auto transform = entity.getComponent<TransformComponent>();


                    if(transform.intersects(position, size))
                    {
                        collides = true;
                        break;
                    }
                }

                if(!collides)
                {
                    selected.quantity--;
                    inventory.dirty = true;

                    world.setBlock(block_position.x, block_position.y, {itemToBlock(selected.itemID), 0});
                }
            }
            else if(itemDatabase[selected.itemID].category != ItemCategory::Block)
            {
                if(itemDatabase[selected.itemID].onUse(world, {static_cast<float>(player.cursor_position.x), static_cast<float>(player.cursor_position.y)}, id))
                {
                    selected.quantity--;
                    inventory.dirty = true;
                }
            }

            player.since_last_use = 0.0f;
        }

        if(player.drops)
        {
            player.drops = false;
            player.dirty = true;

            auto& stack = inventory.inventory.slots[inventory.selectedSlot];

            if(stack.empty()) break;

            inventory.dirty = true;

            ItemStack to_drop;
            if(player.drops_full_stack || stack.isUnique())
            {
                to_drop = stack;
                stack = {ItemID::None, 0};
            }
            else
            {
                to_drop = {stack.itemID, 1};
                if(--stack.quantity == 0)
                {
                    stack.itemID = ItemID::None;
                }
            }

            sf::Vector2<double> drop_position = transform.position + sf::Vector2<double>(0.0, 1.5);
            sf::Vector2f throw_force = sf::Vector2f(player.cursor_position - transform.position) * 60.0f;

            spawnDroppedItem(world, drop_position, to_drop, throw_force);
        }

        if(player.change_slot.has_value())
        {
            auto& data = player.change_slot.value();

            if(data.scroll)
            {
                inventory.selectedSlot += (data.value + 9);
            }
            else
            {
                inventory.selectedSlot = data.value;
            }

            inventory.selectedSlot %= 9;
            inventory.dirty = true;

            player.change_slot = std::nullopt;
            player.dirty = true;
        }
    }
}