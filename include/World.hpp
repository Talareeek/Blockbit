#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
#include "Entity.hpp"
#include "PerlinNoise.hpp"
#include "Climate.hpp"

#include <unordered_map>
#include <filesystem>
#include <optional>

#include <SFML/System/Vector2.hpp>

struct GenerationProperties
{
    bool flat;
    float base_height;
    float height_scale;
    float frequency;
    float amplitude;
    float persistence;
};

inline std::unordered_map<Biome, std::pair<Block, Block>> surface_blocks = 
{
    {Biome::Plains, {{BlockID::Grass, 0}, {BlockID::Dirt, 0}}},
    {Biome::Forest, {{BlockID::Grass, 0}, {BlockID::Dirt, 0}}},
    {Biome::Ocean, {{BlockID::Sand, 0}, {BlockID::Sand, 0}}},
    {Biome::Desert, {{BlockID::Sand, 0}, {BlockID::Sand, 0}}},
    {Biome::Savanna, {{BlockID::Coarse_Dirt, 0}, {BlockID::Coarse_Dirt, 0}}},
    {Biome::Mountains, {{BlockID::Stone, 0}, {BlockID::Stone, 0}}},
    {Biome::Snow, {{BlockID::Snow, 0}, {BlockID::Snow, 0}}},
};


class World
{
private:

    std::unordered_map<int, Chunk> chunks;
    std::unordered_map<UUID, Entity> entities;

    float time = 0.0f;


    std::filesystem::path path;
    std::string name = "world";

    unsigned int seed;
    GenerationProperties generation_properties;
    PerlinNoise perlin{0};

public:

    World() {};
    World(const std::filesystem::path path);
    World(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties);

    Chunk& getChunk(int chunk_position);
    Block getBlock(int world_x, int world_y);
    Climate climateAt(int wx);
    Biome biomeAt(int wx);
    void setBlock(int wx, int wy, Block block);

    std::unordered_map<int, Chunk>& getChunks();

    // GENERATION

    // flat world
    void generateFlatWorld();
    void generateFlatChunk(int chunk_position);

    // normal world
    void generateWorldSpawn();

    [[deprecated]]UUID spawnPlayer(uint32_t clientId);

    std::vector<UUID> getPlayerEntityIDs() const;

    Climate getClimate(int x) const;
    Biome getBiome(int x) const;

    void generateChunk(int chunk_position);
    void loadOrCreateChunk(int chunk_position);

    // phases
    void generateTerrain(int chunk_position);
    void generateCaves(int chunk_position);
    void generateVein(int x, int y, BlockID ore, int size);
    void generateOres(int chunk_position);
    void generateTree(int x, int y, int log_height, BlockID log_type, BlockID leaves_type);
    void generateNature(int chunk_position);

    std::unordered_map<UUID, Entity>& getEntities();
    const std::unordered_map<UUID, Entity>& getEntities() const;
    void addEntity(Entity entity);
    void removeEntity(UUID id);
    Entity& getEntity(UUID id);
    bool doesEntityExist(UUID id);
    
    float getHeightNoise(float x) const;
    int getHeight(int worldX) const;

    void tick(float dt);

    float fluidTimer{0.0f};

    float getDayTime() const { return dayTime; }

    sf::Vector2<double> getSpawnPoint();

    const std::string& getName() const { return name; }

    void setName(const std::string& name) { this->name = name; }


    void save();
    void load();

    // MANIFEST
    void saveManifest();
    void loadManifest();

    // DATA
    void saveData();
    void loadData();

    // CHUNK
    void saveChunk(int chunk_position);
    void saveChunkEnvironment(int chunk_position);
    void saveChunkEntities(int chunk_position);

    void loadChunk(int chunk_position);   
    void loadChunkEnvironment(int chunk_position); 
    void loadChunkEntities(int chunk_position);
    

    bool hasChunkFile(int chunk_position) const;

    std::pair<double, double> getSimulationRangeForEntity(const UUID entity);

    bool trackBlockChanges = false;
    std::vector<std::tuple<int, int, Block>> pendingBlockUpdates;

    float dayTime{0.0f};

    uint64_t days{0};

    static constexpr float DAY_CYCLE_DURATION = 1200.0f; // 20 MINUTES
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
