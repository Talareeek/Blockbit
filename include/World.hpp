#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
#include "Entity.hpp"
#include "PerlinNoise.hpp"
#include "WorldOutputStream.hpp"
#include "ChunkMesh.hpp"

#include <unordered_map>
#include <cstdlib>
#include <filesystem>
#include <optional>

#include <SFML/Graphics.hpp>

class World
{
private:

    std::filesystem::path path;

    std::string name = "world";

    unsigned int seed;

    std::unordered_map<int, Chunk> chunks;
    std::vector<Entity> entities;

    PerlinNoise perlin{0};

    uint32_t version;

    static constexpr sf::Color nightTop   {6, 10, 28};
    static constexpr sf::Color nightBot   {18, 28, 60};
    static constexpr sf::Color dawnTop    {60, 90, 160};
    static constexpr sf::Color dawnBot    {220, 150, 150};
    static constexpr sf::Color dayTop     {80, 160, 240};
    static constexpr sf::Color dayBot     {170, 215, 255};
    static constexpr sf::Color sunsetTop  {120, 90, 160};
    static constexpr sf::Color sunsetBot  {255, 160, 90};
    
    sf::Vector2f spawnPoint{0.0f, 0.0f};

public:

    World() : version(0) {}
    World(unsigned int seed) : perlin(seed), version(0), seed(seed) {}

    World(const std::filesystem::path path);
    World(const std::string name, const std::filesystem::path path, unsigned int seed);

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

    void generateChunk(int chunk_position);

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

    std::pair<sf::Color, sf::Color> getSkyGradient(float t);

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

    void rebuildChunkMesh(int chunk_position, unsigned int unit_size);

    std::unordered_map<int, ChunkMesh> chunkMeshes;

    bool trackBlockChanges = false;
    std::vector<std::tuple<int, int, Block>> pendingBlockUpdates;

    float dayTime{0.0f};

    uint64_t days{0};
};

extern sf::Vector2i getMouseBlockPosition(const World& world, const sf::RenderWindow& window);

extern sf::Vector2f getMouseWorldPosition(const World& world, const sf::RenderWindow& window);

extern sf::Vector2f getSunWorldPosition(const World& world, sf::Vector2f cameraCenter);

extern void renderSunAndMoon(float daytime, sf::RenderWindow& window);

extern void renderSky(sf::RenderWindow& window, sf::Color top, sf::Color bottom);

extern void updateFluids(World& world);

#endif // WORLD_HPP