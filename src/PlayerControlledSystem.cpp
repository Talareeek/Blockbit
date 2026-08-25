#include "../include/PlayerControlledSystem.hpp"
#include "../include/GameCommon.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/InventoryComponent.hpp"

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

            sf::Vector2i block_position = player.mining_block;

            Block block = world.getBlock(block_position.x, block_position.y);

            ItemStack item = inventory.inventory.slots[inventory.selectedSlot];

            if(blockDatabase[block.id].mining.has_value())
            {
                auto& block_data = blockDatabase[block.id];
                auto& item_data  = itemDatabase[item.itemID];

                bool correct_type = item_data.tool.has_value() && item_data.tool->tool_type == block_data.mining->desired_tool.tool_type;
                int tool_level  = correct_type ? item_data.tool->level : 0;

                float speed = level_based_speed[tool_level];
                float block_mining_time = (block_data.hardness < 0.0f) ? std::numeric_limits<float>::infinity() : block_data.hardness / speed;

                bool will_drop = (correct_type && item_data.tool->level >= block_data.mining->desired_tool.level) || (block_data.drop.has_value() && block_data.drop->always_drop);

                if(player.mining_time >= block_mining_time && world.getBlock(block_position.x, block_position.y).id != BlockID::Air && blockDatabase[world.getBlock(block_position.x, block_position.y).id].breakable && isBlockInRange(transform, block_position, 4.0f) && will_drop)
                {
                    std::uniform_int_distribution drop_range(block_data.drop->min_amount, block_data.drop->max_amount);

                    ItemStack drop = {block_data.drop->drop, drop_range(rng)};

                    Entity new_entity(generateUUID());
                    new_entity.addComponent(TransformComponent{{block_position.x + 0.25f, block_position.y - 0.25f}, {0.5f, 0.5f}, sf::degrees(0.0f)});
                    new_entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
                    new_entity.addComponent(ItemComponent(drop));
                    new_entity.addComponent(RenderComponent{static_cast<unsigned short>(itemDatabase[new_entity.getComponent<ItemComponent>().item.itemID].texture), {{0, 0}, {16, 16}}, {0.5f, 0.5f}});
                    world.setBlock(block_position.x, block_position.y, {BlockID::Air, 0});

                    world.addEntity(std::move(new_entity));

                    player.mining_time = 0.0f;
                }
            }            
        }


        if(player.mid_usage)
        {
            sf::Vector2i block_position = block_from_position(player.cursor_position);

            auto& selected = inventory.inventory.slots[inventory.selectedSlot];

            if(selected.empty()) break;

            if((world.getBlock(block_position.x, block_position.y).id == BlockID::Air || world.getBlock(block_position.x, block_position.y).id == BlockID::Water) && isBlockInRange(transform, block_position, 4.0f) && itemDatabase[selected.itemID].category == ItemCategory::Block)
            {
                selected.quantity--;

                world.setBlock(block_position.x, block_position.y, {itemToBlock(selected.itemID), 0});
            }
            else if(itemDatabase[selected.itemID].category != ItemCategory::Block)
            {
                if(itemDatabase[selected.itemID].onUse(world, {static_cast<float>(player.cursor_position.x), static_cast<float>(player.cursor_position.y)}, id))
                {
                    selected.quantity--;
                }
            }
        }
    }
}