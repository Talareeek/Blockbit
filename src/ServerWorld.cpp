#include "../include/ServerWorld.hpp"
#include "../include/NormalWorldGenerator.hpp"
#include "../include/FlatWorldGenerator.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/AIComponent.hpp"
#include "../include/BlockEntity.hpp"
#include "../include/ChestBlockEntity.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/ItemInstance.hpp"
#include "../include/RenderComponent.hpp"

#include <fstream>
#include <zstd.h>
#include <iostream>
#include <random>

static std::unique_ptr<WorldGenerator> makeWorldGenerator(unsigned int seed, const GenerationProperties& generation_properties)
{
    if (generation_properties.flat)
    {
        FlatWorldProperties flat_properties;
        flat_properties.layers = generation_properties.flat_layers;
        if (flat_properties.layers.empty())
        {
            flat_properties = FlatWorldProperties::defaults();
        }

        return std::make_unique<FlatWorldGenerator>(std::move(flat_properties));
    }

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

ServerWorld::ServerWorld(const std::filesystem::path path) : path{path}
{
    load();

    if(!world_generator) throw("World generator is nullptr");
}

void ServerWorld::setBlock(int wx, int wy, Block block)
{
    Block previous = getBlock(wx, wy);

    World::setBlock(wx, wy, block);
    pending_block_updates.emplace_back(wx, wy);

    if (previous.id != block.id)
    {
        if (previous.id == BlockID::Chest)
        {
            if (auto* chest = dynamic_cast<ChestBlockEntity*>(getBlockEntity(wx, wy)))
            {
                for (const auto& stack : chest->stacks)
                {
                    if (stack.empty()) continue;

                    Entity drop(generateUUID());
                    drop.addComponent(TransformComponent{{wx + 0.25, wy + 0.25}, {0.5f, 0.5f}, sf::degrees(0.0f)});
                    drop.addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, false, false, false, true});
                    drop.addComponent(ItemComponent(stack));
                    drop.addComponent(RenderComponent{itemDatabase[stack.itemID].texture, {{0, 0}, {16, 16}}, {0.5f, 0.5f}});
                    addEntity(std::move(drop));
                }
            }
            removeBlockEntity(wx, wy);
        }

        if (block.id == BlockID::Chest)
        {
            setBlockEntity(wx, wy, std::make_unique<ChestBlockEntity>(sf::Vector2i{wx, wy}));
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

ServerWorld::ServerWorld(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties) : name(name), path(path), seed(seed), perlin(seed), generation_properties{generation_properties}
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

    data["generation_properties"] = TagCompound();
    data["generation_properties"]["flat"] = Tag(generation_properties.flat);
    data["generation_properties"]["base_height"] = Tag(generation_properties.base_height);
    data["generation_properties"]["height_scale"] = Tag(generation_properties.height_scale);
    data["generation_properties"]["frequency"] = Tag(generation_properties.frequency);
    data["generation_properties"]["amplitude"] = Tag(generation_properties.amplitude);
    data["generation_properties"]["persistence"] = Tag(generation_properties.persistence);

    TagList flat_layers_list;
    for (const FlatWorldLayer& layer : generation_properties.flat_layers)
    {
        TagCompound layer_compound;
        layer_compound["block"] = Tag(static_cast<uint32_t>(layer.block));
        layer_compound["count"] = Tag(static_cast<int32_t>(layer.count));

        flat_layers_list.push_back(Tag(layer_compound));
    }
    data["generation_properties"]["flat_layers"] = Tag(flat_layers_list);

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
    
    generation_properties =
    {
        .flat = data["generation_properties"]["flat"].get<bool>(),
        .base_height = data["generation_properties"]["base_height"].get<float>(),
        .height_scale = data["generation_properties"]["height_scale"].get<float>(),
        .frequency = data["generation_properties"]["frequency"].get<float>(),
        .amplitude = data["generation_properties"]["amplitude"].get<float>(),
        .persistence = data["generation_properties"]["persistence"].get<float>()
    };

    generation_properties.flat_layers.clear();

    const TagCompound& gen_props = data["generation_properties"].get<TagCompound>();
    auto layers_iter = gen_props.find("flat_layers");
    if (layers_iter != gen_props.end())
    {
        const TagList& layers_list = layers_iter->second.get<TagList>();
        generation_properties.flat_layers.reserve(layers_list.size());

        for (const Tag& layer_tag : layers_list)
        {
            const TagCompound& layer_compound = layer_tag.get<TagCompound>();
            FlatWorldLayer layer;
            layer.block = static_cast<BlockID>(layer_compound.at("block").get<uint32_t>());
            layer.count = static_cast<int>(layer_compound.at("count").get<int32_t>());
            generation_properties.flat_layers.push_back(layer);
        }
    }

    if (generation_properties.flat_layers.empty())
    {
        generation_properties.flat_layers = FlatWorldProperties::defaults().layers;
    }

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
}

static constexpr uint32_t CHUNK_FILE_MAGIC = 0x4B434242u;

void ServerWorld::saveChunkEnvironment(int chunk_position)
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
}

