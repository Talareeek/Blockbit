#include "../include/WorldList.hpp"
#include "../include/Button.hpp"
#include <fstream>

WorldList::WorldList() : UIElement({0.0f, 0.0f}, {300.0f, 400.0f})
{

}

WorldList::WorldList(std::filesystem::path path) : path(path), UIElement({0.0f, 0.0f}, {300.0f, 400.0f})
{
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if(entry.is_directory())
        {
            std::filesystem::path manifest = entry.path() / "manifest";
            if(std::filesystem::exists(manifest))
            {
                std::ifstream file(manifest);

                std::string name;
                std::getline(file, name);

                worldPaths.push_back(entry.path());

                buttons.emplace_back(
                    sf::Vector2f(position.x * 1.05f , position.y + worldPaths.size() * size.x * 0.9f * 0.3), 
                    sf::Vector2f(size.x * 0.9f, size.x * 0.9f * 0.25), 
                    sf::Color::Green, 
                    name
                );
            }
        }
    }
}

void WorldList::handleEvent(const sf::Event& event)
{
    for(auto& button : buttons)
    {
        button.handleEvent(event);
    }

    hide.handleEvent(event);
}

void WorldList::update(float dt)
{
    if(hide.clicked()) visible = !visible;

    for(int i = 0; i < buttons.size(); i++)
    {
        buttons[i].update(dt);

        buttons[i].setPosition(sf::Vector2f(position.x + size.x * 0.05f, position.y + i * size.x * 0.9f * 0.25 + i * size.x * 0.9f * 0.05));
        buttons[i].setSize(sf::Vector2f(size.x * 0.9f, size.x * 0.9f * 0.25));
    }

    hide.setSize({size.x / 20.0f, size.y / 10.0f});
    hide.update(dt);

    if(visible)
    {
        hide.setPosition({position.x - size.x / 20.0f, size.y * 0.45f});
        hide.setText(">");
    }
    else
    {
        hide.setPosition({position.x + size.x - size.x / 20.0f, size.y * 0.45f});
        hide.setText("<");
    }
}

void WorldList::render(sf::RenderWindow& window)
{
    if(visible)
    {
        sf::RectangleShape background(size);
        background.setPosition(position);

        background.setFillColor(sf::Color::Blue);

        window.draw(background);

        for(auto& button : buttons)
        {
            sf::FloatRect buttonBounds = {button.getPosition(), button.getSize()};
            sf::FloatRect thisBounds = {position, size};

            if(buttonBounds.findIntersection(thisBounds))
            {
                button.render(window);
            }
        }
    }

    hide.render(window);
}

void WorldList::setVisible(bool visible)
{
    this->visible = visible;
}

bool WorldList::getVisible()
{
    return visible;
}