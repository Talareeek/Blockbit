#include "../include/GameRegistries.hpp"
#include "../include/PackManager.hpp"

GameRegistries& gameRegistries()
{
    static GameRegistries instance;
    return instance;
}

void registerVanillaContent()
{
    PackManager::loadAllContent();
}
