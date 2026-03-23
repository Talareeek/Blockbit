#include "../include/DeathScreenState.hpp"
#include "../include/Game.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/TransformComponent.hpp"

DeathScreenState::DeathScreenState(Game* game, World& world, uint32_t entityID) : GameState(game), world(world), entityID(entityID)
{
    respawn = Button{
        sf::Vector2f(300, 200),
        sf::Vector2f(200, 50),
        sf::Color::Red,
        "Respawn",
        [this]()
        {
            for(auto& entity : this->world.getEntities())
            {
                if(entity.getID() == this->entityID)
                {
                    entity.getComponent<HealthComponent>().health = entity.getComponent<HealthComponent>().maxHealth;
                    entity.getComponent<TransformComponent>().position = this->world.getSpawnPoint();
                    break;
                }
            }

            this->game->popState();
        }
    };
}

void DeathScreenState::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::Resized>())
    {

    }

    respawn.handleEvent(event);
}

void DeathScreenState::update(float dt)
{
    respawn.update(dt);    
}

void DeathScreenState::render(sf::RenderWindow& window)
{
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    sf::RectangleShape background({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    background.setPosition({0.0f, 0.0f});
    
    sf::Color bgColor = sf::Color::Red;
    bgColor.a = 100;
    background.setFillColor(bgColor);

    window.draw(background);

    respawn.render(window);
}