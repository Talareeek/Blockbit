#include "../include/World.hpp"
#include "../include/Block.hpp"
#include "../include/AssetManager.hpp"
#include "../include/RenderSystem.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/MainGameState.hpp"
#include <SFML/Graphics.hpp>

#include <iostream>

unsigned int World::getSeed() const
{
    return seed;
}

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


Block World::getBlock(int wx, int wy)
{
    if(wy < 0 || wy >= CHUNK_HEIGHT) return {BlockID::Air, 0};

    int chunk_position = (wx >= 0)
    ? wx / CHUNK_WIDTH
    : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    int local_x = wx - chunk_position * CHUNK_WIDTH;
    int local_y = wy;

    return getChunk(chunk_position).blocks[local_y][local_x];
}

void World::setBlock(int wx, int wy, Block block)
{
    if(wy < 0 || wy >= CHUNK_HEIGHT) return;

    int chunk_position = (wx >= 0)
    ? wx / CHUNK_WIDTH
    : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    int local_x = wx - chunk_position * CHUNK_WIDTH;
    int local_y = wy;

    getChunk(chunk_position).blocks[local_y][local_x] = block;
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
                chunk.blocks[y][x] = {BlockID::Bedrock, 0};
            else if (y < 5)
                chunk.blocks[y][x] = {BlockID::Stone, 0};
            else if (y == 5)
                chunk.blocks[y][x] = {BlockID::Dirt, 0};
            else if (y == 6)
                chunk.blocks[y][x] = {BlockID::Grass, 0};
            else
                chunk.blocks[y][x] = {BlockID::Air, 0};
        }
    }
}


/*
==================================================
WORLD GENERATION
==================================================
*/

void World::generateChunk(int chunk_position)
{
    Chunk& chunk = getChunk(chunk_position);

    generateTerrain(chunk_position);
    generateCaves(chunk_position);
    generateOres(chunk_position);
    generateNature(chunk_position);

    chunk.generated = true;
    chunk.dirty = true;
}


void World::generateTerrain(int chunk_position)
{
    Chunk& chunk = getChunk(chunk_position);

    for(int x = 0; x < CHUNK_WIDTH; x++)
    {
        int worldX = chunk_position * CHUNK_WIDTH + x;
        int height = getHeight(worldX);

        for(int y = 0; y < CHUNK_HEIGHT; y++)
        {
            if(y == 0)
                chunk.blocks[y][x] = {BlockID::Bedrock, 0};

            else if(y < height - 4)
                chunk.blocks[y][x] = {BlockID::Stone, 0};

            else if(y < height - 1)
                chunk.blocks[y][x] = {BlockID::Dirt, 0};

            else if(y == height - 1)
                if(y < SEA_LEVEL - 1) chunk.blocks[y][x] = {BlockID::Dirt, 0};
                else chunk.blocks[y][x] = {BlockID::Grass, 0};

            else if(y < SEA_LEVEL)
                chunk.blocks[y][x] = {BlockID::Water, static_cast<uint8_t>(WaterLevel::SOURCE)};

            else
                chunk.blocks[y][x] = {BlockID::Air, 0};
        }
    }
}

void World::generateCaves(int chunk_position)
{
    for(int x = 0; x < CHUNK_WIDTH; x++)
    {
        int worldX = chunk_position * CHUNK_WIDTH + x;

        for(int y = 5; y < CHUNK_HEIGHT; y++)
        {
            float n = perlin.noise(worldX * 0.05f, y * 0.05f);

            if(n > 0.65f)
                setBlock(worldX, y, {BlockID::Air, 0});
        }
    }
}

void World::generateVein(int x, int y, BlockID ore, int size)
{
    std::mt19937 rng(getSeed() ^ (x << 16) ^ y);

    for(int i=0; i<size; i++)
    {
        if(getBlock(x, y).id == BlockID::Stone)
            setBlock(x, y, {ore, 0});

        x += rng() % 3 - 1;
        y += rng() % 3 - 1; // suspicious
    }
}

void World::generateOres(int chunk_position)
{
    std::mt19937 rng(getSeed() + chunk_position);

    int diamond_height = rng() % 14 + 1; // DIAMONDS 0  -  15
    int gold_height = rng() % 34 + 16;   // GOLD     16 -  50
    int iron_height = rng() % 204 + 51;  // IRON     51 - 255

    int x = chunk_position * CHUNK_WIDTH;

    generateVein(x + rng() % CHUNK_WIDTH, diamond_height, BlockID::Diamond_Ore, 8);
    generateVein(x + rng() % CHUNK_WIDTH, gold_height, BlockID::Gold_Ore, 10);
    generateVein(x + rng() % CHUNK_WIDTH, iron_height, BlockID::Iron_Ore, 12);

}

