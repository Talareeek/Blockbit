#ifndef SERVER_WORLD_HPP
#define SERVER_WORLD_HPP

#include "World.hpp"
#include "Prefab.hpp"
#include "BlockUpdate.hpp"
#include "BlockUpdatePipeline.hpp"

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

public:

    ServerWorld(const std::filesystem::path path);
    ServerWorld(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties);

    void setBlock(int wx, int wy, Block block) override;
    void setBlock(int wx, int wy, Block block, BlockUpdateFlags flags);
    using World::setBlock;

    std::vector<sf::Vector2i> pending_block_updates;

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