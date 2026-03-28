#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
#include "Entity.hpp"
#include "PerlinNoise.hpp"
#include "WorldOutputStream.hpp"

#include <unordered_map>
#include <cstdlib>

#include <SFML/Graphics.hpp>

class World
{
private:

    std::unordered_map<int, Chunk> chunks;
    std::vector<Entity> entities;

    PerlinNoise perlin{0};

    uint32_t version;

    float dayTime{0.0f};

    uint64_t days{0};

    sf::Color night   {12, 18, 40};
    sf::Color dawn    {90, 120, 180};
    sf::Color day     {120, 190, 255};
    sf::Color sunset  {255, 170, 120};  
    
    sf::Vector2f spawnPoint{0.0f, 0.0f};

public:

    World() : version(0) {}
    World(unsigned int seed) : perlin(seed), version(0) {}

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

    // GENERATION

    // flat world
    void generateFlatWorld();
    void generateFlatChunk(int chunk_position);

    // normal world
    void generateWorld();

    void generateChunk(int chunk_position);

    // phases
    void generateTerrain(int chunk_position);
    void generateCaves(int chunk_position);

    std::vector<Entity>& getEntities();

    std::vector<Entity> getEntities() const;

    uint32_t getVersion() const;

    float getHeightNoise(float x) const;
    int getHeight(int worldX) const;

    void tick(float dt);

    friend class WorldOutputStream;

    static constexpr float DAY_CYCLE_DURATION = 1200.0f; // 20 MINUTES

    static constexpr int SEA_LEVEL = 75;

    static constexpr float FLUID_TICK = 0.5f;

    float fluidTimer{0.0f};

    sf::Color getSkyColor(float t);

    float getDayTime() const { return dayTime; }

    sf::Vector2f getSpawnPoint() const { return spawnPoint; }

};

extern void RenderWorld(World& world, sf::RenderWindow& window);

extern void RenderBlockOverlay(World& world, sf::RenderWindow& window);

extern sf::Vector2i getMouseBlockPosition(const World& world, const sf::RenderWindow& window);

extern sf::Vector2f getMouseWorldPosition(const World& world, const sf::RenderWindow& window);

extern sf::Color lerpColor(sf::Color a, sf::Color b, float t);

extern void updateFluids(World& world);

#endif // WORLD_HPP