void ServerWorld::loadChunkEnvironment(int chunk_position)
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
    }
}

sf::Vector2<double> ServerWorld::getSpawnPoint()
{
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist_x(-16, 16);

    auto findSurface = [&](int wx) -> std::optional<int>
    {
        for (int y = CHUNK_HEIGHT - 2; y >= 0; --y)
        {
            Block here  = getBlock(wx, y);
            Block above = getBlock(wx, y + 1);
            if (here.id != BlockID::Air && here.id != BlockID::Water && above.id == BlockID::Air)
            {
                return y + 1;
            }
        }
        return std::nullopt;
    };

    for (int attempt = 0; attempt < 24; ++attempt)
    {
        int wx = dist_x(rng);

        int chunk_position = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
        loadOrCreateChunk(chunk_position);

        if (auto y = findSurface(wx))
        {
            return {static_cast<double>(wx) + 0.5, static_cast<double>(*y)};
        }
    }

    loadOrCreateChunk(0);
    if (auto y = findSurface(0))
    {
        return {0.5, static_cast<double>(*y)};
    }

    return {0.5, static_cast<double>(CHUNK_HEIGHT - 10)};
}

void ServerWorld::generateWorldSpawn()
{
    for (int i = -2; i <= 2; ++i)
    {
        chunks[i] = world_generator->generateChunk(i);
    }
}

UUID ServerWorld::spawn(PrefabID prefab_id, sf::Vector2<double> position)
{
    Entity entity = PrefabRegistry::get(prefab_id).cloneAs(generateUUID());

    if(entity.hasComponent<TransformComponent>()) entity.getComponent<TransformComponent>().position = position;

    addEntity(std::move(entity));

    return entity.getID();
}

void ServerWorld::trySpawnPassiveMobs(float dt)
{
    constexpr float SPAWN_INTERVAL   = 20.0f;
    constexpr int   MAX_PIGS         = 15;
    constexpr int   MIN_DIST         = 10;
    constexpr int   MAX_DIST         = 40;
    constexpr int   GROUP_MIN        = 2;
    constexpr int   GROUP_MAX        = 4;

    passive_mob_spawn_timer += dt;
    if (passive_mob_spawn_timer < SPAWN_INTERVAL) return;
    passive_mob_spawn_timer = 0.0f;

    const bool is_day = dayTime < DAY_CYCLE_DURATION * 0.5f;
    if (!is_day) return;

    int pig_count = 0;
    for (const auto& [id, entity] : entities)
    {
        if (entity.hasComponent<AIComponent>()) ++pig_count;
    }
    if (pig_count >= MAX_PIGS) return;

    std::vector<UUID> players = getPlayerEntityIDs();
    if (players.empty()) return;

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist_offset(MIN_DIST, MAX_DIST);
    std::uniform_int_distribution<int> dist_sign(0, 1);
    std::uniform_int_distribution<int> dist_group(GROUP_MIN, GROUP_MAX);

    for (UUID player_id : players)
    {
        if (!doesEntityExist(player_id)) continue;
        Entity& player = getEntity(player_id);
        if (!player.hasComponent<TransformComponent>()) continue;

        int cx = static_cast<int>(std::floor(player.getComponent<TransformComponent>().position.x));

        int group = dist_group(rng);
        int anchor_x = cx + (dist_sign(rng) ? 1 : -1) * dist_offset(rng);

        for (int i = 0; i < group; ++i)
        {
            if (pig_count >= MAX_PIGS) return;

            std::uniform_int_distribution<int> dist_jitter(-3, 3);
            int wx = anchor_x + dist_jitter(rng);

            int chunk_position = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
            auto it = chunks.find(chunk_position);
            if (it == chunks.end() || !it->second.generated) continue;

            for (int y = CHUNK_HEIGHT - 3; y >= 1; --y)
            {
                Block ground = getBlock(wx, y);
                Block above1 = getBlock(wx, y + 1);
                Block above2 = getBlock(wx, y + 2);
                if (ground.id == BlockID::Grass && above1.id == BlockID::Air && above2.id == BlockID::Air)
                {
                    spawn(PrefabID::Pig, {static_cast<double>(wx) + 0.5, static_cast<double>(y + 1)});
                    ++pig_count;
                    break;
                }
            }
        }
    }
}