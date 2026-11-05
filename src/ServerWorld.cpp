#include "../include/ServerWorld.hpp"
#include "../include/NormalWorldGenerator.hpp"
#include "../include/FlatWorldGenerator.hpp"
#include "../include/DungeonShooterWorldGenerator.hpp"
#include "../include/SkygridWorldGenerator.hpp"
#include "../include/RandomizerWorldGenerator.hpp"
#include "../include/NoodleWorldGenerator.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/AIComponent.hpp"
#include "../include/BlockEntity.hpp"
#include "../include/ChestBlockEntity.hpp"
#include "../include/FurnaceBlockEntity.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/ItemInstance.hpp"
#include "../include/Item.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/WorldLightView.hpp"

#include <algorithm>

#include <fstream>
#include <zstd.h>
#include <iostream>
#include <random>

static std::unique_ptr<WorldGenerator> makeWorldGenerator(unsigned int seed, const GenerationProperties& generation_properties)
{
    switch (generation_properties.type)
    {
        case GenerationType::Flat:
        {
            FlatWorldProperties flat_properties;
            flat_properties.layers = generation_properties.flat_layers;
            if (flat_properties.layers.empty())
            {
                flat_properties = FlatWorldProperties::defaults();
            }

            return std::make_unique<FlatWorldGenerator>(std::move(flat_properties));
        }

        case GenerationType::DungeonShooter:
        {
            return std::make_unique<DungeonShooterWorldGenerator>(seed);
        }

        case GenerationType::Skygrid:
        {
            return std::make_unique<SkygridWorldGenerator>(seed);
        }

        case GenerationType::Randomizer:
        {
            return std::make_unique<RandomizerWorldGenerator>(seed);
        }

        case GenerationType::Noodle:
        {
            NormalWorldGeneratorProperties properties
            {
                .base_height = generation_properties.base_height,
                .height_scale = generation_properties.height_scale,
                .frequency = generation_properties.frequency,
                .amplitude = generation_properties.amplitude,
                .persistence = generation_properties.persistence
            };

            return std::make_unique<NoodleWorldGenerator>(seed, properties, generation_properties.noodle_threshold);
        }

        case GenerationType::Normal:
        default:
        {
            NormalWorldGeneratorProperties properties
            {
                .base_height = generation_properties.base_height,
                .height_scale = generation_properties.height_scale,
                .frequency = generation_properties.frequency,
                .amplitude = generation_properties.amplitude,
                .persistence = generation_properties.persistence
            };

            return std::make_unique<NormalWorldGenerator>(seed, properties);
        }
    }
}

ServerWorld::ServerWorld(const std::filesystem::path path) : path{path}
{
    load();

    if(!world_generator) throw("World generator is nullptr");
}

void ServerWorld::setBlock(int wx, int wy, Block block)
{
    setBlock(wx, wy, block, BlockUpdateFlags::Default);
}

void ServerWorld::setBlock(int wx, int wy, Block block, BlockUpdateFlags flags)
{
    Block previous = getBlock(wx, wy);

    // True no-op write: skip everything. Prevents redundant propagation when a
    // callback or system writes the same block state back.
    if (previous.id == block.id && previous.metadata == block.metadata) return;

    World::setBlock(wx, wy, block);
    pending_block_updates.emplace_back(wx, wy);

    if (!hasFlag(flags, BlockUpdateFlags::SuppressAll))
    {
        pending_propagation_events.push_back(PendingPropagation{
            .position = {wx, wy},
            .previous = previous,
            .current  = block,
            .flags    = flags,
            .depth    = 0
        });
    }

    if (previous.id != block.id)
    {
        if (previous.id == BlockID::Chest)
        {
            if (auto* chest = dynamic_cast<ChestBlockEntity*>(getBlockEntity(wx, wy)))
            {
                for (const auto& stack : chest->stacks)
                {
                    if (stack.empty()) continue;
                    spawnDroppedItem(*this, {wx + 0.25, wy + 0.25}, stack, randomDropSpread(60.0f));
                }
            }
            removeBlockEntity(wx, wy);
        }
        else if (previous.id == BlockID::Furnace)
        {
            if (auto* furnace = dynamic_cast<FurnaceBlockEntity*>(getBlockEntity(wx, wy)))
            {
                for (const ItemStack* stack : {&furnace->fuel, &furnace->input, &furnace->output})
                {
                    if (stack->empty()) continue;
                    spawnDroppedItem(*this, {wx + 0.25, wy + 0.25}, *stack, randomDropSpread(60.0f));
                }
            }
            removeBlockEntity(wx, wy);
        }

        if (block.id == BlockID::Chest)
        {
            setBlockEntity(wx, wy, std::make_unique<ChestBlockEntity>(sf::Vector2i{wx, wy}));
        }
        else if (block.id == BlockID::Furnace)
        {
            setBlockEntity(wx, wy, std::make_unique<FurnaceBlockEntity>(sf::Vector2i{wx, wy}));
        }
    }
}

