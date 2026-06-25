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

        sf::Vector2i mousePos = event.getIf<sf::Event::MouseMoved>()->position;

        hover = buttonRect.contains(mousePos);
    }
}

void Button::update(float dt)
{
    float targetScale = hover ? 1.05f : 1.0f;
    scale = scale + (targetScale - scale) * dt * 10.f;

    Clicked = false;
}

void Button::render(sf::RenderWindow& window)
{
    sf::Vector2f center = position + size / 2.0f;

    sf::RectangleShape button(size);
    button.setOrigin(size / 2.0f);
    button.setPosition(center);
    button.setFillColor(color);

    button.setOutlineThickness(size.y * 0.1f);
    sf::Color outlineColor
    (
        std::min(255, static_cast<int>(color.r) + 50),
        std::min(255, static_cast<int>(color.g) + 50),
        std::min(255, static_cast<int>(color.b) + 50)
    );

    button.setOutlineColor(outlineColor);
    button.setScale({scale, scale});
    window.draw(button);



    sf::Text textObj(AssetManager::getFont(0), text, static_cast<unsigned int>(size.y * 0.6f));

    sf::FloatRect bounds = textObj.getLocalBounds();
    float text_scale = 1.0f;

    if(bounds.size.x > 0 && bounds.size.y > 0)
    {
        float scaleX = (size.x * 0.9f) / bounds.size.x;
        float scaleY = (size.y * 0.7f) / bounds.size.y;
        text_scale = std::min(scaleX, scaleY);
    }

    textObj.setOrigin({bounds.position.x + bounds.size.x / 2.0f, bounds.position.y + bounds.size.y / 2.0f});
    textObj.setPosition(center);
    textObj.setScale({text_scale * scale, text_scale * scale});
    textObj.setFillColor(sf::Color::White);
    textObj.setOutlineThickness(1.0f);
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