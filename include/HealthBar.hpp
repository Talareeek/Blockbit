#ifndef HEALTH_BAR_HPP
#define HEALTH_BAR_HPP

#include "UIElement.hpp"
#include "HealthComponent.hpp"

class HealthBar : public UIElement
{
private:

    HealthComponent* health;

public:

    HealthBar() = default;

    HealthBar(HealthComponent* health);

    void handleEvent(const sf::Event& event) override;

    void update(float dt) override;

    void render(sf::RenderWindow& window) override;

    void setHealth(HealthComponent* health);
};

#endif // HEALTH_BAR_HPP