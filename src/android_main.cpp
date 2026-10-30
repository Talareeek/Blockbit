// Android entry point.
//
// On Android, ANativeActivity_onCreate is provided by SFML::Main (whole-archive
// linked into libBlockbit.so) and ultimately dispatches into this main(). The
// desktop entry in main.cpp is excluded from the Android build by the
// if(ANDROID) branch in the top-level CMakeLists.txt.
//
// Boot path matches the no-argv flow of desktop main.cpp: push the
// AuthorGameState splash, which auto-transitions into MenuGameState. From
// there world list / host / join reach the same ClientGameState constructors
// used by the --load / --host / --join CLI switches.

#ifdef __ANDROID__

#include "../include/Game.hpp"
#include "../include/AuthorGameState.hpp"

#include <android/log.h>

#include <iostream>
#include <memory>

namespace
{
    constexpr const char* kLogTag = "Blockbit";
}

int main(int /*argc*/, char* /*argv*/[])
{
    __android_log_write(ANDROID_LOG_INFO, kLogTag, "main() entered");

    try
    {
        Game game;
        game.pushState(nullptr, std::make_unique<AuthorGameState>(&game));
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
