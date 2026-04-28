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
                sound.setVolume(100);

                sound.play();

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
    
    sf::Text textObj(AssetManager::getFont(0), text, static_cast<unsigned int>(size.y * 0.6f));
    
    sf::FloatRect bounds = textObj.getLocalBounds();
    float scale = 1.0f;
    
    if(bounds.size.x > 0 && bounds.size.y > 0)
    {
        float scaleX = (size.x * 0.9f) / bounds.size.x;
        float scaleY = (size.y * 0.7f) / bounds.size.y;
        scale = std::min(scaleX, scaleY);
    }
    
    textObj.setScale({scale, scale});
    textObj.setFillColor(sf::Color::White);
    textObj.setOutlineThickness(1.0f);
    textObj.setOutlineColor(sf::Color::Black);
    
    bounds = textObj.getLocalBounds();
    textObj.setPosition({
        position.x + (size.x - bounds.size.x * scale) / 2.0f - bounds.position.x * scale,
        position.y + (size.y - bounds.size.y * scale) / 2.0f - bounds.position.y * scale
    });
    
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