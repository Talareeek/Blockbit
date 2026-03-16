#ifndef ACCOUNT_WIDGET_HPP
#define ACCOUNT_WIDGET_HPP

#include "UIElement.hpp"
#include "Account.hpp"

class AccountWidget : public UIElement
{
private:

    Account* account = nullptr;

    bool hovered = false;

public:

    AccountWidget();
    AccountWidget(Account* account);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // ACCOUNT_WIDGET_HPP