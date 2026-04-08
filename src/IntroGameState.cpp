#include "../include/IntroGameState.hpp"

#include "../include/MenuGameState.hpp"

#include "../include/Game.hpp"

void IntroGameState::handleEvent(const sf::Event& event)
{

}


void IntroGameState::update(float dt)
{
    timer += dt;

    if(timer >= animationTime)
    {
        Game* temp_game = this->game;

        temp_game->popState();
        temp_game->pushState(std::make_unique<MenuGameState>(temp_game));
    }
}

void IntroGameState::render(sf::RenderWindow& window)
{
    sf::Texture texture;

    if(!texture.loadFromFile("resources/textures/intro.png")) throw std::runtime_error("Cannot access intro.png file");


    sf::RectangleShape background({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});

    background.setPosition({0.0f, 0.0f});

    background.setFillColor(sf::Color::Black);

    window.draw(background);


    sf::Sprite intro(texture);

    float scale = static_cast<float>(window.getSize().y) / static_cast<float>(texture.getSize().y);
    intro.setScale({scale, scale});

    float scaledWidth = texture.getSize().x * scale;
    intro.setPosition(
        {
            (static_cast<float>(window.getSize().x) - scaledWidth) / 2.0f,
            0.0f
        }
    );

    sf::Color introColor = sf::Color::White;
    
    if(timer <= 1.0f)
    {
        float factor = timer / 1.0f;
        introColor.a = static_cast<uint32_t>(255.0f * factor);
    }
    else
    {
        introColor.a = 255;
    }
    
    intro.setColor(introColor);
    window.draw(intro);

    sf::RectangleShape overlay({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});

    overlay.setPosition({0.0f, 0.0f});

    sf::Color color = sf::Color::Black;

    if(timer <= 1.0f)
    {
        float factor = 1.0f - (timer / 1.0f);
        color.a = static_cast<uint32_t>(255.0f * factor);
    }
    else
    {
        color.a = 0;
    }

    overlay.setFillColor(color);

    window.draw(overlay);
}