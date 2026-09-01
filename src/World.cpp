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
#include <any>

World::World(const std::filesystem::path path) : path{path}
{
    load();
}

World::World(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties) : name(name), path(path), seed(seed), perlin(seed), generation_properties{generation_properties}
{
    generateWorldSpawn();
    save();
}

Chunk& World::getChunk(int chunk_position)
{
    if (chunks.find(chunk_position) == chunks.end())
    {
        loadOrCreateChunk(chunk_position);

        /*
        Chunk chunk{};
        chunk.chunk_position = chunk_position;
        chunk.dirty = true;
        chunk.meshDirty = true;
        chunk.generated = false;
        */
    }
    return chunks[chunk_position];
}


Block World::getBlock(int world_x, int world_y)
{
    if(world_y < 0 || world_y >= CHUNK_HEIGHT) return {BlockID::Air, 0};

    int chunk_position = (world_x >= 0) ? world_x / CHUNK_WIDTH : (world_x - CHUNK_WIDTH + 1) / CHUNK_WIDTH;

    if(!chunks.contains(chunk_position)) return {BlockID::Air, 0};

    int local_x = world_x - chunk_position * CHUNK_WIDTH;
    int local_y = world_y;

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
    Chunk& chunk = chunks[chunk_position];
    chunk.chunk_position = chunk_position;

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

    if (hasChunkFile(chunk_position)) loadChunk(chunk_position);
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
    Chunk& chunk = chunks[chunk_position];
    chunk.chunk_position = chunk_position;

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
    Chunk& chunk = chunks[chunk_position];
    chunk.chunk_position = chunk_position;

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
    uint32_t seed = this->seed ^ (static_cast<uint32_t>(x) << 16) ^ static_cast<uint32_t>(y);
    std::mt19937 rng(seed);

    for(int i=0; i<size; i++)
    {
        if(getBlock(x, y).id == BlockID::Stone)
            setBlock(x, y, {ore, 0});

        x += rng() % 3 - 1;
        y += rng() % 3 - 1;
    }
}

void World::generateOres(int chunk_position)
{
    std::mt19937 rng(seed + chunk_position);

    int ruby_height = rng() % 14 + 1;
    int diamond_height = rng() % 14 + 1; // DIAMONDS 0  -  15
    int gold_height = rng() % 34 + 16;   // GOLD     16 -  50
    int iron_height = rng() % 204 + 51;  // IRON     51 - 255

    int x = chunk_position * CHUNK_WIDTH;

    generateVein(x + rng() % CHUNK_WIDTH, diamond_height, BlockID::Diamond_Ore, 8);
    generateVein(x + rng() % CHUNK_WIDTH, gold_height, BlockID::Gold_Ore, 10);
    generateVein(x + rng() % CHUNK_WIDTH, iron_height, BlockID::Iron_Ore, 12);
    generateVein(x + rng() % CHUNK_WIDTH, ruby_height, BlockID::Ruby_Ore, 6);
}

