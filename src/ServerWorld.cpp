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
#include "../include/InventoryComponent.hpp"
#include "../include/ItemInstance.hpp"
#include "../include/Item.hpp"
#include "../include/GameRegistries.hpp"
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

    data["weather"] = Tag(static_cast<uint8_t>(weather));
    data["weather_timer"] = Tag(weatherTimer);

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

    // Weather fields were added after the initial save format, so tolerate
    // older world files that don't have them. Missing entries just leave the
    // defaults (Clear, timer=0) and WeatherSystem will roll a fresh state on
    // the next tick.
    if (data.root().count("weather"))        weather       = static_cast<WeatherState>(data["weather"].get<uint8_t>());
    if (data.root().count("weather_timer"))  weatherTimer  = data["weather_timer"].get<float>();

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

void ServerWorld::garbageCollectItemInstances(const std::unordered_set<UUID>& extra_live_ids)
{
    std::unordered_set<UUID> live = extra_live_ids;

    auto visitStack = [&](const ItemStack& stack)
    {
        if (stack.instanceID.has_value()) live.insert(*stack.instanceID);
    };

    for (const auto& [entity_id, entity] : entities)
    {
        if (entity.hasComponent<InventoryComponent>())
        {
            for (const auto& slot : entity.getComponent<InventoryComponent>().inventory.slots)
            {
                visitStack(slot);
            }
        }

        if (entity.hasComponent<ItemComponent>())
        {
            visitStack(entity.getComponent<ItemComponent>().item);
        }
    }

    for (const auto& [chunk_position, chunk] : chunks)
    {
        for (const auto& [key, block_entity] : chunk.block_entities)
        {
            if (auto* chest = dynamic_cast<ChestBlockEntity*>(block_entity.get()))
            {
                for (const auto& slot : chest->stacks) visitStack(slot);
            }
            else if (auto* furnace = dynamic_cast<FurnaceBlockEntity*>(block_entity.get()))
            {
                visitStack(furnace->fuel);
                visitStack(furnace->input);
                visitStack(furnace->output);
            }
        }
    }

    for (auto it = itemInstances.begin(); it != itemInstances.end();)
    {
        if (live.find(it->first) == live.end())
        {
            it = itemInstances.erase(it);
        }
        else
        {
            ++it;
        }
    }
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

// V2 magic bytes ('BBC2' little-endian) — palette + indexes format with
// RegistryKey strings so content survives registry re-ordering between
// Blockbit versions and modpack additions. Older format saves are rejected
// and regenerated from world gen.
static constexpr uint32_t CHUNK_FILE_MAGIC_V2 = 0x32434242u;

void ServerWorld::saveChunkEnvironment(int chunk_position)
{
    auto iterator = chunks.find(chunk_position);
    if(iterator == chunks.end()) return;

    constexpr int kCells = CHUNK_WIDTH * CHUNK_HEIGHT;

    // Build the palette once: map BlockID → small palette index in first-seen
    // order. The on-disk palette entries are RegistryKey strings so the save
    // keeps working if a later game version shifts runtime ids around.
    std::unordered_map<uint32_t, uint8_t> seen;
    std::vector<std::string>              palette_keys;
    std::vector<uint8_t>                  indexes(kCells);
    std::vector<uint16_t>                 metadata(kCells);
    palette_keys.reserve(32);

    const auto& reg = gameRegistries().blocks;

    for (int y = 0, cell = 0; y < CHUNK_HEIGHT; ++y)
    {
        for (int x = 0; x < CHUNK_WIDTH; ++x, ++cell)
        {
            const Block& block = iterator->second.blocks[y][x];
            const uint32_t raw_id = static_cast<uint32_t>(block.id);
            auto [it, inserted] = seen.try_emplace(raw_id, static_cast<uint8_t>(palette_keys.size()));
            if (inserted)
            {
                if (palette_keys.size() >= 256)
                {
                    std::cerr << "saveChunkEnvironment: palette overflow at chunk " << chunk_position << '\n';
                    return;
                }
                if (raw_id < reg.size() && reg.contains(static_cast<RuntimeID>(raw_id)))
                {
                    palette_keys.push_back(reg.keyOf(static_cast<RuntimeID>(raw_id)));
                }
                else
                {
                    // Unknown id — log and fall back to air so the chunk still
                    // round-trips rather than silently corrupting a save.
                    std::cerr << "saveChunkEnvironment: unknown BlockID " << raw_id
                              << " in chunk " << chunk_position << " — writing as blockbit:air\n";
                    palette_keys.push_back("blockbit:air");
                }
            }
            indexes[cell]  = it->second;
            metadata[cell] = block.metadata;
        }
    }

    // Serialize the palette+indexes+climates+biomes blob (pre-zstd).
    std::vector<char> raw;
    raw.reserve(kCells + kCells * sizeof(uint16_t)
                + 32 * palette_keys.size()
                + CHUNK_WIDTH * (sizeof(Climate) + sizeof(Biome))
                + 8);

    auto appendBytes = [&](const void* data, std::size_t n)
    {
        const char* bytes = static_cast<const char*>(data);
        raw.insert(raw.end(), bytes, bytes + n);
    };

    const uint16_t palette_count = static_cast<uint16_t>(palette_keys.size());
    appendBytes(&palette_count, sizeof(uint16_t));
    for (const auto& key : palette_keys)
    {
        const uint16_t len = static_cast<uint16_t>(key.size());
        appendBytes(&len, sizeof(uint16_t));
        appendBytes(key.data(), len);
    }

    appendBytes(indexes.data(), indexes.size());
    appendBytes(metadata.data(), metadata.size() * sizeof(uint16_t));

    for (int x = 0; x < CHUNK_WIDTH; ++x)
    {
        appendBytes(&iterator->second.climates[x], sizeof(Climate));
        appendBytes(&iterator->second.biomes[x],   sizeof(Biome));
    }

    const size_t raw_size = raw.size();

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

        uint32_t magic             = CHUNK_FILE_MAGIC_V2;
        uint32_t raw_size32        = static_cast<uint32_t>(raw_size);
        uint32_t compressed_size32 = static_cast<uint32_t>(compressed_size);

        file.write(reinterpret_cast<const char*>(&magic),             sizeof(uint32_t));
        file.write(reinterpret_cast<const char*>(&raw_size32),        sizeof(uint32_t));
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

    auto discardCorrupt = [&](const std::string& reason)
    {
        std::cerr << "Discarding corrupt chunk file " << chunk_path
                  << " (" << reason << "); will regenerate\n";
        std::error_code ec;
        std::filesystem::remove(chunk_path, ec);
    };

    std::ifstream file(chunk_path, std::ios::binary);
    if(!file) return;

    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(uint32_t));
    if(file.gcount() != static_cast<std::streamsize>(sizeof(uint32_t)) || magic != CHUNK_FILE_MAGIC_V2)
    {
        discardCorrupt("not a V2 palette chunk file");
        return;
    }

    uint32_t raw_size        = 0;
    uint32_t compressed_size = 0;
    file.read(reinterpret_cast<char*>(&raw_size),        sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&compressed_size), sizeof(uint32_t));
    if(!file)
    {
        discardCorrupt("header incomplete");
        return;
    }

    std::vector<char> compressed(compressed_size);
    if(compressed_size > 0)
    {
        file.read(compressed.data(), compressed_size);
        if(file.gcount() != static_cast<std::streamsize>(compressed_size))
        {
            discardCorrupt("compressed payload truncated");
            return;
        }
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
    const char* end = raw.data() + raw.size();

    auto readFromBuffer = [&](void* dst, std::size_t n) -> bool
    {
        if (static_cast<std::size_t>(end - ptr) < n) return false;
        std::memcpy(dst, ptr, n);
        ptr += n;
        return true;
    };

    // Palette: [u16 count][count x (u16 len + bytes)]. Each key is looked up
    // in the live block registry — after Phase 2 lands, a missing key means a
    // pack dependency was dropped and we fall back to air with a loud log
    // line rather than corrupting the chunk.
    uint16_t palette_count = 0;
    if (!readFromBuffer(&palette_count, sizeof(uint16_t)))
    {
        discardCorrupt("v2 palette header truncated");
        return;
    }

    std::vector<BlockID> palette_to_id(palette_count, BlockID::Air);
    const auto& reg = gameRegistries().blocks;

    for (uint16_t i = 0; i < palette_count; ++i)
    {
        uint16_t len = 0;
        if (!readFromBuffer(&len, sizeof(uint16_t)))
        {
            discardCorrupt("v2 palette key length truncated");
            return;
        }
        if (static_cast<std::size_t>(end - ptr) < len)
        {
            discardCorrupt("v2 palette key bytes truncated");
            return;
        }
        std::string key(ptr, ptr + len);
        ptr += len;

        if (auto rid = reg.find(key); rid.has_value())
        {
            palette_to_id[i] = static_cast<BlockID>(*rid);
        }
        else
        {
            std::cerr << "loadChunkEnvironment: unknown block key '" << key
                      << "' in chunk " << chunk_position << " — substituting blockbit:air\n";
            palette_to_id[i] = BlockID::Air;
        }
    }

    constexpr int kCells = CHUNK_WIDTH * CHUNK_HEIGHT;
    if (static_cast<std::size_t>(end - ptr) < static_cast<std::size_t>(kCells) + kCells * sizeof(uint16_t))
    {
        discardCorrupt("v2 index/metadata truncated");
        return;
    }

    for (int y = 0, cell = 0; y < CHUNK_HEIGHT; ++y)
    {
        for (int x = 0; x < CHUNK_WIDTH; ++x, ++cell)
        {
            const uint8_t idx = static_cast<uint8_t>(ptr[cell]);
            if (idx >= palette_count)
            {
                discardCorrupt("v2 palette index out of range");
                return;
            }
            chunk.blocks[y][x].id = palette_to_id[idx];
        }
    }
    ptr += kCells;

    for (int y = 0, cell = 0; y < CHUNK_HEIGHT; ++y)
    {
        for (int x = 0; x < CHUNK_WIDTH; ++x, ++cell)
        {
            uint16_t md = 0;
            std::memcpy(&md, ptr + cell * sizeof(uint16_t), sizeof(uint16_t));
            chunk.blocks[y][x].metadata = md;
        }
    }
    ptr += kCells * sizeof(uint16_t);

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

namespace
{
    // Player entity files live inside the world directory keyed by identity id.
    // Using the id (16-char hex + .plr extension) instead of the raw display
    // name keeps the filesystem happy (no slashes/spaces/traversal) and makes
    // the save survive a persona-name change — the switch to Steam personas
    // is the whole reason this keying moved away from the nickname string.
    std::filesystem::path playerFilePath(const std::filesystem::path& world_path, uint64_t id)
    {
        return world_path / (identityIdToHex(id) + ".plr");
    }

    std::filesystem::path legacyPlayerFilePath(const std::filesystem::path& world_path, const std::string& display_name)
    {
        return world_path / display_name;
    }
}

void ServerWorld::savePlayer(UUID entity_id)
{
    Entity& entity = getEntity(entity_id);

    if(!entity.hasComponent<PlayerControlledComponent>()) return;


    auto& player = entity.getComponent<PlayerControlledComponent>();



    BBT root(player.identity.display_name);

    root.root() = entities.at(entity_id).serialize().get<TagCompound>();


    std::ofstream stream(playerFilePath(path, player.identity.id), std::ios::binary);

    std::vector<uint8_t> buffer = root.save();

    stream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

    stream.close();

    // Old build keyed the save file by the raw display name. If the world was
    // migrated after login (loadPlayer handled that), the legacy file is
    // already gone; this handles the case where we saved from a fresh spawn
    // that happened to share a name with a pre-identity leftover.
    std::error_code ec;
    std::filesystem::remove(legacyPlayerFilePath(path, player.identity.display_name), ec);

    std::cout << "Saved entity for player " << player.identity.display_name
              << " (id " << identityIdToHex(player.identity.id) << ")\n";
}


UUID ServerWorld::loadPlayer(const PlayerIdentity& identity)
{
    std::filesystem::path player_path = playerFilePath(path, identity.id);

    // Migrate pre-identity worlds: a save keyed by the raw display name gets
    // promoted to the id-hex path on first load.
    std::filesystem::path legacy = legacyPlayerFilePath(path, identity.display_name);
    if (!std::filesystem::exists(player_path) && std::filesystem::exists(legacy))
    {
        std::error_code ec;
        std::filesystem::rename(legacy, player_path, ec);
        if (ec)
        {
            // Can't rename across weird mounts — fall back to copy then remove.
            std::filesystem::copy_file(legacy, player_path, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(legacy, ec);
        }
        std::cout << "Migrated legacy player file '" << identity.display_name
                  << "' to " << identityIdToHex(identity.id) << ".plr\n";
    }

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

    // Save files predate the identity_id field; stamp the current identity on
    // the loaded component so subsequent saves migrate correctly.
    if (entity.hasComponent<PlayerControlledComponent>())
    {
        auto& player = entity.getComponent<PlayerControlledComponent>();
        player.identity = identity;
    }

    UUID id = entity.getID();
    addEntity(std::move(entity));

    std::cout << "Loaded entity for player " << identity.display_name << '\n';

    return id;
}

bool ServerWorld::playerFileExist(const PlayerIdentity& identity) const
{
    return std::filesystem::exists(playerFilePath(path, identity.id))
        || std::filesystem::exists(legacyPlayerFilePath(path, identity.display_name));
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

