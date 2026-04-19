#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
#include "Entity.hpp"
#include "PerlinNoise.hpp"
#include "WorldOutputStream.hpp"

#include <unordered_map>
#include <cstdlib>
#include <filesystem>

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

    float dayTime{0.0f};

    uint64_t days{0};

    static constexpr sf::Color night   {12, 18, 40};
    static constexpr sf::Color dawn    {90, 120, 180};
    static constexpr sf::Color day     {120, 190, 255};
    static constexpr sf::Color sunset  {255, 170, 120};  
    
    sf::Vector2f spawnPoint{0.0f, 0.0f};

    uint32_t playerID{0};

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

    void createPlayer();

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

    float fluidTimer{0.0f};

    sf::Color getSkyColor(float t);

    float getDayTime() const { return dayTime; }

    sf::Vector2f getSpawnPoint() const { return spawnPoint; }

    const std::string& getName() const { return name; }

    void setName(const std::string& name) { this->name = name; }

    uint32_t getPlayerID() const { return playerID; }
    void setPlayerID(uint32_t id) { playerID = id; }

    
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

};

extern void RenderWorld(World& world, sf::RenderWindow& window);

extern void RenderBlockOverlay(World& world, sf::RenderWindow& window);

extern sf::Vector2i getMouseBlockPosition(const World& world, const sf::RenderWindow& window);

extern sf::Vector2f getMouseWorldPosition(const World& world, const sf::RenderWindow& window);

extern sf::Color lerpColor(sf::Color a, sf::Color b, float t);

extern void updateFluids(World& world);

#endif // WORLD_HPP