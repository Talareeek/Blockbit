#include "../include/CreateWorldState.hpp"
#include "../include/InputManager.hpp"
#include "../include/Game.hpp"
#include "../include/ClientGameState.hpp"
#include "../include/World.hpp"
#include "../include/ClientWorld.hpp"
#include "../include/Block.hpp"
#include "../include/BlockAtlas.hpp"
#include "../include/ChunkMesh.hpp"
#include "../include/Item.hpp"
#include "../include/Account.hpp"
#include "../include/Render.hpp"
#include "../include/Settings.hpp"
#include "../include/NormalWorldGenerator.hpp"
#include "../include/DungeonShooterWorldGenerator.hpp"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/View.hpp>

#include <imgui.h>
#include <imgui-SFML.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <random>
#include <sstream>
#include <string>

namespace
{
    constexpr std::array<BlockID, 39> BLOCK_OPTIONS
    {
        BlockID::Air,
        BlockID::Bedrock,
        BlockID::Stone,
        BlockID::Smooth_Stone,
        BlockID::Diorite,
        BlockID::Granite,
        BlockID::Dirt,
        BlockID::Grass,
        BlockID::Sand,
        BlockID::Coarse_Dirt,
        BlockID::Snow,
        BlockID::Cobblestone,
        BlockID::Obsidian,
        BlockID::Gravel,
        BlockID::Bricks,
        BlockID::Glass,
        BlockID::Oak_Log,
        BlockID::Oak_Leaves,
        BlockID::Oak_Planks,
        BlockID::Cactus,
        BlockID::Dead_Bush,
        BlockID::Coal_Ore,
        BlockID::Iron_Ore,
        BlockID::Gold_Ore,
        BlockID::Diamond_Ore,
        BlockID::Ruby_Ore,
        BlockID::Coal_Block,
        BlockID::Iron_Block,
        BlockID::Gold_Block,
        BlockID::Ruby_Block,
        BlockID::Water,
        BlockID::Lava,
        BlockID::Fire,
        BlockID::Chest,
        BlockID::Furnace,
        BlockID::Workbench,
        BlockID::Woodcutter,
        BlockID::Output_Bulb,
        BlockID::Ruby_Wire
    };

    int blockOptionIndex(BlockID id)
    {
        for (size_t i = 0; i < BLOCK_OPTIONS.size(); ++i)
        {
            if (BLOCK_OPTIONS[i] == id) return static_cast<int>(i);
        }
        return 0;
    }

    const char* blockName(BlockID id)
    {
        switch (id)
        {
            case BlockID::Air:   return "Air";
            case BlockID::Water: return "Water";
            case BlockID::Lava:  return "Lava";
            case BlockID::Fire:  return "Fire";
            default: break;
        }
        auto item = blockToItem(id);
        auto iter = itemDatabase.find(item);
        if (iter == itemDatabase.end()) return "?";
        return iter->second.name.c_str();
    }

    constexpr int      PREVIEW_CHUNK_RADIUS = 2;
    constexpr unsigned PREVIEW_TEXTURE_W    = 640;
    constexpr unsigned PREVIEW_TEXTURE_H    = 320;
    constexpr float    PREVIEW_UNIT_SIZE    = static_cast<float>(PREVIEW_TEXTURE_W) /
                                              static_cast<float>((PREVIEW_CHUNK_RADIUS * 2 + 1) * CHUNK_WIDTH);
    constexpr sf::Color PREVIEW_SKY { 135, 206, 235 };
}

CreateWorldState::CreateWorldState(Game* game) : GameState(game)
{
    flat_layers = FlatWorldProperties::defaults().layers;
    background_texture = generateBackground();

    rerollSeed();
}

void CreateWorldState::rerollSeed()
{
    static thread_local std::mt19937 rng(std::random_device{}());
    preview_seed = static_cast<unsigned int>(rng());
    std::snprintf(seed_buffer, sizeof(seed_buffer), "%u", preview_seed);
    rebuildPreview();
}

