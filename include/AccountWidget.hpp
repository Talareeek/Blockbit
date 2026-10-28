#ifndef ACCOUNT_WIDGET_HPP
#define ACCOUNT_WIDGET_HPP

#include "UIElement.hpp"
#include "Account.hpp"

#include <SFML/System/Vector2.hpp>

class AccountWidget
{
private:

    char input[16] = {};

    bool editing = false;

public:

    void performImGui(sf::Vector2f position, sf::Vector2f size);
};

#endif // ACCOUNT_WIDGET_HPP