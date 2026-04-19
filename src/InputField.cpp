#include "../include/InputField.hpp"
#include "../include/AssetManager.hpp"

InputField::InputField() : UIElement({0.0f, 0.0f}, {100.0f, 50.0f})
{

}

InputField::InputField(UIElement&& ui, std::string text, std::string placeholder) : UIElement(ui)
{
    this->text = text;
    this->placeholder = placeholder;
}

void InputField::handleEvent(const sf::Event& event)
{
    if (event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();

        sf::Vector2f mousepos = {static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)};

        sf::FloatRect box = {position, size};

        focused = box.contains(mousepos);
    }

    if (focused && event.is<sf::Event::TextEntered>())
    {
        auto text = event.getIf<sf::Event::TextEntered>();

        if (text->unicode == 8)
        {
            if (!(this->text.empty()))
                this->text.pop_back();
        }
        else if (text->unicode < 128)
        {
            this->text += static_cast<char>(text->unicode);
        }
    }
}

void InputField::update(float dt)
{
    
}

void InputField::render(sf::RenderWindow& window)
{
    sf::Vector2f borderless_size = size - sf::Vector2f{4.0f, 4.0f};
    sf::Vector2f borderless_position = position + sf::Vector2f{2.0f, 0.0f};

    sf::RectangleShape background(borderless_size);
    background.setPosition(borderless_position);

    background.setOutlineColor(sf::Color::Black);
    background.setOutlineThickness(2.0f);

    sf::Text text(AssetManager::getFont(0), this->text, borderless_size.y * 0.9f);

    text.setPosition(borderless_position + sf::Vector2f{borderless_size.x * 0.05f, borderless_size.y * 0.05f});

    while(text.getLocalBounds().size.x > borderless_size.x * 0.9f)
    {
        auto content = text.getString();

        content = content.substring(1);

        text.setString(content);
    }

    text.setFillColor(sf::Color::White);
    text.setOutlineThickness(0.5f);
    text.setOutlineColor(sf::Color::Black);

    window.draw(background);
    window.draw(text);
}

std::string InputField::getText() const
{
    return text;
}