void World::generateTree(int x, int y, int log_height, BlockID log_type, BlockID leaves_type)
{
    uint32_t seed = this->seed ^ (static_cast<uint32_t>(x) << 16) ^ static_cast<uint32_t>(y);
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
    std::mt19937 rng(seed + chunk_position * 31);

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


std::unordered_map<UUID, Entity>& World::getEntities()
{
    return entities;
}

const std::unordered_map<UUID, Entity>& World::getEntities() const
{
    return entities;
}

void World::addEntity(Entity entity)
{
    UUID id = entity.getID();

    if(!entity.hasComponent<TransformComponent>())
    {
        Chunk& chunk = getChunk(0);
        chunk.entity_ids.insert(id);
        chunk.dirty = true;        
    }
    else
    {
        Chunk& chunk = getChunk(entity.getComponent<TransformComponent>().chunkPosition());
        entity.getComponent<TransformComponent>().previous_position = entity.getComponent<TransformComponent>().position;
        chunk.entity_ids.insert(id);
        chunk.dirty = true;  
    }    

    entities.emplace(id, std::move(entity));
}

void World::removeEntity(UUID id)
{
    auto it = entities.find(id);
    if (it == entities.end()) return;

    if (it->second.hasComponent<TransformComponent>())
    {
        auto& transform = it->second.getComponent<TransformComponent>();
        int chunk_position = transform.chunkPosition();

        auto chunkIt = chunks.find(chunk_position);
        if (chunkIt != chunks.end())
        {
            chunkIt->second.entity_ids.erase(id);
            chunkIt->second.dirty = true;
        }
    }

    entities.erase(it);
}

Entity& World::getEntity(UUID id)
{
    if(entities.contains(id)) return getEntities().at(id);

    throw std::runtime_error("No entity found");
}

bool World::doesEntityExist(UUID id)
{
    return entities.contains(id);
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
    /*
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
    */
}




// "BBCK" little-endian: identifies the chunk file format we write today.
static constexpr uint32_t CHUNK_FILE_MAGIC = 0x4B434242u;


bool World::hasChunkFile(int chunk_position) const
{
    return std::filesystem::exists(path / ("chunk_" + std::to_string(chunk_position)));
}

UUID World::spawnPlayer(uint32_t clientId)
{
    Entity player(generateUUID());

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

    player.addComponent(RenderComponent{AssetManager::GameTextureID::Player, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});
    player.addComponent(HealthComponent{100, 100, false});
    player.addComponent(PlayerControlledComponent{""});

    entities.emplace(player.getID(), std::move(player));

    return player.getID();
}

std::vector<UUID> World::getPlayerEntityIDs() const
{
    std::vector<UUID> ids;
    for (const auto& [id, entity] : entities)
    {
        if (entity.hasComponent<PlayerControlledComponent>())
            ids.push_back(entity.getID());
    }
    return ids;
}

std::pair<double, double> World::getSimulationRangeForEntity(const UUID entity)
{
    auto& transform = getEntity(entity).getComponent<TransformComponent>();

    int entity_chunk = transform.position.x / CHUNK_WIDTH;

    return
    {
        static_cast<float>((entity_chunk - SIMULATION_DISTANCE) * CHUNK_WIDTH),
        static_cast<float>((entity_chunk + SIMULATION_DISTANCE) * CHUNK_WIDTH + CHUNK_WIDTH)
    };
}

                                                             


void World::save()
{
    saveManifest();
    saveData();

    for(auto& [chunk_position, chunk] : chunks)
    {
        if(chunk.dirty)
        {
            saveChunk(chunk_position);
        }
    }
}

void World::load()
{
    if(!std::filesystem::exists(path)) throw std::runtime_error("World path does not exist");

    loadManifest();
    loadData();
}


void World::saveManifest()
{
    BBT root("manifest");

    root["name"] = Tag(name);


    std::ofstream file(path / "manifest", std::ios::binary);

    if(!file) throw std::runtime_error("Cannot open manifest file");


    std::vector<uint8_t> buffer = root.save();

    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());


    file.close();
}

void World::loadManifest()
{
    std::ifstream stream(path / "manifest", std::ios::ate | std::ios::binary);

    if(!stream) throw std::runtime_error("Failed to open manifest file");

    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    stream.close();


    BBT root = BBT::load(buffer);

    name = root["name"].get<std::string>();
}


void World::saveData()
{
    BBT data("data");

    data["generation_properties"] = TagCompound();
    data["generation_properties"]["flat"] = Tag(generation_properties.flat);
    data["generation_properties"]["base_height"] = Tag(generation_properties.base_height);
    data["generation_properties"]["height_scale"] = Tag(generation_properties.height_scale);
    data["generation_properties"]["frequency"] = Tag(generation_properties.frequency);
    data["generation_properties"]["amplitude"] = Tag(generation_properties.amplitude);
    data["generation_properties"]["persistence"] = Tag(generation_properties.persistence);

    data["seed"] = Tag(seed);

    data["day_time"] = Tag(getDayTime());
    data["days"] = Tag(days);

    auto buffer = data.save();

    std::ofstream file(path / "data", std::ios::out | std::ios::binary);
    
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

void World::loadData()
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
}


void World::saveChunk(int chunk_position)
{
    saveChunkEnvironment(chunk_position);
    saveChunkEntities(chunk_position);
}

