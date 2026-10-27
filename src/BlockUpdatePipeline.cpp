#include "../include/BlockUpdatePipeline.hpp"
#include "../include/ServerWorld.hpp"
#include "../include/Prefab.hpp"

#include <algorithm>
#include <iostream>

BlockUpdatePipeline::BlockUpdatePipeline(ServerWorld& world)
    : world(world)
    , rng(std::random_device{}())
{
}

bool BlockUpdatePipeline::hasScheduledTick(sf::Vector2i position) const
{
    return scheduled_set.contains(packPos(position));
}

bool BlockUpdatePipeline::scheduleTick(sf::Vector2i position, int delay_ticks, uint32_t priority)
{
    if (delay_ticks < 1) delay_ticks = 1;

    uint64_t key = packPos(position);
    if (scheduled_set.contains(key)) return false;

    scheduled_heap.push_back(ScheduledTick{
        .position  = position,
        .fire_tick = current_tick + static_cast<uint64_t>(delay_ticks),
        .priority  = priority
    });
    std::push_heap(scheduled_heap.begin(), scheduled_heap.end());
    scheduled_set.insert(key);
    return true;
}

void BlockUpdatePipeline::setBlockSuppressNC(sf::Vector2i position, Block block)
{
    world.setBlock(position.x, position.y, block, BlockUpdateFlags::SuppressNC);
}

void BlockUpdatePipeline::drainPropagation()
{
    std::unordered_map<uint64_t, int> visit_counts;
    visit_counts.reserve(64);

    // Standard DFS: pop from the back of pending_propagation_events, process
    // (which may push more), repeat. Each processOneEvent uses a stack marker
    // so its own neighbor fan-out drains nested events in-place before moving
    // on — satisfying the spec's "finish B's subtree before C" rule.
    while (!world.pending_propagation_events.empty())
    {
        PendingPropagation ev = world.pending_propagation_events.back();
        world.pending_propagation_events.pop_back();

        if (ev.depth > max_propagation_depth)
        {
            std::cerr << "[BlockUpdatePipeline] depth limit (" << max_propagation_depth
                      << ") exceeded at (" << ev.position.x << ", " << ev.position.y
                      << "); dropping event\n";
            continue;
        }

        processOneEvent(ev, visit_counts);
    }
}

void BlockUpdatePipeline::drainNestedUpTo(
    size_t stack_marker,
    std::unordered_map<uint64_t, int>& visit_counts)
{
    while (world.pending_propagation_events.size() > stack_marker)
    {
        PendingPropagation nested = world.pending_propagation_events.back();
        world.pending_propagation_events.pop_back();

        if (nested.depth > max_propagation_depth)
        {
            std::cerr << "[BlockUpdatePipeline] depth limit (" << max_propagation_depth
                      << ") exceeded at (" << nested.position.x << ", " << nested.position.y
                      << "); dropping nested event\n";
            continue;
        }

        processOneEvent(nested, visit_counts);
    }
}