void World::generateTree(int x, int y, int log_height, BlockID log_type, BlockID leaves_type)
{
    std::mt19937 rng(getSeed() ^ (x << 16) ^ y);

    for(int i = 0; i < log_height; i++)
    {
        setBlock(x, y + i, {log_type, 0});
    }

    int crown_y = y + log_height - 2;

    for(int dx = -2; dx <= 2; dx++)
    {
        for(int dy = -1; dy <= 1; dy++)
        {
            for(int dz = -2; dz <= 2; dz++)
            {
                if(abs(dx) + abs(dy) + abs(dz) <= 3)
                {
                    if(getBlock(x + dx, crown_y + dy).id == BlockID::Air)
                        setBlock(x + dx, crown_y + dy, {leaves_type, 0});
                }
            }
        }
    }
}

void World::generateNature(int chunk_position)
{
    std::mt19937 rng(getSeed() + chunk_position * 31);

    bool tree_spawns = rng() % 100 <= 35;

    if(tree_spawns)
    {
        int x = chunk_position * CHUNK_WIDTH + rng() % CHUNK_WIDTH;
        int y = 0;

        while(getBlock(x, y).id != BlockID::Grass && y < CHUNK_HEIGHT)
        {
            y++;
        }

        generateTree(x, y, rng() % 5 + 3, BlockID::Oak_Log, BlockID::Oak_Leaves);
    }
}

void World::generateWorld()
{
    for (int i = -100; i <= 100; ++i)
    {
        generateChunk(i);
    }

    for(int i = 60; i < 255; i++)
    {
        if(getBlock(0, i).id != BlockID::Air && getBlock(0, i + 1).id == BlockID::Air)
        {
            spawnPoint = {0.0f, static_cast<float>(i + 1)};
        }
    }
}

void RenderWorld(World& world, sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / MainGameState::UNIT_SIZE_FACTOR;

    int chunk = window.getView().getCenter().x / unit_size / CHUNK_WIDTH;

    for (int i = chunk - 2; i <= chunk + 2; ++i)
    {
        if (world.getChunk(i).generated == false) continue; //temoprary

        for (int y = 0; y < CHUNK_HEIGHT; ++y)
        {
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                Block block = world.getChunk(i).blocks[y][x];
                if (block.id == BlockID::Air) continue;

                sf::Texture& texture = AssetManager::getTexture(blockDatabase[block.id].texture);

                sf::Sprite sprite(texture);

                float heightFactor = 1.0f;

                if (block.id == BlockID::Water)
                {
                    uint8_t level = block.metadata;

                    // 9 = source → traktujemy jak 8
                    if(level == 9)
                        level = 8;

                    BlockID above = world.getBlock(i * CHUNK_WIDTH + x, y + 1).id;

                    if (above == BlockID::Water)
                    {
                        heightFactor = 1.0f;
                    }
                    else
                    {
                        heightFactor = level / 9.0f;
                    }

                    int texHeight = texture.getSize().y * heightFactor;

                    sprite.setTextureRect(sf::IntRect({
                        {0,
                        texture.getSize().y - texHeight},
                        {texture.getSize().x,
                        texHeight}
                    }));
                }

                sprite.setPosition({
                    (i * CHUNK_WIDTH + x) * static_cast<float>(unit_size),
                    (y + 1) * static_cast<float>(unit_size) - unit_size * (1.0f - heightFactor)
                });

                sprite.setScale({
                    (float)unit_size / sprite.getTextureRect().size.x,
                    -(float)(unit_size * heightFactor) / sprite.getTextureRect().size.y
                });
                window.draw(sprite);
            }
        }
    }
}

