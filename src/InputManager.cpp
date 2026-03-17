#include "../include/InputManager.hpp"

std::unordered_map<sf::Keyboard::Key, std::chrono::steady_clock::time_point> InputManager::lazyKeysLastPressedTime;

bool InputManager::isLazyKeyPressed(sf::Keyboard::Key key)
{
    auto it = lazyKeysLastPressedTime.find(key);
    if (it != lazyKeysLastPressedTime.end())
    {
        std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
        std::chrono::milliseconds timeSinceLastPress = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - it->second);

        if (!(timeSinceLastPress < LAZY_KEY_COOLDOWN))
        {
            it->second = currentTime;
            return true;
        }
    }
    return false;
}

bool InputManager::isKeyPressed(sf::Keyboard::Key key)
{
    return sf::Keyboard::isKeyPressed(key);
}