#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
#include "Entity.hpp"
#include "PerlinNoise.hpp"

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

struct Climate
{
    float temperature;
    float humidity;
    float continentalness;
    float erosion;
    float weirdness;
};

class World
{
private:

    std::filesystem::path path;

    std::string name = "world";

    unsigned int seed;

    GenerationProperties generation_properties;

    PerlinNoise perlin{0};

    std::unordered_map<int, Chunk> chunks;
    std::vector<Entity> entities;


    uint32_t version;

    sf::Vector2f spawnPoint{0.0f, 0.0f};

public:

    World() : version(0) {}
    World(unsigned int seed, GenerationProperties generation_properties) : perlin(seed), version(0), seed(seed), generation_properties{generation_properties} {}

    World(const std::filesystem::path path);
    World(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties);

    void setSeed(unsigned int seed)
    {
        this->seed = seed;
        perlin = PerlinNoise(seed);
    }
    unsigned int getSeed() const;

    uint32_t getPossibleID()
    {
        for(uint32_t i = 1; i < UINT32_MAX; i++)
        {
            bool exists = false;
            for (const auto& entity : entities)
            {
                if (entity.getID() == i)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists) return i;
        }
        throw std::runtime_error("No available entity ID found");
    }

    Chunk& getChunk(int chunk_position);
    Block getBlock(int wx, int wy);
    void setBlock(int wx, int wy, Block block);

    std::unordered_map<int, Chunk>& getChunks();

    // GENERATION

    // flat world
    void generateFlatWorld();
    void generateFlatChunk(int chunk_position);

    // normal world
    void generateWorldSpawn();

    uint32_t spawnPlayer(uint32_t clientId);

    std::vector<uint32_t> getPlayerEntityIDs() const;

    std::optional<uint32_t> findPlayerEntityByClient(uint32_t clientId) const;

    Climate getClimate(int x);

    void generateChunk(int chunk_position);
    void loadOrCreateChunk(int chunk_position);

    // phases
    void generateTerrain(int chunk_position);
    void generateCaves(int chunk_position);
    void generateVein(int x, int y, BlockID ore, int size);
    void generateOres(int chunk_position);
    void generateTree(int x, int y, int log_height, BlockID log_type, BlockID leaves_type);
    void generateNature(int chunk_position);

    std::vector<Entity>& getEntities();

    const std::vector<Entity>& getEntities() const;

    uint32_t getVersion() const;

    float getContinentalNoise(float x) const;
    float getErosionNoise(float x) const;
    float getPeakNoise(float x) const;
    
    float getHeightNoise(float x) const;
    int getHeight(int worldX) const;

    void tick(float dt);

    friend class WorldOutputStream;

    static constexpr float DAY_CYCLE_DURATION = 1200.0f; // 20 MINUTES

    static constexpr int SEA_LEVEL = 75;

    static constexpr float FLUID_TICK = 0.5f;

    static constexpr int SIMULATION_DISTANCE = 10;

    static constexpr int MAX_CHUNKS_LOADED = 24;

    static constexpr int PREFFERED_CHUNKS_LOADED = 16;

    float fluidTimer{0.0f};

    float getDayTime() const { return dayTime; }

    sf::Vector2f getSpawnPoint() const { return spawnPoint; }

    const std::string& getName() const { return name; }

    void setName(const std::string& name) { this->name = name; }

    void writeManifest() const;
    void writeChunk(int chunk_position) const;
    void writeEntities() const;
    void writeData() const;

    void save() const;

    bool hasChunkFile(int chunk_position) const;


    void readManifest();
    void readChunk(int chunk_position);
    void readEntities();
    void readData();

    void load();

    std::pair<float, float> getSimulationRangeForEntity(const uint32_t entity);

    bool trackBlockChanges = false;
    std::vector<std::tuple<int, int, Block>> pendingBlockUpdates;

    float dayTime{0.0f};

    uint64_t days{0};
};

extern void updateFluids(World& world);

#endif // WORLD_HPP
