#include "../include/WorldList.hpp"
#include "../include/Button.hpp"
#include "../include/World.hpp"
#include "../include/AssetManager.hpp"
#include "../include/MainGameState.hpp"
#include <fstream>

WorldList::WorldList(std::filesystem::path path, Game* game) : path(path), game{game}
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
                    sf::Color(40, 40, 40), 
                    name,
                    [game = this->game, worldPath = entry.path()]()
                    {
                        game->pushState(std::make_unique<MainGameState>(game, World(worldPath)));
                    }
                );
            }
        }
    }
}

void WorldList::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseMoved>())
    {
        auto mouse = event.getIf<sf::Event::MouseMoved>();

        if(mode == WorldList::Mode::HIDDEN)
        {
            sf::FloatRect sidebar{{position.x + size.x - size.x / 10.0f, 0.0f}, {size.x / 10.0f, size.y}};

            if(sidebar.contains({static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)}))
            {
                mode = WorldList::Mode::ANIMATION;
                direction = WorldList::AnimationDirection::LEFT;
            }
        }
        else if(mode == WorldList::Mode::VISIBLE)
        {
            sf::FloatRect sidebar{position, size};

            if(!sidebar.contains({static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)}))
            {
                mode = WorldList::Mode::ANIMATION;
                direction = WorldList::AnimationDirection::RIGHT;
            }
        }
    }

    if(mode == WorldList::Mode::VISIBLE)
    {
        for(auto& button : buttons)
        {
            button.handleEvent(event);
        }
    }
}

void WorldList::update(float dt)
{
    if(mode == WorldList::Mode::ANIMATION)
    {
        animation_time += dt;
        if(animation_time >= ANIMATION_TOTAL_LENGTH)
        {
            animation_time = 0.0f;

            switch(direction)
            {
            case WorldList::AnimationDirection::LEFT:
                mode = WorldList::Mode::VISIBLE;
                break;

            case WorldList::AnimationDirection::RIGHT:
                mode = WorldList::Mode::HIDDEN;
                break;
            }
        }
    }

    for(int i = 0; i < buttons.size(); i++)
    {
        buttons[i].update(dt);

        buttons[i].setPosition(sf::Vector2f(position.x + size.x * 0.05f, position.y + i * size.x * 0.9f * 0.25 + i * size.x * 0.9f * 0.05));
        buttons[i].setSize(sf::Vector2f(size.x * 0.9f, size.x * 0.9f * 0.25));
    }
}

void WorldList::render(sf::RenderWindow& window)
{
    sf::Texture& background_texture = AssetManager::getTexture(18);
    background_texture.setRepeated(true);

    sf::Vector2u texSize = background_texture.getSize();

    auto makeTexRect = [&](sf::Vector2f areaSize)
    {
        return sf::IntRect({0, 0}, {static_cast<int>(areaSize.x / texSize.x * texSize.x), static_cast<int>(areaSize.y / texSize.y * texSize.y)});
    };

    if(mode == WorldList::Mode::HIDDEN)
    {
        sf::Vector2f sidebarSize = {size.x / 10.0f, size.y};

        sf::RectangleShape sidebar(sidebarSize);
        sidebar.setPosition({position.x + size.x - sidebarSize.x, 0.0f});
        sidebar.setTexture(&background_texture);
        sidebar.setTextureRect(makeTexRect(sidebarSize));

        window.draw(sidebar);
    }
    else if(mode == WorldList::Mode::ANIMATION)
    {
        float t = animation_time / ANIMATION_TOTAL_LENGTH;
        float width = 0.0f;

        if(direction == WorldList::AnimationDirection::LEFT)
        {
            width = size.x / 10.0f + t * (size.x * 0.9f);
        }
        else
        {
            width = size.x - t * (size.x * 0.9f);
        }

        sf::Vector2f animatedSize = {width, size.y};

        sf::RectangleShape animated_sidebar(animatedSize);
        animated_sidebar.setPosition({position.x + size.x - width, 0.0f});
        animated_sidebar.setTexture(&background_texture);
        animated_sidebar.setTextureRect(makeTexRect(animatedSize));

        window.draw(animated_sidebar);
    }
    else if(mode == WorldList::Mode::VISIBLE)
    {
        sf::RectangleShape background(size);
        background.setPosition(position);
        background.setTexture(&background_texture);
        background.setTextureRect(makeTexRect(size));

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
}