void CreateWorldState::rebuildPreview()
{
    unsigned int seed_value = preview_seed;
    const std::string seed_text(seed_buffer);
    if (!seed_text.empty())
    {
        std::istringstream stream(seed_text);
        unsigned int parsed = 0;
        if (stream >> parsed) seed_value = parsed;
    }
    preview_seed = seed_value;

    std::unique_ptr<WorldGenerator> gen;
    switch (mode)
    {
        case Mode::Flat:
        {
            FlatWorldProperties fp;
            fp.layers = flat_layers;
            gen = std::make_unique<FlatWorldGenerator>(std::move(fp));
            break;
        }
        case Mode::DungeonShooter:
            gen = std::make_unique<DungeonShooterWorldGenerator>(seed_value);
            break;
        case Mode::Normal:
        default:
        {
            NormalWorldGeneratorProperties np{
                base_height, height_scale, frequency, amplitude, persistence
            };
            gen = std::make_unique<NormalWorldGenerator>(seed_value, np);
            break;
        }
    }

    ClientWorld world;
    auto& world_chunks = world.getChunks();
    for (int cp = -PREVIEW_CHUNK_RADIUS; cp <= PREVIEW_CHUNK_RADIUS; ++cp)
    {
        world_chunks[cp] = gen->generateChunk(cp);
    }

    // BuildChunkMesh writes animation-frame data into vertex colors when
    // block_animations is on, which only renders correctly through the
    // animation shader. We render without that shader, so build the mesh in
    // the plain path and restore the user's setting afterwards.
    const bool prev_anim = Settings::graphics.block_animations;
    Settings::graphics.block_animations = false;
    for (int cp = -PREVIEW_CHUNK_RADIUS; cp <= PREVIEW_CHUNK_RADIUS; ++cp)
    {
        BuildChunkMesh(world, cp);
    }
    Settings::graphics.block_animations = prev_anim;

    // Center on chunk 0's average surface so the horizon sits roughly in the
    // middle of the texture regardless of base_height.
    double cam_y = 75.0;
    {
        auto it = world_chunks.find(0);
        if (it != world_chunks.end())
        {
            long long sum = 0;
            int count = 0;
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                int top = 0;
                for (int y = CHUNK_HEIGHT - 1; y >= 0; --y)
                {
                    if (it->second.blocks[y][x].id != BlockID::Air) { top = y; break; }
                }
                sum += top;
                ++count;
            }
            if (count > 0) cam_y = static_cast<double>(sum) / static_cast<double>(count);
        }
    }
    const double cam_x = static_cast<double>(CHUNK_WIDTH) * 0.5;

    sf::RenderTexture rt(sf::Vector2u(PREVIEW_TEXTURE_W, PREVIEW_TEXTURE_H));
    rt.clear(PREVIEW_SKY);

    sf::View view({0.0f, 0.0f},
                  {static_cast<float>(PREVIEW_TEXTURE_W), -static_cast<float>(PREVIEW_TEXTURE_H)});
    rt.setView(view);

    for (int cp = -PREVIEW_CHUNK_RADIUS; cp <= PREVIEW_CHUNK_RADIUS; ++cp)
    {
        const ChunkMesh& mesh = world.getChunkMeshes()[cp];
        if (!mesh.built) continue;

        const float tx = static_cast<float>(static_cast<double>(cp * CHUNK_WIDTH) - cam_x) * PREVIEW_UNIT_SIZE;
        const float ty = static_cast<float>(-cam_y) * PREVIEW_UNIT_SIZE;

        sf::RenderStates states;
        states.texture = &BlockAtlas::getTexture();
        states.transform.translate({tx, ty});
        states.transform.scale({PREVIEW_UNIT_SIZE, PREVIEW_UNIT_SIZE});

        rt.draw(mesh.vertices, states);
    }

    rt.display();

    const sf::Image image = rt.getTexture().copyToImage();
    if (preview_texture.loadFromImage(image))
    {
        preview_has_image = true;
    }
}