void BlockUpdatePipeline::processOneEvent(
    PendingPropagation ev,
    std::unordered_map<uint64_t, int>& visit_counts)
{
    BlockUpdateContext ctx{world, *this, current_tick};

    const auto prev_data_it = blockDatabase.find(ev.previous.id);
    const auto cur_data_it  = blockDatabase.find(ev.current.id);

    // onBreak fires only when a different block replaced a real previous one.
    // Pure metadata changes within the same id don't fire onBreak — those are
    // state changes, not removals.
    if (ev.previous.id != BlockID::Air
        && ev.previous.id != ev.current.id
        && prev_data_it != blockDatabase.end()
        && prev_data_it->second.callbacks.onBreak != nullptr)
    {
        const size_t marker = world.pending_propagation_events.size();
        prev_data_it->second.callbacks.onBreak(ctx, ev.position);
        drainNestedUpTo(marker, visit_counts);
    }

    // onPlace fires for a new or changed block id. The callback is a self-adjust
    // step: it may read neighbors and call setBlock on itself. If it does, that
    // writes a NEW event onto the stack; we drain nested events before continuing
    // the fan-out below, but our captured ev.previous/ev.current remain the
    // originals — the fan-out based on them is NOT "eaten" by self-mutation.
    if (ev.current.id != BlockID::Air
        && ev.previous.id != ev.current.id
        && cur_data_it != blockDatabase.end()
        && cur_data_it->second.callbacks.onPlace != nullptr)
    {
        const size_t marker = world.pending_propagation_events.size();
        cur_data_it->second.callbacks.onPlace(ctx, ev.position);
        drainNestedUpTo(marker, visit_counts);
    }

    // PP fan-out: tell each immediate neighbor "something next to you changed".
    // The direction we pass to the callback is from the receiver's point of
    // view — i.e. the direction FROM the receiver TO us.
    for (Direction d : PP_PROPAGATION_ORDER)
    {
        sf::Vector2i neighbor_pos = ev.position + direction_offsets[static_cast<int>(d)];
        Direction    from_dir     = opposite_direction[static_cast<int>(d)];

        Block neighbor = world.getBlock(neighbor_pos.x, neighbor_pos.y);
        auto neighbor_data_it = blockDatabase.find(neighbor.id);
        if (neighbor_data_it == blockDatabase.end()) continue;
        const BlockData& neighbor_data = neighbor_data_it->second;

        // Declarative support check: a change directly below a block that
        // requires support (from_dir == Down means we are its support) may
        // snap it off. Runs before the regular PP callback so a block that
        // just lost its ground doesn't also run its own PP logic.
        if (from_dir == Direction::Down && !neighbor_data.support_blocks_below.empty())
        {
            const auto& ids = neighbor_data.support_blocks_below;
            const bool supported = std::find(ids.begin(), ids.end(), ev.current.id) != ids.end();
            if (!supported)
            {
                world.setBlock(neighbor_pos.x, neighbor_pos.y, {BlockID::Air, 0});
                if (neighbor_data.drop.has_value())
                {
                    const auto& drop = neighbor_data.drop.value();
                    spawnDroppedItem(
                        world,
                        {static_cast<double>(neighbor_pos.x) + 0.25,
                         static_cast<double>(neighbor_pos.y) + 0.25},
                        ItemStack{drop.drop, drop.min_amount},
                        randomDropSpread(60.0f)
                    );
                }
                // The setBlock we just issued enqueues its own PendingPropagation
                // event; nested PP from it will cascade the break upward.
                const size_t marker = world.pending_propagation_events.size() - 1;
                drainNestedUpTo(marker, visit_counts);
                continue;
            }
        }

        if (neighbor_data.callbacks.onPostPlacement == nullptr) continue;
        if ((neighbor_data.pp_accept_mask & directionBit(from_dir)) == 0) continue;

        uint64_t neighbor_key = packPos(neighbor_pos);
        int& visits = visit_counts[neighbor_key];
        if (visits >= max_visits_per_epoch) continue;
        ++visits;

        const size_t marker = world.pending_propagation_events.size();
        neighbor_data.callbacks.onPostPlacement(ctx, neighbor_pos, static_cast<uint8_t>(from_dir));
        drainNestedUpTo(marker, visit_counts);
    }

    if (hasFlag(ev.flags, BlockUpdateFlags::SuppressNC)) return;

    // NC fan-out: logical neighbor notification. Standard 4 directions first,
    // then any extra_nc_offsets configured on the current block (wire-like).
    auto fire_nc = [&](sf::Vector2i target_pos)
    {
        Block target = world.getBlock(target_pos.x, target_pos.y);
        auto target_data_it = blockDatabase.find(target.id);
        if (target_data_it == blockDatabase.end()) return;
        if (target_data_it->second.callbacks.onNeighborChanged == nullptr) return;

        uint64_t key = packPos(target_pos);
        int& visits = visit_counts[key];
        if (visits >= max_visits_per_epoch) return;
        ++visits;

        const size_t marker = world.pending_propagation_events.size();
        target_data_it->second.callbacks.onNeighborChanged(ctx, target_pos, ev.position);
        drainNestedUpTo(marker, visit_counts);
    };

    for (Direction d : NC_PROPAGATION_ORDER)
    {
        fire_nc(ev.position + direction_offsets[static_cast<int>(d)]);
    }

    if (cur_data_it != blockDatabase.end())
    {
        for (const sf::Vector2i& offset : cur_data_it->second.extra_nc_offsets)
        {
            fire_nc(ev.position + offset);
        }
    }
}

