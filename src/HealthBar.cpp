#include "../include/HealthBar.hpp"

HealthBar::HealthBar() : UIElement({0.0f, 0.0f}, {100.0f, 20.0f}), hp(100), maxHp(100)
{

}

HealthBar::HealthBar(const sf::Vector2f& position, const sf::Vector2f& size) : UIElement(position, size)
{

}

void HealthBar::handleEvent(const sf::Event& event)
{

}

void HealthBar::update(float dt)
{
    
}

void HealthBar::render(sf::RenderWindow& window)
{
    sf::RectangleShape background(size);
    background.setPosition(position);

    background.setFillColor(sf::Color(72, 72, 72, 128));
    background.setOutlineColor(sf::Color(72, 72, 72));
    background.setOutlineThickness((1.0f/20.0f) * size.y);
    window.draw(background);

    sf::RectangleShape bar(size - sf::Vector2f((2.0f/20.0f) * size.y, (2.0f/20.0f) * size.y));
    bar.setPosition(position + sf::Vector2f((1.0f/20.0f) * size.y, (1.0f/20.0f) * size.y));
    bar.setSize(sf::Vector2f((static_cast<float>(hp) / static_cast<float>(maxHp)) * bar.getSize().x, bar.getSize().y));

    bar.setFillColor(sf::Color::Red);
    window.draw(bar);
}

void HealthBar::setHealth(const HealthComponent& health)
{
    hp = health.health;
    maxHp = health.maxHealth;
}