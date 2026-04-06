#ifndef WORLD_LIST_HPP
#define WORLD_LIST_HPP

#include "UIElement.hpp"
#include "Button.hpp"

#include <iostream>
#include <filesystem>

class WorldList : public UIElement
{
private:

    std::filesystem::path path;

    std::vector<std::filesystem::path> worldPaths;

    std::vector<Button> buttons;


    bool visible = false;

    Button hide{{0.0f, 0.0f}, {0.0f, 0.0f}, sf::Color::Blue, ">"};

public:

    WorldList();
    WorldList(std::filesystem::path path);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void setVisible(bool visible);
    bool getVisible();
};

#endif // WORLD_LIST_HPP