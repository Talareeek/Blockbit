#include "../include/Button.hpp"

#include "../include/AssetManager.hpp"

Button::Button() : UIElement({0.0f, 0.0f}, {100.0f, 50.0f})
{
    this->text = "Button";
    this->color = sf::Color::Blue;
    this->onClick = [](){};
}

Button::Button(const sf::Vector2f& position, const sf::Vector2f& size, sf::Color color, const std::string& text, std::function<void()> onClick) : UIElement(position, size)
{
    this->text = text;
    this->color = color;
    this->onClick = std::move(onClick);
}

Button::Button(const UIElement::ScreenRelative& screenRelative, sf::Color color, const std::string& text, std::function<void()> onClick) : UIElement(screenRelative)
{
    this->text = text;
    this->color = color;
    this->onClick = std::move(onClick);
}

void Button::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseButtonPressed>())
    {
        if(event.getIf<sf::Event::MouseButtonPressed>()->button == sf::Mouse::Button::Left)
        {
            sf::IntRect buttonRect({{static_cast<int>(position.x), static_cast<int>(position.y)}, {static_cast<int>(size.x), static_cast<int>(size.y)}});

            sf::Vector2i mousePos = event.getIf<sf::Event::MouseButtonPressed>()->position;

            if(buttonRect.contains(mousePos))
            {
                onClick();
                
                Clicked = true;
            }
        }
    }
    else if (event.getIf<sf::Event::MouseMoved>())
    {
        sf::IntRect buttonRect({{static_cast<int>(position.x), static_cast<int>(position.y)}, {static_cast<int>(size.x), static_cast<int>(size.y)}});

        auto a = cursor.createFromSystem(sf::Cursor::Type::Arrow);

        sf::Vector2i mousePos = event.getIf<sf::Event::MouseMoved>()->position;

        cursorHovering = buttonRect.contains(mousePos);
    }
}

void Button::update(float dt)
{
    Clicked = false;
}

void Button::render(sf::RenderWindow& window)
{
    sf::RectangleShape button(size);
    button.setPosition(position);
    button.setFillColor(color);

    button.setOutlineThickness(size.y * 0.1f);
    sf::Color outlineColor
    (
        std::min(255, static_cast<int>(color.r) + 50),
        std::min(255, static_cast<int>(color.g) + 50),
        std::min(255, static_cast<int>(color.b) + 50)
    );

    button.setOutlineColor(outlineColor);
    window.draw(button);
    
    sf::Text textObj(AssetManager::getFont(0), text, size.y / 2);
    textObj.setScale({size.x / textObj.getLocalBounds().size.x, size.x / textObj.getLocalBounds().size.x});
    textObj.setPosition({position.x + (size.x - textObj.getLocalBounds().size.x) / 2, position.y + (size.y - textObj.getLocalBounds().size.y) / 2});
    textObj.setFillColor(sf::Color::White);
    textObj.setOutlineThickness(2.0f);
    textObj.setOutlineColor(sf::Color::Black);
    window.draw(textObj);
}


void Button::setText(std::string text)
{
    this->text = text;
}

bool Button::clicked()
{
    return Clicked;
}