#ifndef INPUT_FIELD_HPP
#define INPUT_FIELD_HPP

#include <string>

#include "../include/UIElement.hpp"

class InputField : public UIElement
{
private:

    std::string text;
    std::string placeholder;
    bool focused = false;
    
public:

    using UIElement::UIElement;

    InputField();
    InputField(UIElement&& ui, std::string text, std::string placeholder = "");

    std::string getText() const;


    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // INPUT_FIELD_HPP