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

    //POLISH YAPPING
    sf::Color night   {12, 18, 40};   // ciemny granat
    sf::Color dawn    {90, 120, 180}; // chłodny niebiesko-fioletowy
    sf::Color day     {120, 190, 255}; // jasne niebo
    sf::Color sunset  {255, 170, 120}; // lekki pomarańcz

public:

    World() : version(0) {}
    World(unsigned int seed) : perlin(seed), version(0) {}
    
    Chunk& getChunk(int chunk_position);
    BlockID getBlock(int wx, int wy);
    void setBlock(int wx, int wy, BlockID block_id);

    // Generation
    void generateFlatWorld();
    void generateFlatChunk(int chunk_position);

    void generateWorld();
    void generateChunk(int chunk_position);

    std::vector<Entity>& getEntities();

    std::vector<Entity> getEntities() const;

    uint32_t getVersion() const;

    float getHeightNoise(float x) const;
    int getHeight(int worldX) const;

    void tick(float dt);

    friend class WorldOutputStream;

    static constexpr float DAY_CYCLE_DURATION = 1200.0f; // 20 MINUTES

    sf::Color getSkyColor(float t);

    float getDayTime() const { return dayTime; }

};

extern void RenderWorld(World& world, sf::RenderWindow& window);

extern void RenderBlockOverlay(World& world, sf::RenderWindow& window);

extern sf::Vector2i getMouseBlockPosition(const World& world, const sf::RenderWindow& window);

extern sf::Color lerpColor(sf::Color a, sf::Color b, float t);

#endif // WORLD_HPP