void BlockUpdatePipeline::processScheduled()
{
    BlockUpdateContext ctx{world, *this, current_tick};

    // Pop all entries whose fire_tick is <= current_tick. Each callback may
    // write blocks (pushing propagation events) or reschedule itself.
    while (!scheduled_heap.empty() && scheduled_heap.front().fire_tick <= current_tick)
    {
        std::pop_heap(scheduled_heap.begin(), scheduled_heap.end());
        ScheduledTick st = scheduled_heap.back();
        scheduled_heap.pop_back();
        scheduled_set.erase(packPos(st.position));

        Block at = world.getBlock(st.position.x, st.position.y);
        auto it = blockDatabase.find(at.id);
        if (it == blockDatabase.end()) continue;
        if (it->second.callbacks.onScheduledTick == nullptr) continue;

        it->second.callbacks.onScheduledTick(ctx, st.position);

        // Any writes the callback made are now in pending_propagation_events;
        // drain them before processing the next scheduled entry so cascades
        // (e.g. falling-sand chain) resolve one at a time.
        drainPropagation();
    }
}

void BlockUpdatePipeline::runRandomTicks()
{
    if (random_ticks_per_chunk <= 0) return;

    BlockUpdateContext ctx{world, *this, current_tick};
    std::uniform_int_distribution<int> dist_x(0, 15);
    std::uniform_int_distribution<int> dist_y(0, 255);

    for (auto& [chunk_position, chunk] : world.getChunks())
    {
        if (!chunk.generated) continue;

        for (int i = 0; i < random_ticks_per_chunk; ++i)
        {
            int lx = dist_x(rng);
            int ly = dist_y(rng);
            Block b = chunk.blocks[ly][lx];

            auto it = blockDatabase.find(b.id);
            if (it == blockDatabase.end()) continue;
            if (!it->second.random_tickable) continue;
            if (it->second.callbacks.onRandomTick == nullptr) continue;

            sf::Vector2i wpos{chunk_position * 16 + lx, ly};
            it->second.callbacks.onRandomTick(ctx, wpos);
        }
    }

    drainPropagation();
}

Tag BlockUpdatePipeline::serializeChunk(int chunk_position) const
{
    TagList ticks;

    for (const ScheduledTick& st : scheduled_heap)
    {
        if (chunkOf(st.position.x) != chunk_position) continue;

        TagCompound entry;
        entry["pos"]      = Tag(st.position);
        // Store as DELAY (fire_tick - current_tick) instead of absolute, so the
        // schedule survives a server restart that resets the tick counter.
        uint64_t delay = (st.fire_tick > current_tick) ? (st.fire_tick - current_tick) : 1ull;
        entry["delay"]    = Tag(delay);
        entry["priority"] = Tag(st.priority);
        ticks.push_back(Tag(entry));
    }

    TagCompound root;
    root["ticks"] = Tag(ticks);
    return Tag(root);
}

void BlockUpdatePipeline::deserializeChunk(int chunk_position, const Tag& tag)
{
    const TagCompound& root = tag.get<TagCompound>();
    auto it = root.find("ticks");
    if (it == root.end()) return;

    const TagList& ticks = it->second.get<TagList>();

    for (const Tag& entry_tag : ticks)
    {
        const TagCompound& entry = entry_tag.get<TagCompound>();

        auto pos_it      = entry.find("pos");
        auto delay_it    = entry.find("delay");
        auto priority_it = entry.find("priority");
        if (pos_it == entry.end() || delay_it == entry.end()) continue;

        sf::Vector2i pos = pos_it->second.get<sf::Vector2i>();
        if (chunkOf(pos.x) != chunk_position) continue;  // belongs to another chunk

        uint64_t delay = delay_it->second.get<uint64_t>();
        uint32_t priority = (priority_it != entry.end()) ? priority_it->second.get<uint32_t>() : 0;

        uint64_t key = packPos(pos);
        if (scheduled_set.contains(key)) continue;

        scheduled_heap.push_back(ScheduledTick{
            .position  = pos,
            .fire_tick = current_tick + delay,
            .priority  = priority
        });
        std::push_heap(scheduled_heap.begin(), scheduled_heap.end());
        scheduled_set.insert(key);
    }
}

void BlockUpdatePipeline::dropChunk(int chunk_position)
{
    size_t write = 0;
    for (size_t read = 0; read < scheduled_heap.size(); ++read)
    {
        const ScheduledTick& st = scheduled_heap[read];
        if (chunkOf(st.position.x) == chunk_position)
        {
            scheduled_set.erase(packPos(st.position));
            continue;
        }
        if (write != read) scheduled_heap[write] = st;
        ++write;
    }
    scheduled_heap.resize(write);
    std::make_heap(scheduled_heap.begin(), scheduled_heap.end());
}
