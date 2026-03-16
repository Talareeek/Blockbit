#ifndef HEALTH_BAR_HPP
#define HEALTH_BAR_HPP

#include "UIElement.hpp"
#include "HealthComponent.hpp"

class HealthBar : public UIElement
{
private:

    uint32_t hp;
    uint32_t maxHp;

public:

    HealthBar();

    HealthBar(const sf::Vector2f& position, const sf::Vector2f& size);

    void handleEvent(const sf::Event& event) override;

    void update(float dt) override;

    void render(sf::RenderWindow& window) override;

    void setHealth(const HealthComponent& health);
};

#endif // HEALTH_BAR_HPP