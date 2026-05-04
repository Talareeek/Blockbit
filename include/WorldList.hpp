#ifndef WORLD_LIST_HPP
#define WORLD_LIST_HPP

#include "UIElement.hpp"
#include "Button.hpp"
#include "Game.hpp"

#include <iostream>
#include <filesystem>

class WorldList : public UIElement
{
private:

    Game* game;

    std::filesystem::path path;

    std::vector<std::filesystem::path> worldPaths;

    std::vector<Button> buttons;


    enum class Mode
    {
        HIDDEN,
        ANIMATION,
        VISIBLE
    } mode = Mode::HIDDEN;

    enum class AnimationDirection
    {
        LEFT,
        RIGHT
    } direction;

    static constexpr float ANIMATION_TOTAL_LENGTH = 0.2f;

    float animation_time = 0.0f;

    enum class Selection
    {
        SINGLEPLAYER,
        MULTIPLAYER
    } selection = Selection::SINGLEPLAYER;

public:

    WorldList() = default;
    WorldList(std::filesystem::path path, Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // WORLD_LIST_HPP