void World::saveChunkEnvironment(int chunk_position)
{
    auto iterator = chunks.find(chunk_position);
    if(iterator == chunks.end()) return;

    size_t block_size = sizeof(BlockID) + sizeof(uint8_t);
    size_t raw_size = CHUNK_WIDTH * CHUNK_HEIGHT * block_size + 16 * sizeof(Climate) + 16 * sizeof(Biome);

    std::vector<char> raw(raw_size);
    char* ptr = raw.data();

    for(int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for(int x = 0; x < CHUNK_WIDTH; x++)
        {
            const Block& block = iterator->second.blocks[y][x];

            std::memcpy(ptr, &block.id, sizeof(BlockID));
            ptr += sizeof(BlockID);

            std::memcpy(ptr, &block.metadata, sizeof(uint8_t));
            ptr += sizeof(uint8_t);
        }
    }

    for(int x = 0; x < 16; x++)
    {
        const Climate& climate = iterator->second.climates[x]; 
        const Biome& biome = iterator->second.biomes[x];

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
        std::cerr << "ZSTD_compress failed for chunk " << chunk_position << ": " << ZSTD_getErrorName(compressed_size) << '\n';
        return;
    }

    std::filesystem::path chunk_dir = path / ("chunk_" + std::to_string(chunk_position));

    std::error_code ec;
    std::filesystem::create_directories(chunk_dir, ec);

    if(ec)
    {
        std::cerr << "Failed to create chunk directory: "
                << chunk_dir << " : "
                << ec.message() << '\n';
        return;
    }

    std::filesystem::path final_path = path / ("chunk_" + std::to_string(chunk_position)) / "environment";
    std::filesystem::path tmp_path = path / ("chunk_" + std::to_string(chunk_position)) / ("environment.tmp");

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
            std::error_code error_code;
            std::filesystem::remove(tmp_path, error_code);
            return;
        }
    }

    std::error_code error_code;
    std::filesystem::rename(tmp_path, final_path, error_code);
    if(error_code)
    {
        std::cerr << "Failed to finalize chunk " << chunk_position << ": " << error_code.message() << '\n';
        std::filesystem::remove(tmp_path, error_code);
    }
}

void World::saveChunkEntities(int chunk_position)
{
    Chunk& chunk = getChunk(chunk_position);

    BBT root("entities_" + std::to_string(chunk_position));

    for(auto& uuid : chunk.entity_ids)
    {     
        root[entities.at(uuid).getID().toString()] = entities.at(uuid).serialize();
    }

    std::ofstream stream(path / ("chunk_" + std::to_string(chunk_position)) / "entities", std::ios::binary);

    std::vector<uint8_t> buffer = root.save();

    stream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

    stream.close();
}

void World::loadChunk(int chunk_position)
{
    loadChunkEnvironment(chunk_position);
    loadChunkEntities(chunk_position);
}

void World::loadChunkEnvironment(int chunk_position)
{
    std::filesystem::path chunk_path = path / ("chunk_" + std::to_string(chunk_position)) / "environment";

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

void World::loadChunkEntities(int chunk_position)
{
    std::ifstream stream(path / ("chunk_" + std::to_string(chunk_position)) / "entities", std::ios::ate | std::ios::binary);

    if(!stream) throw std::runtime_error("Failed to open entities file");

    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    stream.close();


    BBT root = BBT::load(buffer);

    for(auto& [id_string, payload] : root.root())
    {
        Entity entity(uuidFromString(id_string).value());

        entity.deserialize(payload);

        addEntity(std::move(entity));
    }
}

void World::savePlayer(UUID entity_id)
{
    Entity& entity = getEntity(entity_id);

    if(!entity.hasComponent<PlayerControlledComponent>()) return;


    auto& player = entity.getComponent<PlayerControlledComponent>();



    BBT root(player.nickname);

    root.root() = entities.at(entity_id).serialize().get<TagCompound>();


    std::ofstream stream(path / (player.nickname), std::ios::binary);

    std::vector<uint8_t> buffer = root.save();

    stream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

    stream.close();

    std::cout << "Saved entity for player " << player.nickname << '\n';
}


UUID World::loadPlayer(std::string nickname)
{
    std::filesystem::path player_path = path / nickname;

    std::ifstream stream(player_path, std::ios::ate | std::ios::binary);

    if(!stream) throw std::runtime_error("Failed to open player file");

    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    stream.close();


    BBT root = BBT::load(buffer);

    Entity entity(generateUUID());

    Tag tag = Tag(root.root());

    entity.deserialize(tag);

    addEntity(std::move(entity));

    std::cout << "Loaded entity for player " << nickname << '\n';

    return entity.getID();
}

bool World::playerFileExist(std::string nickname) const
{
    return std::filesystem::exists(path / (nickname));
}