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
    this->onClick = onClick;
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

}

void Button::render(sf::RenderWindow& window)
{
    if(cursorHovering)
    {
        if(cursor.createFromSystem(sf::Cursor::Type::Hand))
        {
            window.setMouseCursor(cursor);
        }
    }
    else
    {
        if(cursor.createFromSystem(sf::Cursor::Type::Arrow))
        {
            window.setMouseCursor(cursor);
        }
    }

    sf::RectangleShape rect(size);
    rect.setPosition(position);
    rect.setFillColor(color);
    window.draw(rect);

    sf::RectangleShape outline(size - sf::Vector2f(10.0f, 10.0f));
    outline.setPosition(position + sf::Vector2f(5.0f, 5.0f));
    sf::Color outlineColor = color;
    outlineColor.r = 255; //std::min(255, static_cast<int>(outlineColor.r) + 50);
    outlineColor.g = 255; //std::min(255, static_cast<int>(outlineColor.g) + 50);
    outlineColor.b = 255; //std::min(255, static_cast<int>(outlineColor.b) + 50);
    outlineColor.a = 100; 
    outline.setFillColor(outlineColor);
    window.draw(outline);

    sf::Font font = AssetManager::getFont(0);
    sf::Text textObj(font, text, size.y / 2);
    textObj.setPosition({position.x + size.x / 2 - textObj.getLocalBounds().size.x / 2, position.y + size.y / 2 - textObj.getLocalBounds().size.y / 2});
    textObj.setFillColor(sf::Color::White);
    window.draw(textObj);
}