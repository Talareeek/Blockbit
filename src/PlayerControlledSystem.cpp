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

            if(world.getBlock(block_position.x, block_position.y).id != BlockID::Air && blockDatabase[world.getBlock(block_position.x, block_position.y).id].breakable && isBlockInRange(transform, block_position, 4.0f))
            {
                Entity new_entity(generateUUID());
                new_entity.addComponent(TransformComponent{{block_position.x + 0.25f, block_position.y - 0.25f}, {0.5f, 0.5f}, sf::degrees(0.0f)});
                new_entity.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
                new_entity.addComponent(ItemComponent{{blockToItem(world.getBlock(block_position.x, block_position.y).id), 1}});
                new_entity.addComponent(RenderComponent{static_cast<unsigned short>(itemDatabase[new_entity.getComponent<ItemComponent>().item.itemID].texture), {{0, 0}, {16, 16}}, {0.5f, 0.5f}});
                world.setBlock(block_position.x, block_position.y, {BlockID::Air, 0});

                world.addEntity(std::move(new_entity));
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