void CreateWorldState::createWorld()
{
    if (name_buffer[0] == '\0') return;

    std::string world_name(name_buffer);
    std::string seed_text(seed_buffer);

    std::filesystem::path savesPath = getWorldsPath();
    std::filesystem::path path = savesPath / world_name;

    if (std::filesystem::exists(path)) return;

    std::filesystem::create_directories(path);

    static thread_local std::mt19937 rng(std::random_device{}());
    unsigned int seed_value = static_cast<unsigned int>(rng());
    if (!seed_text.empty())
    {
        std::istringstream stream(seed_text);
        unsigned int parsed = 0;
        if (stream >> parsed) seed_value = parsed;
    }

    GenerationProperties properties;
    switch (mode)
    {
        case Mode::Flat:           properties.type = GenerationType::Flat;           break;
        case Mode::DungeonShooter: properties.type = GenerationType::DungeonShooter; break;
        case Mode::Normal:
        default:                   properties.type = GenerationType::Normal;         break;
    }
    properties.base_height = base_height;
    properties.height_scale = height_scale;
    properties.frequency = frequency;
    properties.amplitude = amplitude;
    properties.persistence = persistence;
    properties.flat_layers = flat_layers;
    properties.spawn_item_chest_column = (mode == Mode::Normal) && spawn_item_chest_column;

    this->game->popState(this);
    this->game->pushState(this, std::make_unique<ClientGameState>(this->game, world_name, seed_value, properties, 0, Account::username));
}

void CreateWorldState::handleEvent(const sf::Event& event)
{
    (void)event;
}

