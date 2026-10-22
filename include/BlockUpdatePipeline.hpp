#ifndef BLOCK_UPDATE_PIPELINE_HPP
#define BLOCK_UPDATE_PIPELINE_HPP

#include "BlockUpdate.hpp"
#include "BBT.hpp"

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ServerWorld;
class BlockUpdatePipeline;

// Passed to every block callback. Carries the mutable world + the pipeline so
// callbacks can schedule follow-up ticks, write other blocks, etc.
struct BlockUpdateContext
{
    ServerWorld&          world;
    BlockUpdatePipeline&  pipeline;
    uint64_t              current_tick;
};

class BlockUpdatePipeline
{
public:
    explicit BlockUpdatePipeline(ServerWorld& world);

    // Current tick is pushed in before each stage runs; scheduleTick(delay=N)
    // resolves to fire_tick = current_tick + N.
    void setCurrentTick(uint64_t tick) { current_tick = tick; }

    // Drain pending_propagation_events on the world. Runs onBreak/onPlace,
    // then PP + NC fan-out DFS. Visits are capped per-epoch to prevent loops.
    void drainPropagation();

    // Fire any scheduled ticks whose fire_tick <= current_tick. Callbacks that
    // call world.setBlock will re-enter drainPropagation inside the loop.
    void processScheduled();

    // Pick `random_ticks_per_chunk` positions in each loaded chunk and fire
    // onRandomTick if the block there is random-tickable.
    void runRandomTicks();

    // Schedule onScheduledTick(pos) at current_tick + delay_ticks. Deduped by
    // position — if there's already a tick pending at pos, this one is dropped.
    // Returns true if the tick was actually enqueued.
    bool scheduleTick(sf::Vector2i position, int delay_ticks, uint32_t priority = 0);

    bool hasScheduledTick(sf::Vector2i position) const;

    // Convenience: setBlock through the world but mark the propagation to skip
    // NC fan-out (door-opens, growth, etc.). The caller already has `world`
    // handy via BlockUpdateContext.
    void setBlockSuppressNC(sf::Vector2i position, Block block);

    // Per-chunk save/load — pipeline writes the subset of scheduled ticks whose
    // position lives in the chunk, and reads them back on load.
    Tag serializeChunk(int chunk_position) const;
    void deserializeChunk(int chunk_position, const Tag& tag);

    // Called when a chunk is unloaded: drop any pending ticks for it so the
    // heap stays bounded. Ticks that weren't flushed to disk are lost — same
    // trade as MC's "unloaded chunks don't tick".
    void dropChunk(int chunk_position);

    // Config knobs. Public so a future command/CLI can tune at runtime.
    int random_ticks_per_chunk = 3;
    int max_propagation_depth  = 128;
    int max_visits_per_epoch   = 32;

private:
    ServerWorld& world;
    uint64_t     current_tick = 0;

    // Min-heap over fire_tick. We manage the heap manually (push_heap/pop_heap)
    // instead of std::priority_queue so we can iterate the underlying vector
    // for save-time filtering by chunk.
    std::vector<ScheduledTick>    scheduled_heap;
    std::unordered_set<uint64_t>  scheduled_set;  // packed-position dedup

    std::mt19937 rng;

    static uint64_t packPos(sf::Vector2i p)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(p.x)) << 32)
             |  static_cast<uint64_t>(static_cast<uint32_t>(p.y));
    }

    static int chunkOf(int world_x)
    {
        return (world_x >= 0) ? world_x / 16 : (world_x - 16 + 1) / 16;
    }

    void processOneEvent(PendingPropagation ev,
                         std::unordered_map<uint64_t, int>& visit_counts);

    void drainNestedUpTo(size_t stack_marker,
                         std::unordered_map<uint64_t, int>& visit_counts);
};

#endif // BLOCK_UPDATE_PIPELINE_HPP
