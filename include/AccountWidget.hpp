#ifndef ACCOUNT_WIDGET_HPP
#define ACCOUNT_WIDGET_HPP

#include "UIElement.hpp"
#include "Account.hpp"

class AccountWidget
{
private:

    bool hovered = false;

    char input[16] = {};

    bool editing = false;

public:

    void performImGui();
};

#endif // ACCOUNT_WIDGET_HPP