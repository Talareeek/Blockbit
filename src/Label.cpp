#include "../include/Label.hpp"
#include "../include/AssetManager.hpp"

void Label::setText(const std::string& text)
{
    this->text = text;
}

const std::string& Label::getText() const
{
    return this->text;
}

void Label::setColor(const sf::Color& color)
{
    this->color = color;
}

const sf::Color& Label::getColor() const
{
    return this->color;
}

void Label::handleEvent(const sf::Event& event)
{
    
}

void Label::update(float dt)
{
    
}

void Label::render(sf::RenderWindow& window)
{
    sf::Text text(AssetManager::getFont(1), this->text, size.y);
    text.setPosition(position);
    text.setFillColor(sf::Color::White);
    window.draw(text);
}

void WLabel::setText(const std::wstring& text)
{
    this->text = text;
}

const std::wstring& WLabel::getText() const
{
    return this->text;
}

void WLabel::setColor(const sf::Color& color)
{
    this->color = color;
}

const sf::Color& WLabel::getColor() const
{
    return this->color;
}

void WLabel::handleEvent(const sf::Event& event)
{
    
}

void WLabel::update(float dt)
{
    
}

void WLabel::render(sf::RenderWindow& window)
{
    sf::Text text(AssetManager::getFont(1), this->text, size.y);
    text.setPosition(position);
    text.setFillColor(sf::Color::White);
    window.draw(text);
}