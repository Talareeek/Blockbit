#include "../include/World.hpp"
#include "../include/Block.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/PreserveComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/AIComponent.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/BBT.hpp"
#include <zstd.h>
#include <iostream>
#include <cmath>
#include <cstring>
#include <fstream>

World::World(const std::filesystem::path path) : path{path}
{
    load();
}

World::World(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties) : name(name), path(path), seed(seed), perlin(seed), generation_properties{generation_properties}
{
    generateWorldSpawn();
    save();
}

unsigned int World::getSeed() const
{
    return seed;
}

Chunk& World::getChunk(int chunk_position)
{
    // lazy generation of chunks
    if (chunks.find(chunk_position) == chunks.end())
    {
        Chunk c{};
        c.chunk_position = chunk_position;
        c.dirty = true;
        c.meshDirty = true;
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

    // Return Air if chunk doesn't exist
    if(!chunks.contains(chunk_position)) return {BlockID::Air, 0};

    int local_x = wx - chunk_position * CHUNK_WIDTH;
    int local_y = wy;

    return chunks[chunk_position].blocks[local_y][local_x];
}

Climate World::climateAt(int wx)
{
    int chunk_position = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
    int local_x = wx - chunk_position * CHUNK_WIDTH;

    return chunks[chunk_position].climates[local_x];
}

Biome World::biomeAt(int wx)
{
    int chunk_position = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
    int local_x = wx - chunk_position * CHUNK_WIDTH;

    return chunks[chunk_position].biomes[local_x];
}

void World::setBlock(int wx, int wy, Block block)
{
    if(wy < 0 || wy >= CHUNK_HEIGHT) return;

    int chunk_position = (wx >= 0)
    ? wx / CHUNK_WIDTH
    : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    int local_x = wx - chunk_position * CHUNK_WIDTH;
    int local_y = wy;

    // Only set block if chunk exists
    if(!chunks.contains(chunk_position)) return;

    Chunk& chunk = chunks[chunk_position];
    chunk.blocks[local_y][local_x] = block;
    chunk.dirty = true;
    chunk.meshDirty = true;
    chunk.generated = true;

    if (trackBlockChanges)
    {
        pendingBlockUpdates.emplace_back(wx, wy, block);
    }
}

std::unordered_map<int, Chunk>& World::getChunks()
{
    return chunks;
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
    chunk.meshDirty = true;
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

void World::loadOrCreateChunk(int chunk_position)
{
    if (chunks.contains(chunk_position) && chunks[chunk_position].generated) return;

    if (hasChunkFile(chunk_position)) readChunk(chunk_position);
    else generateChunk(chunk_position);
}

Climate World::getClimate(int x) const
{
    Climate climate;

    auto extend = [](float value)
    {
        return value * 2 - 1.0f;
    };

    climate.temperature = extend(perlin.noise(x * 0.008f, 1000));

    climate.humidity = extend(perlin.noise(x * 0.008f, 2000));

    climate.continentalness = extend(perlin.noise(x * 0.003f, 3000));

    climate.erosion = perlin.noise(x * 0.015f, 4000);

    climate.weirdness = perlin.noise(x * 0.020f, 5000);

    return climate;
}

Biome World::getBiome(int x) const
{
    auto c = getClimate(x);


    if(c.continentalness < -0.5)
        return Biome::Ocean;

    if(c.temperature < -0.4)
        return Biome::Snow;

    if(c.weirdness > 0.6)
        return Biome::Mountains;

    if(c.temperature > 0.4f)
    {
        if(c.humidity > 0) return Biome::Savanna;
        else return Biome::Desert;
    }

    return Biome::Plains;
}

void World::generateChunk(int chunk_position)
{
    Chunk& chunk = getChunk(chunk_position);

    if(generation_properties.flat)
    {
        generateFlatChunk(chunk_position);
    }
    else
    {
        generateTerrain(chunk_position);
        generateCaves(chunk_position);
        generateOres(chunk_position);
        generateNature(chunk_position);
    }

    chunk.generated = true;
    chunk.dirty = true;
    chunk.meshDirty = true;
}


void World::generateTerrain(int chunk_position)
{
    Chunk& chunk = getChunk(chunk_position);

    for(int x = 0; x < CHUNK_WIDTH; x++)
    {
        int worldX = chunk_position * CHUNK_WIDTH + x;
        int height = getHeight(worldX);

        Climate climate = getClimate(worldX);
        Biome biome = getBiome(worldX);

        chunk.climates[x] = climate;
        chunk.biomes[x] = biome;

        Block primary_block = surface_blocks[biome].first;
        Block secondary_block = surface_blocks[biome].second;

        for(int y = 0; y < CHUNK_HEIGHT; y++)
        {
            if(y == 0)
                chunk.blocks[y][x] = {BlockID::Bedrock, 0};

            else if(y < height - 4)
                chunk.blocks[y][x] = {BlockID::Stone, 0};

            else if(y < height - 1)
                chunk.blocks[y][x] = secondary_block; //{BlockID::Dirt, 0};

            else if(y == height - 1)
            {
                if(y < SEA_LEVEL - 1) chunk.blocks[y][x] = secondary_block; //{BlockID::Dirt, 0};
                else chunk.blocks[y][x] = primary_block; //{BlockID::Grass, 0};
            }

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
    uint32_t seed = getSeed() ^ (static_cast<uint32_t>(x) << 16) ^ static_cast<uint32_t>(y);
    std::mt19937 rng(seed);

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
    uint32_t seed = getSeed() ^ (static_cast<uint32_t>(x) << 16) ^ static_cast<uint32_t>(y);
    std::mt19937 rng(seed);

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
        int y = CHUNK_HEIGHT - 1;

        while(getBlock(x, y).id != BlockID::Grass && y > 0)
        {
            y--;
        }

        if(getBlock(x, y).id == BlockID::Grass)
            generateTree(x, y, rng() % 5 + 3, BlockID::Oak_Log, BlockID::Oak_Leaves);
    }
}

void World::generateWorldSpawn()
{
    for (int i = -2; i <= 2; ++i)
    {
        generateChunk(i);
    }
}

sf::Vector2<double> World::getSpawnPoint()
{
    loadOrCreateChunk(0);

    sf::Vector2<double> spawnPoint;

    for(int i = 60; i < 255; i++)
    {
        if(getBlock(0, i).id != BlockID::Air && getBlock(0, i + 1).id == BlockID::Air)
        {
            spawnPoint = {0.0f, static_cast<double>(i + 1)};
        }
    }

    return spawnPoint;
}

float World::getHeightNoise(float x) const
{
    float total = 0;

    float amplitude = generation_properties.amplitude;

    float frequency = generation_properties.frequency;

    for(int i = 0; i < 4; i++)
    {
        total += perlin.noise(x * frequency, 0.0f) * amplitude;

        amplitude *= generation_properties.persistence;
        frequency *= 2.0f;
    }

    return total;
}

int World::getHeight(int worldX) const
{
    Climate climate = getClimate(worldX);

    float x = static_cast<float>(worldX);

    float base = generation_properties.base_height + climate.continentalness * 0.30f;

    float scale = generation_properties.height_scale * (1.0f - climate.erosion * 0.35f);

    float terrain = getHeightNoise(static_cast<float>(worldX));

    float ridge = (1.0f - std::abs(perlin.noise(worldX * 0.003f, 9000) * 2.0f - 1.0f)) * climate.weirdness * 20.0f;

    return static_cast<int>(base + terrain * scale + ridge);
}


std::vector<Entity>& World::getEntities()
{
    return entities;
}

const std::vector<Entity>& World::getEntities() const
{
    return entities;
}

uint32_t World::getVersion() const
{
    return version;
}

void World::tick(float dt)
{
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

    struct Vec2iHash
    {
        std::size_t operator()(const sf::Vector2i& v) const noexcept
        {
            return std::hash<long long>()((static_cast<long long>(v.x) << 32) ^ static_cast<unsigned int>(v.y));
        }
    };
    std::unordered_map<sf::Vector2i, Block, Vec2iHash> pending_changes;

    auto pendingLevel = [&](int wx, int wy) -> int
    {
        auto it = pending_changes.find({wx, wy});
        if (it == pending_changes.end() || it->second.id != BlockID::Water) return -1;
        return static_cast<int>(it->second.metadata);
    };

    for (int i = chunk - World::SIMULATION_DISTANCE / 2; i <= chunk + World::SIMULATION_DISTANCE / 2; ++i)
    {
        if (world.getChunk(i).generated == false) continue;

        for (int y = 0; y < CHUNK_HEIGHT; ++y)
        {
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                Block block = world.getChunk(i).blocks[y][x];
                if (block.id != BlockID::Water) continue;

                int worldX = i * CHUNK_WIDTH + x;
                int worldY = y;

                // DECAY — non-source water disappears if nothing feeds it
                if (block.metadata != static_cast<uint8_t>(WaterLevel::SOURCE))
                {
                    bool fed = false;
                    if (world.getBlock(worldX, worldY + 1).id == BlockID::Water) fed = true;
                    else
                    {
                        Block left = world.getBlock(worldX - 1, worldY);
                        Block right = world.getBlock(worldX + 1, worldY);
                        if (left.id == BlockID::Water && left.metadata > block.metadata) fed = true;
                        else if (right.id == BlockID::Water && right.metadata > block.metadata) fed = true;
                    }
                    if (!fed)
                    {
                        pending_changes[{worldX, worldY}] = {BlockID::Air, 0};
                        continue;
                    }
                }

                Block below = world.getBlock(worldX, worldY - 1);

                // TRY TO FLOW DOWNWARDS — only into Air, never overwrite Water (preserves SOURCE/FULL)
                if (below.id == BlockID::Air)
                {
                    if (pendingLevel(worldX, worldY - 1) < static_cast<int>(WaterLevel::FULL))
                    {
                        pending_changes[{worldX, worldY - 1}] = {BlockID::Water, static_cast<uint8_t>(WaterLevel::FULL)};
                    }
                    continue;
                }

                // If water is directly below, no sideways spread (water already has somewhere to go / is settled)
                if (below.id == BlockID::Water) continue;

                // TRY TO FLOW SIDEWAYS — never into solid blocks
                if (block.metadata > 1)
                {
                    uint8_t newLevel = static_cast<uint8_t>((block.metadata < 9) ? block.metadata - 1 : 7);

                    auto tryFlow = [&](int tx, int ty)
                    {
                        Block target = world.getBlock(tx, ty);
                        if (target.id != BlockID::Air && target.id != BlockID::Water) return;
                        if (target.id == BlockID::Water && target.metadata >= newLevel) return;
                        if (pendingLevel(tx, ty) >= static_cast<int>(newLevel)) return;

                        pending_changes[{tx, ty}] = {BlockID::Water, newLevel};
                    };

                    tryFlow(worldX - 1, worldY);
                    tryFlow(worldX + 1, worldY);
                }
            }
        }
    }

    for(auto& a : pending_changes)
    {
        world.setBlock(a.first.x, a.first.y, a.second);
    }
}


void World::writeManifest() const
{
    std::ofstream file(path / "manifest");

    if(!file) throw std::runtime_error("Cannot open file");

    file << name << '\n';
    file << seed << '\n';
}

// "BBCK" little-endian: identifies the chunk file format we write today.
static constexpr uint32_t CHUNK_FILE_MAGIC = 0x4B434242u;

void World::writeChunk(int chunk_position) const
{
    auto it = chunks.find(chunk_position);
    if(it == chunks.end()) return;

    size_t block_size = sizeof(BlockID) + sizeof(uint8_t);
    size_t raw_size = CHUNK_WIDTH * CHUNK_HEIGHT * block_size + 16 * sizeof(Climate) + 16 * sizeof(Biome);

    std::vector<char> raw(raw_size);
    char* ptr = raw.data();

    for(int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for(int x = 0; x < CHUNK_WIDTH; x++)
        {
            const Block& block = it->second.blocks[y][x];

            std::memcpy(ptr, &block.id, sizeof(BlockID));
            ptr += sizeof(BlockID);

            std::memcpy(ptr, &block.metadata, sizeof(uint8_t));
            ptr += sizeof(uint8_t);
        }
    }

    for(int x = 0; x < 16; x++)
    {
        const Climate& climate = it->second.climates[x]; 
        const Biome& biome = it->second.biomes[x];

        std::memcpy(ptr, &climate, sizeof(Climate));
        ptr += sizeof(Climate);

        std::memcpy(ptr, &biome, sizeof(Biome));
        ptr += sizeof(Biome);
    }

    size_t bound = ZSTD_compressBound(raw_size);
    std::vector<char> compressed(bound);

    size_t compressed_size = ZSTD_compress(compressed.data(), bound, raw.data(), raw_size, 12);

    if(ZSTD_isError(compressed_size))
    {
        std::cerr << "ZSTD_compress failed for chunk " << chunk_position
                  << ": " << ZSTD_getErrorName(compressed_size) << '\n';
        return;
    }

    std::filesystem::path final_path = path / ("chunk_" + std::to_string(chunk_position));
    std::filesystem::path tmp_path = path / ("chunk_" + std::to_string(chunk_position) + ".tmp");

    {
        std::ofstream file(tmp_path, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            std::cerr << "Failed to open temp file for chunk " << chunk_position << '\n';
            return;
        }

        uint32_t magic = CHUNK_FILE_MAGIC;
        uint32_t raw_size32 = static_cast<uint32_t>(raw_size);
        uint32_t compressed_size32 = static_cast<uint32_t>(compressed_size);

        file.write(reinterpret_cast<const char*>(&magic), sizeof(uint32_t));
        file.write(reinterpret_cast<const char*>(&raw_size32), sizeof(uint32_t));
        file.write(reinterpret_cast<const char*>(&compressed_size32), sizeof(uint32_t));
        file.write(compressed.data(), compressed_size);
        file.flush();

        if(!file)
        {
            std::cerr << "Failed to write chunk " << chunk_position << '\n';
            std::error_code ec;
            std::filesystem::remove(tmp_path, ec);
            return;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmp_path, final_path, ec);
    if(ec)
    {
        std::cerr << "Failed to finalize chunk " << chunk_position << ": " << ec.message() << '\n';
        std::filesystem::remove(tmp_path, ec);
    }
}


void World::writeEntities() const
{
    std::ofstream file(path / "entities", std::ios::out);

    if(!file) throw std::runtime_error("Failed to open file for writing: " + path.string());


    for(const auto& entity : entities)
    {
        file << "Entity ID: " << entity.getID() << '\n';

        for(const auto& [type, component] : entity.getComponents())
        {
            file << "Component Type: ";

            if(type == typeid(PhysicsComponent))
            {
                file << "Physics" << '\n';
                file << std::any_cast<PhysicsComponent>(component).serialize();
            }
            else if(type == typeid(RenderComponent))
            {
                file << "Render" << '\n';
                file << std::any_cast<RenderComponent>(component).serialize();
            }
            else if(type == typeid(AnimationComponent))
            {
                file << "Animation" << '\n';
                file << std::any_cast<AnimationComponent>(component).serialize();
            }
            else if(type == typeid(InventoryComponent))
            {
                file << "Inventory" << '\n';
                file << std::any_cast<InventoryComponent>(component).serialize();
            }
            else if(type == typeid(HealthComponent))
            {
                file << "Health" << '\n';
                file << std::any_cast<HealthComponent>(component).serialize();
            }
            else if(type == typeid(ItemComponent))
            {
                file << "Item" << '\n';
                file << std::any_cast<ItemComponent>(component).serialize();
            }
            else if(type == typeid(PreserveComponent))
            {
                file << "Preserve" << '\n';
                file << (std::any_cast<PreserveComponent>(component) == PreserveComponent::Preserve ? "Preserve" : "Destroy") << '\n';
            }
            else if(type == typeid(ExplosiveComponent))
            {
                file << "Explosive" << '\n';
                file << std::any_cast<ExplosiveComponent>(component).serialize();
            }
            else if(type == typeid(TransformComponent))
            {
                file << "Transform" << '\n';
                file << std::any_cast<TransformComponent>(component).serialize();
            }
            else if(type == typeid(AnimationComponent))
            {
                file << "Animation" << '\n';
                file << std::any_cast<AnimationComponent>(component).serialize();
            }
            else if(type == typeid(AIComponent))
            {
                file << "AI" << "\n";
                file << std::any_cast<AIComponent>(component).serialize();
            }
            else if(type == typeid(PlayerControlledComponent))
            {
                file << "PlayerControlled" << '\n';
                file << std::any_cast<PlayerControlledComponent>(component).serialize();
            }
        }

        file << '\n';
    }
}

void World::save() const
{
    writeManifest();
    writeEntities();
    writeData();

    for(auto& [chunk_position, chunk] : chunks)
    {
        if(chunk.dirty)
        {
            writeChunk(chunk_position);
        }
    }
}

void World::writeData() const
{
    BBT data("data");

    data["day_time"] = Tag(getDayTime());
    data["days"] = Tag(days);

    data["generation_properties"] = TagCompound();
    data["generation_properties"]["flat"] = Tag(generation_properties.flat);
    data["generation_properties"]["base_height"] = Tag(generation_properties.base_height);
    data["generation_properties"]["height_scale"] = Tag(generation_properties.height_scale);
    data["generation_properties"]["frequency"] = Tag(generation_properties.frequency);
    data["generation_properties"]["amplitude"] = Tag(generation_properties.amplitude);
    data["generation_properties"]["persistence"] = Tag(generation_properties.persistence);

    auto buffer = data.save();

    std::ofstream file(path / "data", std::ios::out | std::ios::binary);
    
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}


bool World::hasChunkFile(int chunk_position) const
{
    return std::filesystem::exists(path / ("chunk_" + std::to_string(chunk_position)));
}


void World::readManifest()
{
    std::ifstream file(path / "manifest");

    if(!file) throw std::runtime_error("Cannot open file");

    file >> name;
    file >> seed;
    perlin = PerlinNoise(seed);
}

void World::readChunk(int chunk_position)
{
    std::filesystem::path chunk_path = path / ("chunk_" + std::to_string(chunk_position));

    size_t expected_raw_size = CHUNK_WIDTH * CHUNK_HEIGHT * (sizeof(BlockID) + sizeof(uint8_t)) + CHUNK_WIDTH * (sizeof(Climate) + sizeof(Biome));

    auto discardCorrupt = [&](const std::string& reason)
    {
        std::cerr << "Discarding corrupt chunk file " << chunk_path
                  << " (" << reason << "); will regenerate\n";
        std::error_code ec;
        std::filesystem::remove(chunk_path, ec);
    };

    std::ifstream file(chunk_path, std::ios::binary);
    if(!file) return;

    uint32_t first_word = 0;
    file.read(reinterpret_cast<char*>(&first_word), sizeof(uint32_t));
    if(file.gcount() != static_cast<std::streamsize>(sizeof(uint32_t)))
    {
        discardCorrupt("header truncated");
        return;
    }

    uint32_t raw_size = 0;
    std::vector<char> compressed;

    if(first_word == CHUNK_FILE_MAGIC)
    {
        uint32_t compressed_size = 0;
        file.read(reinterpret_cast<char*>(&raw_size), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&compressed_size), sizeof(uint32_t));
        if(!file)
        {
            discardCorrupt("header incomplete");
            return;
        }
        if(raw_size != expected_raw_size)
        {
            discardCorrupt("raw_size mismatch");
            return;
        }

        compressed.resize(compressed_size);
        if(compressed_size > 0)
        {
            file.read(compressed.data(), compressed_size);
            if(file.gcount() != static_cast<std::streamsize>(compressed_size))
            {
                discardCorrupt("compressed payload truncated");
                return;
            }
        }
    }
    else
    {
        // Legacy format: [raw_size uint32][compressed data to EOF]
        raw_size = first_word;
        if(raw_size != expected_raw_size)
        {
            discardCorrupt("legacy raw_size mismatch");
            return;
        }
        compressed.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    if(compressed.empty())
    {
        discardCorrupt("no compressed data");
        return;
    }

    std::vector<char> raw(raw_size);
    size_t result = ZSTD_decompress(raw.data(), raw_size, compressed.data(), compressed.size());

    if(ZSTD_isError(result))
    {
        discardCorrupt(std::string("ZSTD_decompress: ") + ZSTD_getErrorName(result));
        return;
    }

    if(result != raw_size)
    {
        discardCorrupt("decompressed size mismatch");
        return;
    }

    Chunk& chunk = chunks[chunk_position];
    chunk.chunk_position = chunk_position;
    chunk.generated = true;
    chunk.dirty = false;
    chunk.meshDirty = true;

    const char* ptr = raw.data();

    for(int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for(int x = 0; x < CHUNK_WIDTH; x++)
        {
            Block& block = chunk.blocks[y][x];

            std::memcpy(&block.id, ptr, sizeof(BlockID));
            ptr += sizeof(BlockID);

            std::memcpy(&block.metadata, ptr, sizeof(uint8_t));
            ptr += sizeof(uint8_t);
        }
    }

    for(int x = 0; x < CHUNK_WIDTH; x++)
    {
        Climate& climate = chunk.climates[x];
        Biome& biome = chunk.biomes[x];

        std::memcpy(&climate, ptr, sizeof(Climate));
        ptr += sizeof(Climate);

        std::memcpy(&biome, ptr, sizeof(Biome));
        ptr += sizeof(Biome);

    }
}

void World::readEntities()
{
    std::cout << "Reading entities from: " << (path / "entities") << '\n';

    std::ifstream file(path / "entities");
    if (!file)
        throw std::runtime_error("Failed to open file for reading: " + path.string());

    std::string line;
    std::string bufferedLine;
    bool hasBuffered = false;

    auto getLine = [&](std::string& out) -> bool
    {
        if (hasBuffered)
        {
            out = bufferedLine;
            hasBuffered = false;
            return true;
        }
        return static_cast<bool>(std::getline(file, out));
    };

    auto pushBackLine = [&](const std::string& l)
    {
        bufferedLine = l;
        hasBuffered = true;
    };

    while (getLine(line))
    {
        if (line.empty())
            continue;

        if (line.rfind("Entity ID:", 0) == 0)
        {
            uint32_t entityID = std::stoul(line.substr(11));
            Entity entity(entityID);

            std::cout << "\tEntity ID: " << entityID << '\n';

            while (getLine(line))
            {
                if (line.empty())
                    continue;

                if (line.rfind("Entity ID:", 0) == 0)
                {
                    pushBackLine(line);
                    break;
                }

                if (line.rfind("Component Type:", 0) == 0)
                {
                    std::string componentType = line.substr(16);
                    std::string componentData;

                    while (getLine(line))
                    {
                        if (line.empty())
                            break;

                        if (line.rfind("Component Type:", 0) == 0 ||
                            line.rfind("Entity ID:", 0) == 0)
                        {
                            pushBackLine(line);
                            break;
                        }

                        componentData += line + '\n';
                    }

                    if (componentType == "Physics")
                    {
                        PhysicsComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<PhysicsComponent>(c);
                        std::cout << "\t\tPhysicsComponent loaded\n";
                    }
                    else if (componentType == "Render")
                    {
                        RenderComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<RenderComponent>(c);
                        std::cout << "\t\tRenderComponent loaded\n";
                    }
                    else if (componentType == "Animation")
                    {
                        AnimationComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<AnimationComponent>(c);
                        std::cout << "\t\tAnimationComponent loaded\n";
                    }
                    else if (componentType == "Inventory")
                    {
                        InventoryComponent c(1);
                        c.deserialize(componentData);
                        entity.addComponent<InventoryComponent>(c);
                        std::cout << "\t\tInventoryComponent loaded\n";
                    }
                    else if (componentType == "Health")
                    {
                        HealthComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<HealthComponent>(c);
                        std::cout << "\t\tHealthComponent loaded\n";
                    }
                    else if (componentType == "Item")
                    {
                        ItemComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<ItemComponent>(c);
                        std::cout << "\t\tItemComponent loaded\n";
                    }
                    else if (componentType == "Preserve")
                    {
                        PreserveComponent preserve;
                        preserve = (componentData.find("Preserve") != std::string::npos)
                                   ? PreserveComponent::Preserve
                                   : PreserveComponent::Destroy;

                        entity.addComponent<PreserveComponent>(preserve);
                        std::cout << "\t\tPreserveComponent loaded\n";
                    }
                    else if (componentType == "Explosive")
                    {
                        ExplosiveComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<ExplosiveComponent>(c);
                        std::cout << "\t\tExplosiveComponent loaded\n";
                    }
                    else if (componentType == "Transform")
                    {
                        TransformComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<TransformComponent>(c);
                        std::cout << "\t\tTransformComponent loaded\n";
                    }
                    else if (componentType == "Animation")
                    {
                        AnimationComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<AnimationComponent>(c);
                        std::cout << "\t\tAnimationComponent loaded\n";
                    }
                    else if (componentType == "AI")
                    {
                        AIComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<AIComponent>(c);
                        std::cout << "\t\tAIComponent loaded\n";
                    }
                    else if (componentType == "PlayerControlled")
                    {
                        PlayerControlledComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<PlayerControlledComponent>(c);
                        std::cout << "\t\tPlayerControlledComponent loaded\n";
                    }
                }
            }

            entities.push_back(entity);
            std::cout << "\tEntity loaded succesfully\n";
        }
    }

    std::cout << "Entities loaded succesfully\n";
    std::cout << "Total entities: " << entities.size() << '\n' << '\n';
}

void World::readData()
{
    std::ifstream file(path / "data", std::ios::ate | std::ios::binary);

    if(!file) throw std::runtime_error("Failed to open data file");

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    file.read(reinterpret_cast<char*>(buffer.data()), size);

    file.close();


    BBT data = BBT::load(buffer);

    dayTime = data["day_time"].get<float>();
    days = data["days"].get<uint64_t>();
    
    generation_properties =
    {
        .flat = data["generation_properties"]["flat"].get<bool>(),
        .base_height = data["generation_properties"]["base_height"].get<float>(),
        .height_scale = data["generation_properties"]["height_scale"].get<float>(),
        .frequency = data["generation_properties"]["frequency"].get<float>(),
        .amplitude = data["generation_properties"]["amplitude"].get<float>(),
        .persistence = data["generation_properties"]["persistence"].get<float>()
    };


    /*
    std::string trash;

    file >> trash >> dayTime;

    file >> trash >> days;

    file >> trash >> spawnPoint.x >> spawnPoint.y;

    file >> trash >> generation_properties.flat;
    file >> trash >> generation_properties.base_height;
    file >> trash >> generation_properties.height_scale;
    file >> trash >> generation_properties.frequency;
    file >> trash >> generation_properties.amplitude;
    file >> trash >> generation_properties.persistence;
    */

    std::cout << "Data file: " << std::endl;
    std::cout << "Daytime: " << dayTime << std::endl;
    std::cout << "Days: " << days << std::endl;

    std::cout << "Generation Properties:\n";
    std::cout << "\tFlat: " << ((generation_properties.flat) ? "true" : "false") << '\n';
    std::cout << "\tBase height: " << generation_properties.base_height << '\n';
    std::cout << "\tHeight scale: " << generation_properties.height_scale << '\n';
    std::cout << "\tFrequency: " << generation_properties.frequency << '\n';
    std::cout << "\tAmplitude: " << generation_properties.amplitude << '\n';
    std::cout << "\tPersistence: " << generation_properties.persistence << '\n';
}


void World::load()
{
    // Ensure world directory exists
    if(!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }

    // Manifest
    if(std::filesystem::exists(path / "manifest")) {
        try {
            readManifest();
        } catch(const std::exception& e) {
            std::cerr << "Warning: Failed to read manifest: " << e.what() << '\n';
            name = path.filename().string();
            seed = static_cast<unsigned int>(std::rand());
            perlin = PerlinNoise(seed);
        }
    } else {
        // No manifest -> initialize new world and persist
        name = path.filename().string();
        seed = static_cast<unsigned int>(std::rand());
        perlin = PerlinNoise(seed);
        generateWorldSpawn();
        try { save(); } catch(const std::exception& e) { std::cerr << "Warning: Failed to save new world: " << e.what() << '\n'; }
        return;
    }

    // Entities
    if(std::filesystem::exists(path / "entities")) {
        try {
            readEntities();
        } catch(const std::exception& e) {
            std::cerr << "Warning: Failed to read entities: " << e.what() << '\n';
            entities.clear();
        }
    }

    // Data
    if(std::filesystem::exists(path / "data")) {
        try {
            readData();
        } catch(const std::exception& e) {
            std::cerr << "Warning: Failed to read data: " << e.what() << '\n';
            dayTime = 0.0f;
            days = 0;
        }
    } else {
        // No data file -> write defaults
        try { writeData(); } catch(const std::exception& e) { std::cerr << "Warning: Failed to write data: " << e.what() << '\n'; }
    }

    // Persist any missing files
    try { save(); } catch(const std::exception& e) { std::cerr << "Warning: Failed to save world: " << e.what() << '\n'; }
}

uint32_t World::spawnPlayer(uint32_t clientId)
{
    uint32_t id = getPossibleID();

    Entity player(id);

    player.addComponent(TransformComponent{{0.0f, 0.0f}, {1.0f, 1.0f}, sf::degrees(0.0f)});
    player.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});

    InventoryComponent inv(36);
    inv.inventory.slots[0] = {ItemID::Dynamite, 16};
    inv.inventory.slots[1] = {ItemID::Bucket, 1};
    inv.inventory.slots[2] = {ItemID::Woodcutter, 64};
    inv.inventory.slots[3] = {ItemID::Lighter, 1};
    player.addComponent(std::move(inv));

    sf::Vector2<double> spawn = getSpawnPoint();
    int spawnY = static_cast<int>(spawn.y);
    if (spawnY == 0)
    {
        for (int i = 0; i < 255; i++)
        {
            if (getBlock(static_cast<int>(spawn.x), i).id == BlockID::Air)
            {
                spawnY = i + 1;
                break;
            }
        }
    }
    player.getComponent<TransformComponent>().position = {spawn.x, static_cast<double>(spawnY)};

    player.addComponent(RenderComponent{0, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});
    player.addComponent(HealthComponent{100, 100, false});
    player.addComponent(PlayerControlledComponent{""});

    entities.push_back(std::move(player));

    return id;
}

std::vector<uint32_t> World::getPlayerEntityIDs() const
{
    std::vector<uint32_t> ids;
    for (const auto& e : entities)
    {
        if (e.hasComponent<PlayerControlledComponent>())
            ids.push_back(e.getID());
    }
    return ids;
}

std::pair<float, float> World::getSimulationRangeForEntity(const uint32_t entity)
{
    auto& transform = entityWithID(entity, *this).getComponent<TransformComponent>();

    int entity_chunk = transform.position.x / CHUNK_WIDTH;

    return
    {
        static_cast<float>((entity_chunk - SIMULATION_DISTANCE) * CHUNK_WIDTH),
        static_cast<float>((entity_chunk + SIMULATION_DISTANCE) * CHUNK_WIDTH + CHUNK_WIDTH)
    };
}

