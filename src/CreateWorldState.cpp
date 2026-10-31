#include "../include/CreateWorldState.hpp"
#include "../include/InputManager.hpp"
#include "../include/Game.hpp"
#include "../include/ClientGameState.hpp"
#include "../include/World.hpp"
#include "../include/Block.hpp"
#include "../include/Item.hpp"
#include "../include/Account.hpp"
#include "../include/Render.hpp"

#include <imgui.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>

namespace
{
    constexpr std::array<BlockID, 22> BLOCK_OPTIONS
    {
        BlockID::Bedrock,
        BlockID::Stone,
        BlockID::Dirt,
        BlockID::Grass,
        BlockID::Sand,
        BlockID::Coarse_Dirt,
        BlockID::Snow,
        BlockID::Cobblestone,
        BlockID::Obsidian,
        BlockID::Gravel,
        BlockID::Bricks,
        BlockID::Oak_Log,
        BlockID::Oak_Planks,
        BlockID::Coal_Ore,
        BlockID::Iron_Ore,
        BlockID::Gold_Ore,
        BlockID::Diamond_Ore,
        BlockID::Ruby_Ore,
        BlockID::Coal_Block,
        BlockID::Iron_Block,
        BlockID::Gold_Block,
        BlockID::Ruby_Block
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
        auto item = blockToItem(id);
        auto iter = itemDatabase.find(item);
        if (iter == itemDatabase.end()) return "?";
        return iter->second.name.c_str();
    }
}

CreateWorldState::CreateWorldState(Game* game) : GameState(game)
{
    flat_layers = FlatWorldProperties::defaults().layers;
    background_texture = generateBackground();
}

void CreateWorldState::createWorld()
{
    if (name_buffer[0] == '\0') return;

    std::string world_name(name_buffer);
    std::string seed_text(seed_buffer);

    std::filesystem::path savesPath = getWorldsPath();
    std::filesystem::path path = savesPath / world_name;

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
        ImGui::TextDisabled("Leave seed empty for random");

        ImGui::Separator();

        int mode_int = static_cast<int>(mode);
        ImGui::TextUnformatted("Generation mode");
        ImGui::RadioButton("Normal", &mode_int, static_cast<int>(Mode::Normal));
        ImGui::SameLine();
        ImGui::RadioButton("Flat", &mode_int, static_cast<int>(Mode::Flat));
        ImGui::SameLine();
        ImGui::RadioButton("Dungeon Shooter", &mode_int, static_cast<int>(Mode::DungeonShooter));
        mode = static_cast<Mode>(mode_int);

        ImGui::Separator();

        if (mode == Mode::Normal)
        {
            ImGui::TextUnformatted("Terrain parameters");
            ImGui::SliderFloat("Base height",  &base_height,  0.0f,  200.0f);
            ImGui::SliderFloat("Height scale", &height_scale, 0.0f,  100.0f);
            ImGui::SliderFloat("Frequency",    &frequency,    0.001f, 0.2f, "%.4f");
            ImGui::SliderFloat("Amplitude",    &amplitude,    0.0f,  2.0f);
            ImGui::SliderFloat("Persistence",  &persistence,  0.0f,  1.0f);
        }
        else if (mode == Mode::DungeonShooter)
        {
            ImGui::TextUnformatted("Infinite cobblestone dungeon generated from the world seed.");
        }
        else
        {
            ImGui::TextUnformatted("Flat layers (top first)");

            if (ImGui::Button("Reset to defaults"))
            {
                flat_layers = FlatWorldProperties::defaults().layers;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add layer"))
            {
                flat_layers.push_back({BlockID::Stone, 1});
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
            }
            if (move_up_index >= 0 && move_up_index + 1 < static_cast<int>(flat_layers.size()))
            {
                std::swap(flat_layers[move_up_index], flat_layers[move_up_index + 1]);
            }
            if (move_down_index > 0 && move_down_index < static_cast<int>(flat_layers.size()))
            {
                std::swap(flat_layers[move_down_index], flat_layers[move_down_index - 1]);
            }

            int total = 0;
            for (const FlatWorldLayer& layer : flat_layers) total += layer.count;
            ImGui::Text("Total height: %d / %d", total, CHUNK_HEIGHT);
        }

        ImGui::Separator();

        bool can_create = name_buffer[0] != '\0';
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
