#include "../include/PlayerControlledSystem.hpp"
#include "../include/ItemBehavior.hpp"
#include "../include/GameCommon.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/EffectsComponent.hpp"
#include "../include/ChestView.hpp"
#include "../include/CraftingView.hpp"
#include "../include/FurnaceView.hpp"
#include "../include/UpgradeView.hpp"
#include "../include/Prefab.hpp"
#include "../include/ItemUtils.hpp"

#include <queue>

namespace
{
    void updateRubyCircuit(World& world, sf::Vector2i source)
    {
        // Keep established input-side orientation: N/E/S/W. Output direction
        // follows the gate's facing in E/S/W/N order.
        constexpr sf::Vector2i offsets[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        constexpr sf::Vector2i output_offsets[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
        constexpr int radius = 64;
        const int min_x = source.x - radius;
        const int max_x = source.x + radius;
        const int min_y = std::max(0, source.y - radius);
        const int max_y = std::min(CHUNK_HEIGHT - 1, source.y + radius);
        std::vector<sf::Vector2i> gates;
        std::queue<sf::Vector2i> pending;

        // Clear all derived state first so turning a source off also turns off
        // gates and outputs that no longer have a valid signal.
        for (int x = min_x; x <= max_x; ++x)
            for (int y = min_y; y <= max_y; ++y)
            {
                const Block b = world.getBlock(x, y);
                if (b.id == Blocks::Ruby_Wire)
                {
                    const WireState state = WireState::unpack(b.metadata);
                    if (state.powered) world.setBlock(x, y, {b.id, WireState{state.connections, false}.pack()});
                }
                else if (b.id == Blocks::Output_Bulb)
                {
                    if (BulbState::unpack(b.metadata).powered) world.setBlock(x, y, {b.id, BulbState{false}.pack()});
                }
                else if (b.id == Blocks::And_Gate || b.id == Blocks::Or_Gate || b.id == Blocks::Not_Gate)
                {
                    const GateState state = GateState::unpack(b.metadata);
                    gates.push_back({x, y});
                    if (state.powered) world.setBlock(x, y, {b.id, GateState{state.facing, false}.pack()});
                }
                else if (b.id == Blocks::Input_Bulb && BulbState::unpack(b.metadata).powered)
                    pending.push({x, y});
            }

        auto energize = [&](sf::Vector2i p)
        {
            const Block b = world.getBlock(p.x, p.y);
            if (b.id == Blocks::Ruby_Wire)
            {
                const WireState state = WireState::unpack(b.metadata);
                if (!state.powered)
                {
                    world.setBlock(p, {b.id, WireState{state.connections, true}.pack()});
                    pending.push(p);
                }
            }
            else if (b.id == Blocks::Output_Bulb && !BulbState::unpack(b.metadata).powered)
                world.setBlock(p, {b.id, BulbState{true}.pack()});
        };

        auto drain_wires = [&]()
        {
            while (!pending.empty())
            {
                const sf::Vector2i p = pending.front();
                pending.pop();
                for (const sf::Vector2i d : offsets)
                {
                    const sf::Vector2i n = p + d;
                    if (n.x < min_x || n.x > max_x || n.y < min_y || n.y > max_y) continue;
                    energize(n);
                }
            }
        };
        drain_wires();

        // Repeatedly evaluate gates and propagate their outputs until no new
        // wire can be powered. This also handles gate-to-gate chains and NOT
        // gates whose output is active while their back input is unpowered.
        const size_t max_passes = gates.size() + 1;
        for (size_t pass = 0; pass < max_passes; ++pass)
        {
            bool changed = false;
            for (const sf::Vector2i gate_pos : gates)
            {
                const Block gate = world.getBlock(gate_pos.x, gate_pos.y);
                const GateState state = GateState::unpack(gate.metadata);
                const sf::Vector2i facing = offsets[state.facing];
                const sf::Vector2i side{-facing.y, facing.x};
                auto powered_at = [&](sf::Vector2i p)
                {
                    const Block in = world.getBlock(p.x, p.y);
                    return (in.id == Blocks::Input_Bulb && BulbState::unpack(in.metadata).powered)
                        || (in.id == Blocks::Ruby_Wire && WireState::unpack(in.metadata).powered);
                };
                const bool first = powered_at(gate_pos + side);
                const bool second = powered_at(gate_pos - side);
                const bool active = gate.id == Blocks::Not_Gate ? !powered_at(gate_pos - facing)
                                   : gate.id == Blocks::And_Gate ? first && second
                                   : first || second;
                if (!active || state.powered) continue;

                world.setBlock(gate_pos, {gate.id, GateState{state.facing, true}.pack()});
                const sf::Vector2i out = gate_pos + output_offsets[state.facing];
                const Block target = world.getBlock(out.x, out.y);
                if (target.id == Blocks::Ruby_Wire && !WireState::unpack(target.metadata).powered)
                {
                    energize(out);
                    changed = true;
                }
                else if (target.id == Blocks::Output_Bulb && !BulbState::unpack(target.metadata).powered)
                {
                    energize(out);
                    changed = true;
                }
            }
            drain_wires();
            if (!changed) break;
        }
    }
}

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

        if (entity.hasComponent<EffectsComponent>())
        {
            const double cx = transform.center().x;
            const bool outside = (cx >= static_cast<double>(World::BORDER_X))
                              || (cx <  static_cast<double>(-World::BORDER_X));
            if (outside)
            {
                auto& effects = entity.getComponent<EffectsComponent>();
                EffectInstance* existing = nullptr;
                for (auto& e : effects.effects)
                {
                    if (e.id == EffectID::BoundaryDistortion) { existing = &e; break; }
                }
                if (!existing)
                {
                    EffectInstance e;
                    e.id                 = EffectID::BoundaryDistortion;
                    e.interval           = 2.0f;
                    e.since_last_tick    = 0.0f;
                    e.remaining_duration = 180.0f;
                    e.magnitude          = 1;
                    effects.effects.push_back(e);
                }
                else
                {
                    existing->remaining_duration = 180.0f;
                }
                effects.dirty = true;
            }
        }

        if(player.noclip && entity.hasComponent<PhysicsComponent>())
        {
            constexpr double NOCLIP_SPEED = 20.0;

            auto& physics = entity.getComponent<PhysicsComponent>();

            if(player.jump)
            {
                sf::Vector2<double> dir = player.cursor_position - transform.center();
                double len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                if(len > 1e-6)
                {
                    dir.x /= len;
                    dir.y /= len;

                    transform.position.x += dir.x * NOCLIP_SPEED * dt;
                    transform.position.y += dir.y * NOCLIP_SPEED * dt;
                    transform.render_position = transform.position;
                    transform.dirty = true;
                }

                player.jump = false;
                player.dirty = true;
            }

            physics.velocity = {0.0, 0.0};
            physics.force = {0.0, 0.0};
        }

        if (entity.hasComponent<AnimationComponent>() && entity.hasComponent<PhysicsComponent>())
        {
            auto& animation = entity.getComponent<AnimationComponent>();
            const auto& physics = entity.getComponent<PhysicsComponent>();

            AnimationState desired = std::abs(physics.velocity.x) > 0.5 ? AnimationState::Walking : AnimationState::Idle;

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

        if(!player.mid_attack && player.mining_time != 0.0f)
        {
            player.mining_time = 0.0f;
            player.dirty = true;
        }

        if(player.mid_attack)
        {
            sf::Vector2i target = block_from_position(player.cursor_position);
            bool in_range = isBlockInRange(transform, target, 4.0f);

            if(player.mining_block == target && in_range)
            {
                player.mining_time += dt;
            }
            else
            {
                player.mining_block = target;
                player.mining_time = 0.0f;
            }

            player.dirty = true;

            sf::Vector2i block_position = player.mining_block;

            Block block = world.getBlock(block_position.x, block_position.y);

            ItemStack item = inventory.inventory.slots[inventory.selectedSlot];

            if(in_range && blockDatabase[block.id].mining.has_value())
            {
                auto& block_data = blockDatabase[block.id];
                auto& item_data  = itemDatabase[item.itemID];

                bool correct_type = item_data.tool.has_value() && item_data.tool->tool_type == block_data.mining->desired_tool.tool_type;
                int tool_level  = correct_type ? item_data.tool->level : 0;
                bool proper_tool = correct_type && item_data.tool->level >= block_data.mining->desired_tool.level;

                float speed = level_based_speed[tool_level];
                // Ruby tools gain extra bite from their Rubyness attunement.
                // Alpha (default) is 1x; Omega is 10x.
                if (uint8_t ruby_level = getRubynessLevel(item); ruby_level >= RUBYNESS_MIN_LEVEL)
                {
                    speed *= rubynessMultiplier(ruby_level);
                }
                // Using the wrong tool type adds a time penalty on top of the hand-speed baseline,
                // so a correct-but-low-level tool is always better than a wrong tool.
                float wrong_tool_penalty = correct_type ? 1.0f : 1.5f;
                float block_mining_time = (block_data.hardness < 0.0f) ? std::numeric_limits<float>::infinity() : (block_data.hardness * wrong_tool_penalty) / speed;

                if(player.mining_time >= block_mining_time && world.getBlock(block_position.x, block_position.y).id != Blocks::Air && blockDatabase[world.getBlock(block_position.x, block_position.y).id].breakable && isBlockInRange(transform, block_position, 4.0f))
                {
                    player.dirty = true;

                    world.setBlock(block_position.x, block_position.y, {Blocks::Air, 0});

                    if(block_data.drop.has_value() && (proper_tool || block_data.drop->always_drop))
                    {
                        ItemStack drop;
                        if(block_data.drop->alternative_drop_chance > 0.0f &&
                           std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < block_data.drop->alternative_drop_chance)
                        {
                            drop = {block_data.drop->alternative_drop, 1};
                        }
                        else
                        {
                            std::uniform_int_distribution drop_range(block_data.drop->min_amount, block_data.drop->max_amount);
                            drop = {block_data.drop->drop, drop_range(rng)};
                        }
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
        player.since_last_attack += dt;
        player.dirty = true;

        if(player.mid_usage && player.since_last_use >= 0.2f)
        {
            sf::Vector2i block_position = block_from_position(player.cursor_position);

            Block clicked = world.getBlock(block_position.x, block_position.y);
            if (clicked.id == Blocks::Input_Bulb && isBlockInRange(transform, block_position, 4.0f))
            {
                const bool powered = !BulbState::unpack(clicked.metadata).powered;
                world.setBlock(block_position, {clicked.id, BulbState{powered}.pack()});
                if (powered) updateRubyCircuit(world, block_position);
                else
                {
                    // Recompute the connected wire/output state from all active inputs.
                    for (int x = block_position.x - 64; x <= block_position.x + 64; ++x)
                        for (int y = std::max(0, block_position.y - 64); y < std::min(CHUNK_HEIGHT, block_position.y + 65); ++y)
                        {
                            Block b = world.getBlock(x, y);
                            if (b.id == Blocks::Ruby_Wire) world.setBlock(x, y, {b.id, WireState{WireState::unpack(b.metadata).connections, false}.pack()});
                            else if (b.id == Blocks::Output_Bulb) world.setBlock(x, y, {b.id, BulbState{false}.pack()});
                            else if (b.id == Blocks::And_Gate || b.id == Blocks::Or_Gate || b.id == Blocks::Not_Gate)
                            {
                                const auto state = GateState::unpack(b.metadata);
                                world.setBlock(x, y, {b.id, GateState{state.facing, false}.pack()});
                            }
                        }
                    // Re-energize circuits fed by any other active input in this region.
                    for (int x = block_position.x - 64; x <= block_position.x + 64; ++x)
                        for (int y = std::max(0, block_position.y - 64); y < std::min(CHUNK_HEIGHT, block_position.y + 65); ++y)
                            if (world.getBlock(x, y).id == Blocks::Input_Bulb && BulbState::unpack(world.getBlock(x, y).metadata).powered)
                                updateRubyCircuit(world, {x, y});
                }
                player.mid_usage = false; player.since_last_use = 0.0f;
                continue;
            }

            if(world.getBlock(block_position.x, block_position.y).id == Blocks::Chest && isBlockInRange(transform, block_position, 4.0f))
            {
                player.pending_view_opens.push_back(std::make_unique<ServerChestView>(block_position, id));
                player.mid_usage = false;
                player.since_last_use = 0.0f;
                player.dirty = true;
                continue;
            }

            if(world.getBlock(block_position.x, block_position.y).id == Blocks::Workbench && isBlockInRange(transform, block_position, 4.0f))
            {
                player.pending_view_opens.push_back(std::make_unique<ServerCraftingView>(block_position, id));
                player.mid_usage = false;
                player.since_last_use = 0.0f;
                player.dirty = true;
                continue;
            }

            if(world.getBlock(block_position.x, block_position.y).id == Blocks::Upgrade_Table && isBlockInRange(transform, block_position, 4.0f))
            {
                player.pending_view_opens.push_back(std::make_unique<ServerUpgradeView>(block_position, id));
                player.mid_usage = false;
                player.since_last_use = 0.0f;
                player.dirty = true;
                continue;
            }

            if(world.getBlock(block_position.x, block_position.y).id == Blocks::Furnace && isBlockInRange(transform, block_position, 4.0f))
            {
                auto& selected = inventory.inventory.slots[inventory.selectedSlot];

                // Clicking a furnace with a Lighter or Flint in hand should
                // still light it, not pop open the UI. Fall through to the
                // item-use path.
                bool lights_furnace = !selected.empty() && (selected.itemID == Items::Lighter || selected.itemID == Items::Flint);
                if(!lights_furnace)
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

            if(block_position.y >= 0 && block_position.y < CHUNK_HEIGHT && (world.getBlock(block_position.x, block_position.y).id == Blocks::Air || world.getBlock(block_position.x, block_position.y).id == Blocks::Water || world.getBlock(block_position.x, block_position.y).id == Blocks::Lava) && isBlockInRange(transform, block_position, 4.0f) && itemDatabase[selected.itemID].category == ItemCategory::Block)
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

                const BlockKey placing = itemToBlock(selected.itemID);

                if(!collides && canPlaceBlock(world, placing, block_position))
                {
                    selected.quantity--;
                    inventory.dirty = true;

                    uint16_t placed_metadata = 0;
                    if (placing == Blocks::And_Gate || placing == Blocks::Or_Gate || placing == Blocks::Not_Gate)
                    {
                        const uint8_t facing = entity.hasComponent<AnimationComponent>()
                            && entity.getComponent<AnimationComponent>().direction == AnimationComponent::Direction::Left ? 2 : 0;
                        placed_metadata = GateState{facing, false}.pack();
                    }
                    world.setBlock(block_position.x, block_position.y, {placing, placed_metadata});
                }
            }
            else if(itemDatabase[selected.itemID].category != ItemCategory::Block)
            {
                const auto& data = itemDatabase[selected.itemID];
                bool consumed = false;

                // Declarative behavior takes precedence; `None` means the
                // item hasn't been ported yet so the legacy onUse lambda
                // still runs. This fallback goes away at the end of Phase 1b.
                if (data.behavior.type != ItemBehaviorType::None)
                    consumed = dispatchItemUse(data.behavior, world, player.cursor_position, id, selected);
                else
                    consumed = data.onUse(world, player.cursor_position, id, selected);

                if (consumed)
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
                stack = {Items::None, 0};
            }
            else
            {
                to_drop = {stack.itemID, 1};
                if(--stack.quantity == 0)
                {
                    stack.itemID = Items::None;
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
