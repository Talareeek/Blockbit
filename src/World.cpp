#include "../include/World.hpp"
#include "../include/Block.hpp"
#include "../include/AssetManager.hpp"
#include "../include/RenderSystem.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/MainGameState.hpp"
#include "../include/PreserveComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/MainGameState.hpp"
#include "../include/ExplosiveComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/PlayerComponent.hpp"
#include <SFML/Graphics.hpp>

#include <iostream>

World::World(const std::filesystem::path path) : path{path}
{
    load();
}

World::World(const std::string name, const std::filesystem::path path, unsigned int seed) : name(name), path(path), seed(seed), perlin(seed)
{
    generateWorldSpawn();
    createPlayer();
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
    chunk.generated = true;
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

            else if(y == height - 1) {
                if(y < SEA_LEVEL - 1) chunk.blocks[y][x] = {BlockID::Dirt, 0};
                else chunk.blocks[y][x] = {BlockID::Grass, 0};
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
                        static_cast<int>(texture.getSize().y) - texHeight},
                        {static_cast<int>(texture.getSize().x),
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

    if(world.getBlock(blockX, blockY).id != BlockID::Air && isBlockInRange(entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>(), blockPos, 4.0f))
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

const std::vector<Entity>& World::getEntities() const
{
    return entities;
}

uint32_t World::getVersion() const
{
    return version;
}

sf::Vector2i getMouseBlockPosition(const World& world, const sf::RenderWindow& window)
{
    auto mousePos = getMouseWorldPosition(world, window);
    return {static_cast<int>(std::floor(mousePos.x)), static_cast<int>(std::floor(mousePos.y))};
}

sf::Vector2f getMouseWorldPosition(const World&, const sf::RenderWindow& window)
{
    float unit_size = window.getView().getSize().y / static_cast<float>(MainGameState::UNIT_SIZE_FACTOR);

    sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    return {-(mouseWorld.x / unit_size), -(mouseWorld.y / unit_size)};
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


void World::writeManifest() const
{
    std::ofstream file(path / "manifest");

    if(!file) throw std::runtime_error("Cannot open file");

    file << name << '\n';
    file << seed << '\n';
}

void World::writeChunk(int chunk_position) const
{
    std::ofstream file(path / ("chunk_" + std::to_string(chunk_position)), std::ios::binary);

    auto it = chunks.find(chunk_position);
    if(it == chunks.end()) return;

    for(int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for(int x = 0; x < CHUNK_WIDTH; x++)
        {
            const Block& block = it->second.blocks[y][x];
            file.write(reinterpret_cast<const char*>(&block.id), sizeof(BlockID));
            file.write(reinterpret_cast<const char*>(&block.metadata), sizeof(uint8_t));
        }
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
            else if(type == typeid(PlayerComponent))
            {
                file << "Player" << '\n';
                file << std::any_cast<PlayerComponent>(component).serialize();
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
    std::ofstream file(path / "data", std::ios::out);

    file << "DayTime: " << getDayTime() << '\n';
    file << "Days: " << days << '\n';
    file << "Spawn Point: " << getSpawnPoint().x << ' ' << getSpawnPoint().y << '\n';
    file << "Player ID: " << getPlayerID() << '\n';
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
}

void World::readChunk(int chunk_position)
{
    std::ifstream file(path / ("chunk_" + std::to_string(chunk_position)), std::ios::binary);

    if(!file) throw std::runtime_error("Cannot open file");

    Chunk c;
    c.chunk_position = chunk_position;

    for(int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for(int x = 0; x < CHUNK_WIDTH; x++)
        {
            Block block;

            file.read(reinterpret_cast<char*>(&block.id), sizeof(BlockID));
            file.read(reinterpret_cast<char*>(&block.metadata), sizeof(uint8_t));

            c.blocks[y][x] = block;
        }
    }

    c.generated = true;
    chunks[chunk_position] = c;
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
                    else if (componentType == "Player")
                    {
                        PlayerComponent c;
                        c.deserialize(componentData);
                        entity.addComponent<PlayerComponent>(c);
                        std::cout << "\t\tPlayerComponent loaded\n";
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
    std::ifstream file(path / "data", std::ios::in);

    if(!file) throw std::runtime_error("Failed to open data file");

    std::string trash;

    file >> trash >> dayTime;

    file >> trash >> days;

    file >> trash >> trash >> spawnPoint.x >> spawnPoint.y;

    file >> trash >> trash >> playerID;

    std::cout << "Data file: " << std::endl;
    std::cout << "Daytime: " << dayTime << std::endl;
    std::cout << "Days: " << days << std::endl;
    std::cout << "Spawnpoint: " << spawnPoint.x << ' ' << spawnPoint.y << std::endl;
    std::cout << "Player ID: " << playerID << std::endl;
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
        createPlayer();
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
            createPlayer();
        }
    } else {
        // No entities file -> create default player
        createPlayer();
    }

    // Data
    if(std::filesystem::exists(path / "data")) {
        try {
            readData();
        } catch(const std::exception& e) {
            std::cerr << "Warning: Failed to read data: " << e.what() << '\n';
            dayTime = 0.0f;
            days = 0;
            spawnPoint = {0.0f, 0.0f};
        }
    } else {
        // No data file -> write defaults
        try { writeData(); } catch(const std::exception& e) { std::cerr << "Warning: Failed to write data: " << e.what() << '\n'; }
    }

    // Persist any missing files
    try { save(); } catch(const std::exception& e) { std::cerr << "Warning: Failed to save world: " << e.what() << '\n'; }
}

void World::createPlayer()
{
    setPlayerID(getPossibleID());

    entities.emplace_back(getPlayerID());

    entityWithID(getPlayerID(), *this).addComponent(TransformComponent{{0.0f, 0.0f}, {1.0f, 1.0f}, sf::degrees(0.0f)});
    entityWithID(getPlayerID(), *this).addComponent(PhysicsComponent{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f, true, true, false, true});
    entityWithID(getPlayerID(), *this).addComponent(InventoryComponent(36));
    entityWithID(getPlayerID(), *this).getComponent<InventoryComponent>().inventory.slots[0] = {ItemID::Dynamite, 16};

    for(int i = 0; i < 255; i++)
    {
        if(getBlock(0, i).id == BlockID::Air)
        {
            entityWithID(getPlayerID(), *this).getComponent<TransformComponent>().position.y = i + 1.0f;
            break;
        }
    }

    entityWithID(getPlayerID(), *this).addComponent(RenderComponent{0, {{0, 0}, {16, 16}}, {1.0f, 1.0f}});

    entityWithID(getPlayerID(), *this).addComponent(HealthComponent{100, 100});
    entityWithID(getPlayerID(), *this).addComponent(PlayerComponent{});

}