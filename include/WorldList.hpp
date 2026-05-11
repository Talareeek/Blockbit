#ifndef WORLD_LIST_HPP
#define WORLD_LIST_HPP

#include "UIElement.hpp"
#include "Button.hpp"
#include "InputField.hpp"
#include "WorldEntry.hpp"
#include "Game.hpp"

#include <iostream>
#include <filesystem>

class WorldList : public UIElement
{
private:

    Game* game = nullptr;

    std::filesystem::path path;

    std::vector<WorldEntry> entries;

    InputField ipField;
    Button connectButton;

    float scroll_offset = 0.0f;
    float content_height = 0.0f;


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


    sf::FloatRect getListArea() const;
    sf::FloatRect getTabBarArea() const;

    void loadEntries();

public:

    WorldList() = default;
    WorldList(std::filesystem::path path, Game* game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // WORLD_LIST_HPP
