// Android entry point.
//
// On Android, ANativeActivity_onCreate is provided by SFML::Main (whole-archive
// linked into libBlockbit.so) and ultimately dispatches into this main(). The
// desktop entry in main.cpp is excluded from the Android build by the
// if(ANDROID) branch in the top-level CMakeLists.txt.
//
// Phase 4: hardcoded singleplayer boot. Loads a save at
//   <internalDataPath>/Blockbit/saves/android_dev
// creating a flat world if it doesn't exist yet.

#ifdef __ANDROID__

#include "../include/Game.hpp"
#include "../include/ClientGameState.hpp"
#include "../include/AuthorGameState.hpp"
#include "../include/GameCommon.hpp"
#include "../include/World.hpp"

#include <filesystem>
#include <iostream>
#include <memory>

namespace
{
    constexpr const char* kWorldName = "android_dev";
    constexpr const char* kNickname  = "AndroidPlayer";
    constexpr unsigned int kSeed     = 1337u;
}

int main(int /*argc*/, char* /*argv*/[])
{
    try
    {
        Game game;

        const std::filesystem::path world_path = getWorldsPath() / kWorldName;

        if (std::filesystem::exists(world_path / "manifest"))
        {
            game.pushState(nullptr,
                std::make_unique<ClientGameState>(&game, world_path, 0, kNickname));
        }
        else
        {
            // Create a flat world the first time the app runs. Matches the
            // defaults from CreateWorldState so the generator behaves the same
            // as the desktop "Create world → Flat" flow.
            GenerationProperties properties;
            properties.type         = GenerationType::Flat;
            properties.base_height  = 55.0f;
            properties.height_scale = 35.0f;
            properties.frequency    = 0.035f;
            properties.amplitude    = 1.0f;
            properties.persistence  = 0.55f;

            std::filesystem::create_directories(world_path);

            game.pushState(nullptr,
                std::make_unique<ClientGameState>(
                    &game, std::string(kWorldName), kSeed, properties, 0, kNickname));
        }

        game.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[AndroidMain] FATAL: " << e.what() << '\n';
        return 1;
    }

    return 0;
}

#endif // __ANDROID__
