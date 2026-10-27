#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
#include "Entity.hpp"
#include "PerlinNoise.hpp"
#include "Climate.hpp"
#include "WorldGenerator.hpp"
#include "FlatWorldGenerator.hpp"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <optional>
#include <vector>

#include <SFML/System/Vector2.hpp>

enum class ChunkState : uint8_t
{
    Unloaded = 0,
    Lazy     = 1,
    Loaded   = 2
};

enum class GenerationType : uint32_t
{
    Normal         = 0,
    Flat           = 1,
    DungeonShooter = 2
};

struct GenerationProperties
{
    GenerationType type = GenerationType::Normal;
    float base_height;
    float height_scale;
    float frequency;
    float amplitude;
    float persistence;

    std::vector<FlatWorldLayer> flat_layers = FlatWorldProperties::defaults().layers;
};

class World
{
protected:

    std::unordered_map<int, Chunk> chunks;
    std::unordered_map<UUID, Entity> entities;

    // Snapshot of the chunk-state partition for the current tick, recomputed by
    // ChunkLoadSystem from player positions. Everything else (entity systems,
    // random ticks, unload) reads from these to avoid O(players) scans.
    std::unordered_set<int> loaded_chunk_positions;
    std::unordered_set<int> lazy_chunk_positions;

public:

    virtual ~World() = default;

    void refreshChunkStates();

    ChunkState chunkState(int chunk_position) const;
    bool isChunkLoaded(int chunk_position) const;
    bool isChunkLazy(int chunk_position) const;

    const std::unordered_set<int>& loadedChunks() const { return loaded_chunk_positions; }
    const std::unordered_set<int>& lazyChunks()   const { return lazy_chunk_positions; }

    Chunk& getChunk(int chunk_position);
    Block getBlock(int world_x, int world_y);
    Climate climateAt(int wx);
    Biome biomeAt(int wx);
    virtual void setBlock(int wx, int wy, Block block);
    void setBlock(sf::Vector2i position, Block block);

    // Lightmap queries. Return 0 for cells outside loaded chunks or where
    // lighting has not been initialized yet.
    uint8_t getBlockLight(int wx, int wy) const;
    uint8_t getSkyLight(int wx, int wy) const;

    BlockEntity* getBlockEntity(int world_x, int world_y);
    void setBlockEntity(int world_x, int world_y, std::unique_ptr<BlockEntity> entity);
    void removeBlockEntity(int world_x, int world_y);

    std::unordered_map<int, Chunk>& getChunks();

    std::vector<UUID> getPlayerEntityIDs() const;

    std::unordered_map<UUID, Entity>& getEntities();
    const std::unordered_map<UUID, Entity>& getEntities() const;
    virtual void addEntity(Entity entity);
    virtual void removeEntity(UUID id);
    Entity& getEntity(UUID id);
    bool doesEntityExist(UUID id) const;

    void tick(float dt);

    float getDayTime() const { return dayTime; }

    std::pair<double, double> getSimulationRangeForEntity(const UUID entity);

    float dayTime{0.0f};

    uint64_t days{0};

    static constexpr float DAY_CYCLE_DURATION = 1200.0f;
    static constexpr int SEA_LEVEL  = 75;
    static constexpr int LAVA_LEVEL = 12;

    // World border — every column at X >= BORDER_X is treated as solid by
    // physics and painted as a diagonal-stripe wall by the client. Positioned
    // absurdly far out so ordinary play never reaches it; it's effectively the
    // max playable X.
    static constexpr int BORDER_X = 100000000;

    // Loaded radius (in chunks) around each player. Entities tick only in loaded
    // chunks; blocks tick in loaded + lazy. Lazy is a single-ring buffer past
    // the loaded edge, so block updates (e.g. water flow, scheduled ticks) can
    // propagate just outside the player's active area without entity cost.
    static constexpr int SIMULATION_DISTANCE = 8;
    static constexpr int LAZY_DISTANCE       = SIMULATION_DISTANCE + 1;
};

inline std::filesystem::path getWorldsPath()
{
    std::string home;

    #ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        home = appdata ? appdata : "";
    #else
        const char* homeenv = std::getenv("HOME");
        home = homeenv ? homeenv : "";
    #endif

    std::filesystem::path savesPath = home.empty() ? std::filesystem::temp_directory_path() : std::filesystem::path(home);
    savesPath /= "Blockbit";
    savesPath /= "saves";

    return savesPath;
}

#endif // WORLD_HPP