void ServerWorld::addEntity(Entity entity)
{
    UUID id = entity.getID();
    new_entities.insert(id);
    dead_entities.erase(id);

    World::addEntity(std::move(entity));
}

void ServerWorld::removeEntity(UUID id)
{
    if (!doesEntityExist(id)) return;

    dead_entities.insert(id);
    new_entities.erase(id);

    World::removeEntity(id);
}

ServerWorld::ServerWorld(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties) : path(path), name(name), seed(seed), generation_properties{generation_properties}, perlin(seed)
{
    world_generator = makeWorldGenerator(seed, this->generation_properties);

    if(!world_generator) throw("World generator is nullptr");

    generateWorldSpawn();
    save();
}

void ServerWorld::save()
{
    saveManifest();
    saveData();
    saveItemInstances();

    for(auto& [chunk_position, chunk] : chunks)
    {
        if(chunk.dirty)
        {
            saveChunk(chunk_position);
        }
    }
}

void ServerWorld::load()
{
    if(!std::filesystem::exists(path)) throw std::runtime_error("World path does not exist");

    loadManifest();
    loadData();
    loadItemInstances();

    perlin = PerlinNoise(seed);
}


void ServerWorld::saveManifest()
{
    BBT root("manifest");

    root["name"] = Tag(name);


    std::ofstream file(path / "manifest", std::ios::binary);

    if(!file) throw std::runtime_error("Cannot open manifest file");


    std::vector<uint8_t> buffer = root.save();

    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());


    file.close();
}

void ServerWorld::loadManifest()
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


