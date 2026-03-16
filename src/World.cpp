#include "../include/World.hpp"
#include "../include/Block.hpp"
#include "../include/AssetManager.hpp"
#include "../include/RenderSystem.hpp"
#include <SFML/Graphics.hpp>

Chunk& World::getChunk(int chunk_position)
{
    // lazy generation of chunks
    if (chunks.find(chunk_position) == chunks.end())
    {
        Chunk c;
        c.dirty = true;
        c.generated = false;
        chunks[chunk_position] = c;
    }
    return chunks[chunk_position];
}


BlockID World::getBlock(int wx, int wy)
{
    int chunk_position = (wx >= 0)
    ? wx / CHUNK_WIDTH
    : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    int local_x = wx - chunk_position * CHUNK_WIDTH;
    int local_y = wy;

    return getChunk(chunk_position).blocks[local_y][local_x];
}

void World::setBlock(int wx, int wy, BlockID block_id)
{
    int chunk_position = (wx >= 0)
    ? wx / CHUNK_WIDTH
    : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    int local_x = wx - chunk_position * CHUNK_WIDTH;
    int local_y = wy;

    getChunk(chunk_position).blocks[local_y][local_x] = block_id;
}

void World::generateFlatWorld()
{
    for (int i = -100; i <= 100; ++i)
    {
        generateFlatChunk(i);
    }
};

void World::generateFlatChunk(int chunk_position)
{
    Chunk& chunk = getChunk(chunk_position);
    chunk.dirty = true;
    chunk.generated = true;

    for (int y = 0; y < CHUNK_HEIGHT; ++y)
    {
        for (int x = 0; x < CHUNK_WIDTH; ++x)
        {
            if (y == 0)
                chunk.blocks[y][x] = BlockID::Bedrock;
            else if (y < 5)
                chunk.blocks[y][x] = BlockID::Stone;
            else if (y == 5)
                chunk.blocks[y][x] = BlockID::Dirt;
            else if (y == 6)
                chunk.blocks[y][x] = BlockID::Grass;
            else
                chunk.blocks[y][x] = BlockID::Air;
        }
    }
}

void World::generateChunk(int chunk_position)
{
    Chunk& chunk = getChunk(chunk_position);

    for(int x = 0; x < CHUNK_WIDTH; x++)
    {
        int worldX = chunk_position * CHUNK_WIDTH + x;
        int height = getHeight(worldX);

        for(int y = 0; y < CHUNK_HEIGHT; y++)
        {
            if(y == 0)
                chunk.blocks[y][x] = BlockID::Bedrock;
            else if(y < height - 4)
                chunk.blocks[y][x] = BlockID::Stone;
            else if(y < height - 1)
                chunk.blocks[y][x] = BlockID::Dirt;
            else if(y == height - 1)
                chunk.blocks[y][x] = BlockID::Grass;
            else
                chunk.blocks[y][x] = BlockID::Air;
        }
    }

    chunk.generated = true;
    chunk.dirty = true;
}

void World::generateWorld()
{
    for (int i = -100; i <= 100; ++i)
    {
        generateChunk(i);
    }
}

void RenderWorld(World& world, sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / 9;

    int chunk = window.getView().getCenter().x / unit_size / CHUNK_WIDTH;

    for (int i = chunk - 2; i <= chunk + 2; ++i)
    {
        if (world.getChunk(i).generated == false) continue; //temoprary

        for (int y = 0; y < CHUNK_HEIGHT; ++y)
        {
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                BlockID block_id = world.getChunk(i).blocks[y][x];
                if (block_id == BlockID::Air) continue;

                sf::Sprite sprite(AssetManager::getTexture(blockDatabase[block_id].texture));
                sprite.setPosition({
                    (i * CHUNK_WIDTH + x) * static_cast<float>(unit_size),
                    worldToScreenY((y), unit_size, window.getSize().y)
                });

                sprite.setScale({
                    (1 * static_cast<float>(unit_size)) / sprite.getTextureRect().size.x,
                    (1 * static_cast<float>(unit_size)) / sprite.getTextureRect().size.y
                });
                window.draw(sprite);
            }
        }
    }
}

void RenderBlockOverlay(World& world, sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / 9;
    /*
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    int blockX = static_cast<int>(std::floor(mousePos.x / unit_size));
    int blockY = screenToWorldY(mousePos.y, unit_size, window.getSize().y) + 1;
    */

    sf::Vector2i blockPos = getMouseBlockPosition(world, window);
    int blockX = blockPos.x;
    int blockY = blockPos.y;

    if(world.getBlock(blockX, blockY) != BlockID::Air)
    {
        sf::Sprite sprite(AssetManager::getTexture(8));
        sprite.setPosition({
            blockX * static_cast<float>(unit_size),
            worldToScreenY(blockY, unit_size, window.getSize().y)
        });

        sprite.setScale({
            (1 * static_cast<float>(unit_size)) / sprite.getTextureRect().size.x,
            (1 * static_cast<float>(unit_size)) / sprite.getTextureRect().size.y
        });
        window.draw(sprite);
    }
}

float World::getHeightNoise(float x) const
{
    float total = 0;
    float frequency = 0.01f;
    float amplitude = 1.0f;
    float persistence = 0.5f;

    for(int i = 0; i < 4; i++)
    {
        total += perlin.noise(x * frequency, 0.0f) * amplitude;

        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total;
}

int World::getHeight(int worldX) const
{
    float baseHeight = 60.0f;
    float heightScale = 50.0f;

    return baseHeight + getHeightNoise((float)worldX) * heightScale;
}


std::vector<Entity>& World::getEntities()
{
    return entities;
}

std::vector<Entity> World::getEntities() const
{
    return entities;
}

uint32_t World::getVersion() const
{
    return version;
}

sf::Vector2i getMouseBlockPosition(const World& world, const sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / 9;
    sf::Vector2f mouseWorldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    int blockX = static_cast<int>(std::floor(mouseWorldPos.x / unit_size));
    int blockY = static_cast<int>(std::ceil(screenToWorldY(mouseWorldPos.y, unit_size, window.getSize().y)));

    return {blockX, blockY};
}

sf::Color lerpColor(sf::Color a, sf::Color b, float t)
{
    return sf::Color
    (
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t
    );
}

float smooth(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

sf::Color World::getSkyColor(float t)
{
    t = std::fmod(t, 1.0f);

    if (t < 0.25f)
        return lerpColor(night, dawn, smooth(t / 0.25f));

    if (t < 0.5f)
        return lerpColor(dawn, day, smooth((t - 0.25f) / 0.25f));

    if (t < 0.75f)
        return lerpColor(day, sunset, smooth((t - 0.5f) / 0.25f));

    return lerpColor(sunset, night, smooth((t - 0.75f) / 0.25f));
}

void World::tick(float dt)
{
    dayTime += dt;
    if(dayTime >= DAY_CYCLE_DURATION)
    {
        dayTime -= DAY_CYCLE_DURATION;
        days++;
    }
}