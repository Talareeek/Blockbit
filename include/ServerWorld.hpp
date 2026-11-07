#ifndef SERVER_WORLD_HPP
#define SERVER_WORLD_HPP

#include "World.hpp"
#include "Prefab.hpp"
#include "BlockUpdate.hpp"
#include "BlockUpdatePipeline.hpp"
#include "Packet.hpp"

#include <unordered_set>

class ServerWorld : public World
{
private:

    std::filesystem::path path;
    std::string name = "world";

    unsigned int seed;

    std::unique_ptr<WorldGenerator> world_generator;
    GenerationProperties generation_properties;
    PerlinNoise perlin{0};

    // Snapshot of the chunk-state partition for the current tick, recomputed by
    // ChunkLoadSystem from player positions. Everything else (entity systems,
    // random ticks, unload) reads from these to avoid O(players) scans.
    std::unordered_set<int> loaded_chunk_positions;
    std::unordered_set<int> lazy_chunk_positions;

public:

    ServerWorld(const std::filesystem::path path);
    ServerWorld(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties);

    void setBlock(int wx, int wy, Block block) override;
    void setBlock(int wx, int wy, Block block, BlockUpdateFlags flags);
    using World::setBlock;

    void refreshChunkStates();

    ChunkState chunkState(int chunk_position) const override;
    bool isChunkLoaded(int chunk_position) const override;
    bool isChunkLazy(int chunk_position) const override;

    const std::unordered_set<int>& loadedChunks() const { return loaded_chunk_positions; }
    const std::unordered_set<int>& lazyChunks()   const { return lazy_chunk_positions; }

    // Lazy is a single-ring buffer past the loaded edge, so block updates (e.g.
    // water flow, scheduled ticks) can propagate just outside the player's
    // active area without entity cost.
    static constexpr int LAZY_DISTANCE = SIMULATION_DISTANCE + 1;

    std::vector<sf::Vector2i> pending_block_updates;

    // One-shot particle events queued this tick. GameServer drains this into
    // ParticleBurstPacket broadcasts right after broadcastSnapshot and clears.
    std::vector<ParticleBurstPacket> pending_particle_bursts;

    // Drained by BlockUpdatePipeline each tick. Separate from pending_block_updates
    // (which is just a positions list for client broadcast) because propagation
    // needs the pre/post block pair.
    std::vector<PendingPropagation> pending_propagation_events;

    BlockUpdatePipeline& getBlockUpdatePipeline() { return block_update_pipeline; }

private:
    BlockUpdatePipeline block_update_pipeline{*this};

public:
    std::unordered_set<UUID> new_entities;
    std::unordered_set<UUID> dead_entities;

    void addEntity(Entity entity) override;
    void removeEntity(UUID id) override;

    const std::string& getName() const { return name; }
    void setName(const std::string& name) { this->name = name; }

    UUID spawn(PrefabID prefab_id, sf::Vector2<double> position);

    sf::Vector2<double> getSpawnPoint();
    void generateWorldSpawn();
    void placeStarterItemChestColumn();

    void trySpawnPassiveMobs(float dt);

    void save();
    void load();

    // MANIFEST
    void saveManifest();
    void loadManifest();

    // DATA
    void saveData();
    void loadData();

    // ITEM INSTANCES
    void saveItemInstances();
    void loadItemInstances();

    // Mark-and-sweep over the global itemInstances map. Roots:
    //   - every ItemStack referenced by an InventoryComponent on a live entity
    //   - every ItemStack on a dropped-item ItemComponent
    //   - every ItemStack inside a ChestBlockEntity or FurnaceBlockEntity, across
    //     every loaded chunk
    //   - every UUID in extra_live_ids (contributed by active ServerView
    //     cursor stacks; GameServer collects these before calling)
    // Anything in itemInstances not covered is erased. Safe to call any time;
    // GameServer calls it periodically and right before save().
    void garbageCollectItemInstances(const std::unordered_set<UUID>& extra_live_ids);

    // CHUNK
    void saveChunk(int chunk_position);
    void saveChunkEnvironment(int chunk_position);
    void saveChunkEntities(int chunk_position);
    void saveChunkBlockEntities(int chunk_position);
    void saveChunkScheduledTicks(int chunk_position);
    void savePlayer(UUID entity_id);

    void loadChunk(int chunk_position);
    void loadChunkEnvironment(int chunk_position);
    void loadChunkEntities(int chunk_position);
    void loadChunkBlockEntities(int chunk_position);
    void loadChunkScheduledTicks(int chunk_position);
    UUID loadPlayer(std::string nickname);
    

    bool hasChunkFile(int chunk_position) const;
    bool playerFileExist(std::string nickname) const;

    void loadOrCreateChunk(int chunk_position);

};

#endif // SERVER_WORLD_HPP