void CreateWorldState::update(float dt)
{
    (void)dt;

    if (should_close)
    {
        game->popState(this);
        return;
    }

    if (InputManager::isLazyKeyPressed(sf::Keyboard::Key::Escape))
    {
        game->popState(this);
        return;
    }

    ImVec2 screen = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(screen, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.16f, 0.16f, 0.18f, 0.94f));

    bool open = true;
    if (ImGui::Begin("Create world", &open, flags))
    {
        ImGui::InputText("Name", name_buffer, sizeof(name_buffer));
        ImGui::InputText("Seed", seed_buffer, sizeof(seed_buffer));
        if (ImGui::IsItemDeactivatedAfterEdit()) rebuildPreview();

        if (ImGui::Button("Random seed")) rerollSeed();
        ImGui::SameLine();
        const bool has_seed = seed_buffer[0] != '\0';
        if (!has_seed) ImGui::BeginDisabled();
        if (ImGui::Button("Copy seed")) ImGui::SetClipboardText(seed_buffer);
        if (!has_seed) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Refresh preview")) rebuildPreview();
        ImGui::TextDisabled("Leave seed empty for random at creation");

        ImGui::Separator();

        int mode_int = static_cast<int>(mode);
        const int previous_mode_int = mode_int;
        ImGui::TextUnformatted("Generation mode");
        ImGui::RadioButton("Normal", &mode_int, static_cast<int>(Mode::Normal));
        ImGui::SameLine();
        ImGui::RadioButton("Flat", &mode_int, static_cast<int>(Mode::Flat));
        ImGui::SameLine();
        ImGui::RadioButton("Dungeon Shooter", &mode_int, static_cast<int>(Mode::DungeonShooter));
        mode = static_cast<Mode>(mode_int);
        if (mode_int != previous_mode_int) rebuildPreview();

        ImGui::Separator();

        if (mode == Mode::Normal)
        {
            ImGui::TextUnformatted("Terrain parameters");
            ImGui::SliderFloat("Base height",  &base_height,  0.0f,  200.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) rebuildPreview();
            ImGui::SliderFloat("Height scale", &height_scale, 0.0f,  100.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) rebuildPreview();
            ImGui::SliderFloat("Frequency",    &frequency,    0.001f, 0.2f, "%.4f");
            if (ImGui::IsItemDeactivatedAfterEdit()) rebuildPreview();
            ImGui::SliderFloat("Amplitude",    &amplitude,    0.0f,  2.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) rebuildPreview();
            ImGui::SliderFloat("Persistence",  &persistence,  0.0f,  1.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) rebuildPreview();

            ImGui::Checkbox("Spawn item chest column at (0, surface)", &spawn_item_chest_column);
        }
        else if (mode == Mode::DungeonShooter)
        {
            ImGui::TextUnformatted("Infinite cobblestone dungeon generated from the world seed.");
        }
        else
        {
            ImGui::TextUnformatted("Flat layers (top first)");

            bool flat_changed = false;
            if (ImGui::Button("Reset to defaults"))
            {
                flat_layers = FlatWorldProperties::defaults().layers;
                flat_changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add layer"))
            {
                flat_layers.push_back({BlockID::Stone, 1});
                flat_changed = true;
            }

            int remove_index = -1;
            int move_up_index = -1;
            int move_down_index = -1;

            ImGui::BeginChild("##layers", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2.0f), true);
            for (int i = static_cast<int>(flat_layers.size()) - 1; i >= 0; --i)
            {
                ImGui::PushID(i);

                int selected = blockOptionIndex(flat_layers[i].block);

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
                if (ImGui::BeginCombo("##block", blockName(BLOCK_OPTIONS[selected])))
                {
                    for (int option_i = 0; option_i < static_cast<int>(BLOCK_OPTIONS.size()); ++option_i)
                    {
                        bool is_selected = (option_i == selected);
                        if (ImGui::Selectable(blockName(BLOCK_OPTIONS[option_i]), is_selected))
                        {
                            flat_layers[i].block = BLOCK_OPTIONS[option_i];
                            flat_changed = true;
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
                int count = flat_layers[i].count;
                if (ImGui::InputInt("##count", &count))
                {
                    if (count < 1) count = 1;
                    if (count > CHUNK_HEIGHT) count = CHUNK_HEIGHT;
                    flat_layers[i].count = count;
                    flat_changed = true;
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("^")) move_up_index = i;
                ImGui::SameLine();
                if (ImGui::SmallButton("v")) move_down_index = i;
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) remove_index = i;

                ImGui::PopID();
            }
            ImGui::EndChild();

            if (remove_index >= 0 && remove_index < static_cast<int>(flat_layers.size()))
            {
                flat_layers.erase(flat_layers.begin() + remove_index);
                flat_changed = true;
            }
            if (move_up_index >= 0 && move_up_index + 1 < static_cast<int>(flat_layers.size()))
            {
                std::swap(flat_layers[move_up_index], flat_layers[move_up_index + 1]);
                flat_changed = true;
            }
            if (move_down_index > 0 && move_down_index < static_cast<int>(flat_layers.size()))
            {
                std::swap(flat_layers[move_down_index], flat_layers[move_down_index - 1]);
                flat_changed = true;
            }

            int total = 0;
            for (const FlatWorldLayer& layer : flat_layers) total += layer.count;
            ImGui::Text("Total height: %d / %d", total, CHUNK_HEIGHT);

            if (flat_changed) rebuildPreview();
        }

        ImGui::Separator();

        ImGui::Text("Spawn preview (seed: %u)", preview_seed);
        if (preview_has_image && preview_texture.getSize().x > 0 && preview_texture.getSize().y > 0)
        {
            const sf::Vector2u ts = preview_texture.getSize();
            const float aspect = static_cast<float>(ts.y) / static_cast<float>(ts.x);
            const float avail_w = ImGui::GetContentRegionAvail().x;
            float pw = avail_w;
            float ph = pw * aspect;
            const float max_h = 280.0f;
            if (ph > max_h)
            {
                ph = max_h;
                pw = ph / aspect;
            }
            ImGui::Image(preview_texture, sf::Vector2f(pw, ph));
        }
        else
        {
            ImGui::TextDisabled("Press 'Random seed' or 'Refresh preview' to generate.");
        }

        ImGui::Separator();

        bool name_empty = name_buffer[0] == '\0';
        bool name_taken = !name_empty
            && std::filesystem::exists(getWorldsPath() / std::string(name_buffer));
        bool can_create = !name_empty && !name_taken;

        if (name_taken)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                "A world named \"%s\" already exists.", name_buffer);
        }

        if (!can_create) ImGui::BeginDisabled();
        bool clicked_create = ImGui::Button("Create world", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0.0f));
        if (!can_create) ImGui::EndDisabled();

        ImGui::SameLine();
        bool clicked_cancel = ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

        if (clicked_create && can_create)
        {
            ImGui::End();
            ImGui::PopStyleColor();
            createWorld();
            return;
        }
        if (clicked_cancel) should_close = true;
    }
    ImGui::End();
    ImGui::PopStyleColor();

    if (!open) should_close = true;
}

void CreateWorldState::render(sf::RenderWindow& window)
{
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f},
        {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    sf::Sprite background(background_texture);
    background.setPosition({0.0f, 0.0f});
    background.setScale(
        {
            static_cast<float>(window.getSize().x) / background.getTexture().getSize().x,
            static_cast<float>(window.getSize().y) / background.getTexture().getSize().y
        }
    );
    window.draw(background);
}
