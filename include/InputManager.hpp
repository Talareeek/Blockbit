#ifndef INPUT_MANAGER_HPP
#define INPUT_MANAGER_HPP

#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <chrono>

class InputManager
{
private:

    static std::unordered_map<sf::Keyboard::Key, std::chrono::steady_clock::time_point> lazyKeysLastPressedTime;

public:

    constexpr static std::chrono::milliseconds LAZY_KEY_COOLDOWN{200};

    static bool isLazyKeyPressed(sf::Keyboard::Key key);

    static bool isKeyPressed(sf::Keyboard::Key key);
};

#endif // INPUT_MANAGER_HPP