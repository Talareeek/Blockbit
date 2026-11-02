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

    bool spawn_item_chest_column = false;
};

class World
{
protected:

    std::unordered_map<int, Chunk> chunks;
    std::unordered_map<UUID, Entity> entities;

public:

    virtual ~World() = default;

    // Server owns the loaded/lazy partition; on a plain World (client-side) we
    // treat every chunk the client knows about as loaded, since the client
    // only holds chunks the server has streamed to it.
    virtual ChunkState chunkState(int) const { return ChunkState::Loaded; }
    virtual bool isChunkLoaded(int) const { return true; }
    virtual bool isChunkLazy(int) const { return false; }

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
    // chunks; blocks tick in loaded + lazy. The lazy ring extent itself lives
    // on ServerWorld since only the server computes and uses it.
    static constexpr int SIMULATION_DISTANCE = 8;
};

// Forward-declared in GameCommon.hpp; defined in src/GameCommon.cpp. Picks
// the right per-OS base dir (incl. an Android branch that reads
// ANativeActivity::internalDataPath).
std::filesystem::path getHomePath();

inline std::filesystem::path getWorldsPath()
{
    return getHomePath() / "Blockbit" / "saves";
}

#endif // WORLD_HPP
