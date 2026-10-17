#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
#include "Entity.hpp"
#include "PerlinNoise.hpp"
#include "Climate.hpp"
#include "WorldGenerator.hpp"
#include "FlatWorldGenerator.hpp"

#include <unordered_map>
#include <filesystem>
#include <optional>
#include <deque>
#include <vector>

#include <SFML/System/Vector2.hpp>

struct GenerationProperties
{
    bool flat;
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

public:

    virtual ~World() = default;

    Chunk& getChunk(int chunk_position);
    Block getBlock(int world_x, int world_y);
    Climate climateAt(int wx);
    Biome biomeAt(int wx);
    virtual void setBlock(int wx, int wy, Block block);
    void setBlock(sf::Vector2i position, Block block);

    std::unordered_map<int, Chunk>& getChunks();

    std::vector<UUID> getPlayerEntityIDs() const;

    std::unordered_map<UUID, Entity>& getEntities();
    const std::unordered_map<UUID, Entity>& getEntities() const;
    virtual void addEntity(Entity entity);
    virtual void removeEntity(UUID id);
    Entity& getEntity(UUID id);
    bool doesEntityExist(UUID id) const;

    void tick(float dt);

    float fluidTimer{0.0f};

    float getDayTime() const { return dayTime; }

    std::pair<double, double> getSimulationRangeForEntity(const UUID entity);

    std::deque<PostPlaceBlockUpdate> pending_post_place_block_updates;

    //void handlePostPlaceBlockUpdates();

    float dayTime{0.0f};

    uint64_t days{0};

    static constexpr float DAY_CYCLE_DURATION = 1200.0f;
    static constexpr int SEA_LEVEL = 75;
    static constexpr float FLUID_TICK = 0.5f;
    static constexpr int SIMULATION_DISTANCE = 10;
    static constexpr int MAX_CHUNKS_LOADED = 24;
    static constexpr int PREFFERED_CHUNKS_LOADED = 16;
};

extern void updateFluids(World& world);

inline std::filesystem::path getWorldsPath()
{
    std::string home;

    #ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        home = appdata ? appdata : "";
    #elif __linux__
        const char* homeenv = std::getenv("HOME");
        home = homeenv ? homeenv : "";
    #endif

    std::filesystem::path savesPath = home.empty() ? std::filesystem::temp_directory_path() : std::filesystem::path(home);
    savesPath /= "Blockbit";
    savesPath /= "saves";

    return savesPath;
}

#endif // WORLD_HPP