void ServerWorld::saveData()
{
    BBT data("data");

    data["generation_properties"] = generation_properties.serialize();

    data["seed"] = Tag(seed);

    data["day_time"] = Tag(getDayTime());
    data["days"] = Tag(days);

    auto buffer = data.save();

    std::ofstream file(path / "data", std::ios::out | std::ios::binary);

    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

void ServerWorld::loadData()
{
    std::ifstream file(path / "data", std::ios::ate | std::ios::binary);

    if(!file) throw std::runtime_error("Failed to open data file");

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    file.read(reinterpret_cast<char*>(buffer.data()), size);

    file.close();


    BBT data = BBT::load(buffer);

    seed = data["seed"].get<unsigned int>();

    dayTime = data["day_time"].get<float>();
    days = data["days"].get<uint64_t>();

    generation_properties.deserialize(data["generation_properties"]);

    world_generator = makeWorldGenerator(seed, generation_properties);
}


void ServerWorld::saveItemInstances()
{
    BBT root("item_instances");
    root.root() = serializeItemInstances().get<TagCompound>();

    std::ofstream file(path / "item_instances", std::ios::out | std::ios::binary);
    if (!file) return;

    std::vector<uint8_t> buffer = root.save();
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

void ServerWorld::loadItemInstances()
{
    itemInstances.clear();

    std::filesystem::path file_path = path / "item_instances";
    if (!std::filesystem::exists(file_path)) return;

    std::ifstream file(file_path, std::ios::ate | std::ios::binary);
    if (!file) return;

    std::streamsize size = file.tellg();
    if (size <= 0) return;
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    BBT root = BBT::load(buffer);

    deserializeItemInstances(Tag(root.root()));
}

void ServerWorld::saveChunk(int chunk_position)
{
    saveChunkEnvironment(chunk_position);
    saveChunkEntities(chunk_position);
    saveChunkBlockEntities(chunk_position);
    saveChunkScheduledTicks(chunk_position);
}

void ServerWorld::saveChunkScheduledTicks(int chunk_position)
{
    auto it = chunks.find(chunk_position);
    if (it == chunks.end()) return;

    Tag ticks_tag = block_update_pipeline.serializeChunk(chunk_position);

    BBT root("scheduled_ticks_" + std::to_string(chunk_position));
    root.root() = ticks_tag.get<TagCompound>();

    std::ofstream stream(path / ("chunk_" + std::to_string(chunk_position)) / "scheduled_ticks", std::ios::binary);
    if (!stream) return;

    std::vector<uint8_t> buffer = root.save();
    stream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

void ServerWorld::loadChunkScheduledTicks(int chunk_position)
{
    std::filesystem::path file_path = path / ("chunk_" + std::to_string(chunk_position)) / "scheduled_ticks";
    if (!std::filesystem::exists(file_path)) return;

    std::ifstream stream(file_path, std::ios::ate | std::ios::binary);
    if (!stream) return;

    std::streamsize size = stream.tellg();
    if (size <= 0) return;
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    BBT root = BBT::load(buffer);
    block_update_pipeline.deserializeChunk(chunk_position, Tag(root.root()));
}

static constexpr uint32_t CHUNK_FILE_MAGIC = 0x4B434242u;

void ServerWorld::saveChunkEnvironment(int chunk_position)
{
    auto iterator = chunks.find(chunk_position);
    if(iterator == chunks.end()) return;

    size_t block_size = sizeof(BlockID) + sizeof(uint16_t);
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

            std::memcpy(ptr, &block.metadata, sizeof(uint16_t));
            ptr += sizeof(uint16_t);
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

void ServerWorld::saveChunkEntities(int chunk_position)
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

void ServerWorld::saveChunkBlockEntities(int chunk_position)
{
    auto it = chunks.find(chunk_position);
    if (it == chunks.end()) return;

    BBT root("block_entities_" + std::to_string(chunk_position));

    for (const auto& [key, entity] : it->second.block_entities)
    {
        if (!entity) continue;

        TagCompound entry;
        entry["type"] = Tag(static_cast<uint32_t>(entity->typeId()));
        entry["x"] = Tag(static_cast<int32_t>(entity->position.x));
        entry["y"] = Tag(static_cast<int32_t>(entity->position.y));
        entry["data"] = entity->serialize();

        root[std::to_string(key)] = Tag(entry);
    }

    std::ofstream stream(path / ("chunk_" + std::to_string(chunk_position)) / "block_entities", std::ios::binary);
    if (!stream) return;

    std::vector<uint8_t> buffer = root.save();
    stream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

void ServerWorld::loadChunkBlockEntities(int chunk_position)
{
    std::filesystem::path file_path = path / ("chunk_" + std::to_string(chunk_position)) / "block_entities";

    if (!std::filesystem::exists(file_path)) return;

    std::ifstream stream(file_path, std::ios::ate | std::ios::binary);
    if (!stream) return;

    std::streamsize size = stream.tellg();
    if (size <= 0) return;
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    BBT root = BBT::load(buffer);

    Chunk& chunk = chunks[chunk_position];

    for (const auto& [key_str, entry_tag] : root.root())
    {
        const auto& entry = entry_tag.get<TagCompound>();
        auto type_it = entry.find("type");
        auto x_it = entry.find("x");
        auto y_it = entry.find("y");
        auto data_it = entry.find("data");
        if (type_it == entry.end() || x_it == entry.end() || y_it == entry.end() || data_it == entry.end()) continue;

        sf::Vector2i wpos{x_it->second.get<int32_t>(), y_it->second.get<int32_t>()};
        auto entity = BlockEntity::create(static_cast<BlockEntityID>(type_it->second.get<uint32_t>()), wpos);
        if (!entity) continue;

        entity->deserialize(data_it->second);

        int local_x = wpos.x - chunk_position * CHUNK_WIDTH;
        chunk.setBlockEntity(local_x, wpos.y, std::move(entity));
    }
}

void ServerWorld::loadChunk(int chunk_position)
{
    loadChunkEnvironment(chunk_position);
    loadChunkEntities(chunk_position);
    loadChunkBlockEntities(chunk_position);
    loadChunkScheduledTicks(chunk_position);
}

void ServerWorld::loadChunkEnvironment(int chunk_position)
{
    std::filesystem::path chunk_path = path / ("chunk_" + std::to_string(chunk_position)) / "environment";

    size_t expected_raw_size = CHUNK_WIDTH * CHUNK_HEIGHT * (sizeof(BlockID) + sizeof(uint16_t)) + CHUNK_WIDTH * (sizeof(Climate) + sizeof(Biome));

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

            std::memcpy(&block.metadata, ptr, sizeof(uint16_t));
            ptr += sizeof(uint16_t);
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

void ServerWorld::loadChunkEntities(int chunk_position)
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

void ServerWorld::savePlayer(UUID entity_id)
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


UUID ServerWorld::loadPlayer(std::string nickname)
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

    UUID id = entity.getID();
    addEntity(std::move(entity));

    std::cout << "Loaded entity for player " << nickname << '\n';

    return id;
}

bool ServerWorld::playerFileExist(std::string nickname) const
{
    return std::filesystem::exists(path / (nickname));
}

bool ServerWorld::hasChunkFile(int chunk_position) const
{
    return std::filesystem::exists(path / ("chunk_" + std::to_string(chunk_position)));
}

void ServerWorld::loadOrCreateChunk(int chunk_position)
{
    if (chunks.contains(chunk_position) && chunks.at(chunk_position).generated)
    {
        return;
    }

    bool freshly_generated = false;

    if (hasChunkFile(chunk_position))
    {
        loadChunk(chunk_position);
    }
    else
    {

        if (!world_generator)
            throw std::runtime_error("world_generator is NULL");

        chunks[chunk_position] =
            world_generator->generateChunk(chunk_position);

        freshly_generated = true;
    }

    // Drain structure-generated entity spawns into the live world. Only runs
    // on a fresh chunk — loaded chunks already have their entities restored
    // from disk by loadChunkEntities. pending_spawns is never serialised, so
    // this cannot double-spawn across reloads.
    if (freshly_generated)
    {
        auto gen_it = chunks.find(chunk_position);
        if (gen_it != chunks.end())
        {
            std::vector<PendingSpawn> drained;
            drained.swap(gen_it->second.pending_spawns);

            for (const PendingSpawn& ps : drained)
            {
                spawn(ps.prefab, ps.position);
            }
        }
    }

    // Re-light the freshly materialized chunk together with any already-lit
    // neighbor in one pass. If a chunk was previously lit in isolation (the
    // neighbor on one side wasn't loaded yet), its border cells reflect that
    // missing context and the seam shows up as a visible light cut once the
    // other side appears. Regenerating the whole group from scratch — seeding
    // every column of every member before a single BFS — makes the border
    // propagate as if the chunks had always been loaded together.
    auto chunk_it = chunks.find(chunk_position);
    if (chunk_it != chunks.end())
    {
        std::vector<int> group;
        group.push_back(chunk_position);
        for (int neighbor_cp : {chunk_position - 1, chunk_position + 1})
        {
            auto n_it = chunks.find(neighbor_cp);
            if (n_it == chunks.end()) continue;
            if (!n_it->second.light.initialized) continue;
            group.push_back(neighbor_cp);
        }

        for (int cp : group)
        {
            auto it = chunks.find(cp);
            if (it == chunks.end()) continue;
            auto& light = it->second.light;
            if (!light.block_light.empty())
                std::fill(light.block_light.begin(), light.block_light.end(), 0);
            if (!light.sky_light.empty())
                std::fill(light.sky_light.begin(), light.sky_light.end(), 0);
            light.texture_dirty = true;
        }

        WorldLightView view(*this);
        initializeChunkGroupSkyLight(view, group, CHUNK_WIDTH, CHUNK_HEIGHT);

        for (int cp : group)
        {
            auto it = chunks.find(cp);
            if (it != chunks.end()) it->second.light.initialized = true;
        }
    }
}

sf::Vector2<double> ServerWorld::getSpawnPoint()
{
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist_x(-16, 16);

    // Player AABB from Prefab::Player: position is bottom-left, size {0.75, 1.0}.
    // At spawn x = wx + 0.5, the body straddles tiles wx and wx + 1 horizontally
    // and occupies one tile vertically. We need both columns clear with a solid
    // floor underneath, otherwise physics shoves the player into the terrain.
    auto chunkOf = [](int x) { return (x >= 0) ? x / CHUNK_WIDTH : (x - CHUNK_WIDTH + 1) / CHUNK_WIDTH; };

    auto isPassable = [](Block b) { return b.id == BlockID::Air; };
    auto isFloor    = [](Block b) { return b.id != BlockID::Air && b.id != BlockID::Water && b.id != BlockID::Lava; };

    auto findSpawnY = [&](int wx) -> std::optional<int>
    {
        const int cl = wx;
        const int cr = wx + 1;
        loadOrCreateChunk(chunkOf(cl));
        if (chunkOf(cr) != chunkOf(cl)) loadOrCreateChunk(chunkOf(cr));

        // Resolve each column's surface independently: top-down scan stops at
        // the first non-passable cell. Spawning one above the taller of the
        // two guarantees the player lands on the highest exposed block and
        // never inside a cave pocket beneath a surface — the old joint-scan
        // could fall through a tree/overhang on one side and bottom out in
        // an underground air pocket where both columns coincidentally match.
        auto highestFloor = [&](int x) -> std::optional<int>
        {
            for (int y = CHUNK_HEIGHT - 2; y >= 1; --y)
                if (isFloor(getBlock(x, y))) return y;
            return std::nullopt;
        };

        auto fl = highestFloor(cl);
        auto fr = highestFloor(cr);
        if (!fl || !fr) return std::nullopt;

        const int spawn_y = std::max(*fl, *fr) + 1;
        if (spawn_y >= CHUNK_HEIGHT - 1) return std::nullopt;
        if (!isPassable(getBlock(cl, spawn_y))) return std::nullopt;
        if (!isPassable(getBlock(cr, spawn_y))) return std::nullopt;
        return spawn_y;
    };

    for (int attempt = 0; attempt < 24; ++attempt)
    {
        int wx = dist_x(rng);
        if (auto y = findSpawnY(wx))
            return {static_cast<double>(wx) + 0.5, static_cast<double>(*y)};
    }

    if (auto y = findSpawnY(0))
        return {0.5, static_cast<double>(*y)};

    return {0.5, static_cast<double>(CHUNK_HEIGHT - 10)};
}

void ServerWorld::generateWorldSpawn()
{
    for (int i = -2; i <= 2; ++i)
    {
        chunks[i] = world_generator->generateChunk(i);
    }

    if (generation_properties.type == GenerationType::Normal
        && generation_properties.spawn_item_chest_column)
    {
        placeStarterItemChestColumn();
    }
}

void ServerWorld::placeStarterItemChestColumn()
{
    int surface_y = -1;

    for (int y = CHUNK_HEIGHT - 2; y >= 1; --y)
    {
        if (getBlock(0, y).id != BlockID::Air)
        {
            surface_y = y;
            break;
        }
    }

    if (surface_y < 0) return;

    std::vector<ItemID> items;
    items.reserve(itemDatabase.size());

    for (const auto& [id, data] : itemDatabase)
    {
        if (id == ItemID::None) continue;
        items.push_back(id);
    }

    std::sort(items.begin(), items.end(),
        [](ItemID a, ItemID b) { return static_cast<uint32_t>(a) < static_cast<uint32_t>(b); });

    size_t placed = 0;
    int y = surface_y + 1;

    while (placed < items.size() && y < CHUNK_HEIGHT - 1)
    {
        setBlock(0, y, {BlockID::Chest, 0});

        auto* chest = dynamic_cast<ChestBlockEntity*>(getBlockEntity(0, y));
        if (!chest) { ++y; continue; }

        for (size_t slot = 0; slot < chest->stacks.size() && placed < items.size(); ++slot, ++placed)
        {
            ItemID id = items[placed];
            uint32_t stack_size = itemDatabase.at(id).maxStackSize;
            if (stack_size == 0) stack_size = 1;
            chest->stacks[slot] = ItemStack{id, stack_size, std::nullopt};
        }

        ++y;
    }
}

UUID ServerWorld::spawn(PrefabID prefab_id, sf::Vector2<double> position)
{
    Entity entity = PrefabRegistry::get(prefab_id).cloneAs(generateUUID());

    if(entity.hasComponent<TransformComponent>()) entity.getComponent<TransformComponent>().position = position;

    addEntity(std::move(entity));

    return entity.getID();
}

void ServerWorld::refreshChunkStates()
{
    loaded_chunk_positions.clear();
    lazy_chunk_positions.clear();

    for (const auto& [id, entity] : entities)
    {
        if (!entity.hasComponent<PlayerControlledComponent>()) continue;
        if (!entity.hasComponent<TransformComponent>()) continue;

        int player_chunk = entity.getComponent<TransformComponent>().chunkPosition();

        for (int offset = -LAZY_DISTANCE; offset <= LAZY_DISTANCE; ++offset)
        {
            int chunk_position = player_chunk + offset;
            if (std::abs(offset) <= SIMULATION_DISTANCE)
            {
                loaded_chunk_positions.insert(chunk_position);
            }
            else
            {
                lazy_chunk_positions.insert(chunk_position);
            }
        }
    }

    // A chunk simultaneously inside one player's loaded radius and another's
    // lazy ring must be treated as loaded.
    for (int chunk_position : loaded_chunk_positions)
    {
        lazy_chunk_positions.erase(chunk_position);
    }
}

ChunkState ServerWorld::chunkState(int chunk_position) const
{
    if (loaded_chunk_positions.contains(chunk_position)) return ChunkState::Loaded;
    if (lazy_chunk_positions.contains(chunk_position))   return ChunkState::Lazy;
    return ChunkState::Unloaded;
}

bool ServerWorld::isChunkLoaded(int chunk_position) const
{
    return loaded_chunk_positions.contains(chunk_position);
}

bool ServerWorld::isChunkLazy(int chunk_position) const
{
    return lazy_chunk_positions.contains(chunk_position);
}