void RenderBlockOverlay(World& world, sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / MainGameState::UNIT_SIZE_FACTOR;

    sf::Vector2i blockPos = getMouseBlockPosition(world, window);
    int blockX = blockPos.x;
    int blockY = blockPos.y;

    if(world.getBlock(blockX, blockY).id != BlockID::Air)
    {
        sf::Sprite sprite(AssetManager::getTexture(8));
        sprite.setPosition({
            blockX * static_cast<float>(unit_size),
            blockY * static_cast<float>(unit_size)
        });

        sprite.setScale({
            (1 * static_cast<float>(unit_size)) / sprite.getTextureRect().size.x,
            -((1 * static_cast<float>(unit_size)) / sprite.getTextureRect().size.y)
        });

        sprite.setOrigin({
            0.f,
            static_cast<float>(sprite.getTextureRect().size.y)
        });

        window.draw(sprite);
    }
}

float World::getHeightNoise(float x) const
{
    float total = 0;
    float frequency = 0.03f;
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
    float baseHeight = 45.0f;
    float heightScale = 35.0f;

    return baseHeight + getHeightNoise((float)worldX) * heightScale;
}


std::vector<Entity>& World::getEntities()
{
    return entities;
}

void World::deleteEntity(uint32_t id)
{
    Entity& entity = entityWithID(id, *this);

    if(entity.getAssignedChunk())
    {
        chunks[entity.getChunk()].removeEntity(entity, *this);
    }

    entities.erase(std::remove_if(entities.begin(), entities.end(), [id](const Entity& e) { return e.getID() == id; }), entities.end());
}

void World::addEntity(Entity& entity)
{
    entities.push_back(entity);

    if(!entity.hasComponent<TransformComponent>()) return;

    entity.setAssignedChunk(true);
    int chunk_position = entity.getComponent<TransformComponent>().position.x / CHUNK_WIDTH;
    chunks[chunk_position].addEntity(entity);
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
    unsigned int unit_size = window.getSize().y / MainGameState::UNIT_SIZE_FACTOR;

    sf::Vector2f mouseWorldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    int blockX = static_cast<int>(std::floor(mouseWorldPos.x / static_cast<float>(unit_size)));
    int blockY = static_cast<int>(std::floor(mouseWorldPos.y / static_cast<float>(unit_size)));

    return {blockX, blockY};
}

sf::Vector2f getMouseWorldPosition(const World& world, const sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / MainGameState::UNIT_SIZE_FACTOR;

    sf::Vector2f position = {window.mapPixelToCoords(sf::Mouse::getPosition(window)).x / static_cast<float>(unit_size), window.mapPixelToCoords(sf::Mouse::getPosition(window)).y / static_cast<float>(unit_size)};

    return position;
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

    fluidTimer += dt;
    if(fluidTimer >= FLUID_TICK)
    {
        fluidTimer -= FLUID_TICK;
        updateFluids(*this);
    }
}


void updateFluids(World& world)
{
    int chunk = world.getEntities()[0].getComponent<TransformComponent>().position.x / (CHUNK_WIDTH);

    for (int i = chunk - 5; i <= chunk + 5; ++i)
    {
        if (world.getChunk(i).generated == false) continue;

        for (int y = 0; y < CHUNK_HEIGHT; ++y)
        {
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                Block block = world.getChunk(i).blocks[y][x];
                if (block.id == BlockID::Water)
                {
                    int worldX = i * CHUNK_WIDTH + x;
                    int worldY = y;

                    if (world.getBlock(worldX, worldY - 1).id == BlockID::Air || world.getBlock(worldX, worldY - 1).id == BlockID::Water) // TRY TO FLOW DOWNWARDS
                    {
                        world.setBlock(worldX, worldY - 1, {BlockID::Water, static_cast<uint8_t>(WaterLevel::FULL)});
                    }
                    else // TRY TO FLOW SIDEWAYS
                    {
                        if(block.metadata > 1)
                        {
                            if (world.getBlock(worldX - 1, worldY).id == BlockID::Air || (world.getBlock(worldX - 1, worldY).id == BlockID::Water && world.getBlock(worldX - 1, worldY).metadata < block.metadata))
                            {
                                world.setBlock(worldX - 1, worldY, {BlockID::Water, static_cast<uint8_t>((block.metadata < 9) ? block.metadata - 1 : 7)});
                            }
                            if (world.getBlock(worldX + 1, worldY).id == BlockID::Air || (world.getBlock(worldX + 1, worldY).id == BlockID::Water && world.getBlock(worldX + 1, worldY).metadata < block.metadata))
                            {
                                world.setBlock(worldX + 1, worldY, {BlockID::Water, static_cast<uint8_t>((block.metadata < 9) ? block.metadata - 1 : 7)});
                            }
                        }
                    }
                }
            }
        }
    }
}