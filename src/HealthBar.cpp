#include "../include/HealthBar.hpp"

HealthBar::HealthBar(HealthComponent* health) : UIElement({0.0f, 0.0f}, {0.0f, 0.0f})
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
    position = {
        window.getSize().x * 0.05f,
        window.getSize().y * 0.05f
    };

    size = {
        window.getSize().x * 0.2f,
        window.getSize().y * 0.03f
    };

    float hp = static_cast<float>(health->health);
    float maxHp = static_cast<float>(health->maxHealth);

    if (maxHp <= 0.f) maxHp = 1.f;

    float ratio = hp / maxHp;
    ratio = std::max(0.f, std::min(ratio, 1.f));

    float padding = size.y * 0.05f;

    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setFillColor(sf::Color(72, 72, 72, 128));
    background.setOutlineColor(sf::Color(72, 72, 72));
    background.setOutlineThickness(padding);
    window.draw(background);

    sf::Vector2f innerSize = {
        size.x - 2.f * padding,
        size.y - 2.f * padding
    };

    sf::RectangleShape bar(innerSize);
    bar.setPosition(position + sf::Vector2f(padding, padding));

    bar.setSize({ innerSize.x * ratio, innerSize.y });

    bar.setFillColor(sf::Color::Red);
    window.draw(bar);
}

void HealthBar::setHealth(HealthComponent* health)
{
    this->health = health;
}