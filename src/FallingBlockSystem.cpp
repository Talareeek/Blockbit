#include "../include/FallingBlockSystem.hpp"
#include "../include/FallingBlockComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/Prefab.hpp"
#include "../include/Item.hpp"

#include <vector>

namespace
{
    inline int ifloor(double v) noexcept
    {
        int i = static_cast<int>(v);
        return (v < static_cast<double>(i)) ? i - 1 : i;
    }

    // Can a falling block land into this cell? Air and water yes; anything else
    // is treated as an obstruction and the block drops as an item instead
    // (matches MC: sand lands on open torch → torch pops as item, sand lays down).
    bool is_landable(Block b)
    {
        return b.id == BlockID::Air || b.id == BlockID::Water;
    }
}

void FallingBlockSystem(ServerWorld& world)
{
    // Decide first, mutate after — avoids iterator invalidation and keeps the
    // iteration over world.getEntities() clean.
    struct LandingResolution
    {
        UUID         entity_id;
        sf::Vector2i cell;
        BlockID      block_id;
        uint8_t      metadata;
        bool         place_block;   // false → drop as item instead
    };

    std::vector<LandingResolution> resolutions;

    for (auto& [id, entity] : world.getEntities())
    {
        if (!entity.hasComponent<FallingBlockComponent>()) continue;
        if (!entity.hasComponent<PhysicsComponent>())      continue;
        if (!entity.hasComponent<TransformComponent>())    continue;

        const auto& physics = entity.getComponent<PhysicsComponent>();
        if (!physics.onGround) continue;

        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& falling   = entity.getComponent<FallingBlockComponent>();

        // Entity is 1x1 and position is bottom-left (per TransformComponent
        // convention used by DroppedItem/Pig/Player). Resting cell is the
        // tile under the entity's bottom edge.
        sf::Vector2i cell{
            ifloor(transform.position.x + transform.size.x * 0.5),
            ifloor(transform.position.y + 0.001)  // nudge so we don't pick the tile below the floor
        };

        Block at_cell = world.getBlock(cell.x, cell.y);
        bool  can_place = is_landable(at_cell);

        resolutions.push_back(LandingResolution{
            .entity_id   = id,
            .cell        = cell,
            .block_id    = falling.block_id,
            .metadata    = falling.metadata,
            .place_block = can_place
        });
    }

    for (const LandingResolution& r : resolutions)
    {
        if (r.place_block)
        {
            // Overwriting water with the carried block mirrors onFall from
            // sand_can_fall_into — water is displaced on contact.
            world.setBlock(r.cell.x, r.cell.y, Block{r.block_id, r.metadata});
        }
        else
        {
            // Can't place here: pop as an item at the entity's position so the
            // player doesn't just lose it.
            ItemID drop_item = blockToItem(r.block_id);
            if (drop_item != ItemID::None)
            {
                sf::Vector2<double> drop_pos{
                    static_cast<double>(r.cell.x) + 0.25,
                    static_cast<double>(r.cell.y) + 0.25
                };
                spawnDroppedItem(world, drop_pos, ItemStack{drop_item, 1});
            }
        }

        world.removeEntity(r.entity_id);
    }
}
