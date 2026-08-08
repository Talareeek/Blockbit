#include "../include/InputField.hpp"
#include "../include/AssetManager.hpp"

#include <cmath>

namespace
{
    float character_pos_x(const sf::Text& text, std::size_t index)
    {
        const auto& glyphs = text.getShapedGlyphs();
        const float origin_x = text.getPosition().x;

        for (const auto& g : glyphs)
            if (g.cluster >= index)
                return origin_x + g.position.x;

        if (!glyphs.empty())
        {
            const auto& last = glyphs.back();
            return origin_x + last.position.x + last.glyph.advance;
        }

        return origin_x;
    }
}

InputField::InputField() : UIElement({0.0f, 0.0f}, {100.0f, 50.0f})
{

}

InputField::InputField(UIElement&& ui, std::string text, std::string placeholder) : UIElement(ui)
{
    this->text = text;
    this->placeholder = placeholder;
    this->cursor_pos = this->text.size();
}

void InputField::handleEvent(const sf::Event& event)
{
    if (event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();

        sf::Vector2f mousepos = {static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)};

        sf::FloatRect box = {position, size};

        bool was_focused = focused;
        focused = box.contains(mousepos);

        if (focused)
        {
            sf::Vector2f borderless_size = size - sf::Vector2f{4.0f, 4.0f};
            sf::Vector2f borderless_position = position + sf::Vector2f{2.0f, 2.0f};

            float pad_x = borderless_size.y * 0.25f;
            float text_x = borderless_position.x + pad_x;
            unsigned font_size = static_cast<unsigned>(borderless_size.y * 0.55f);

            if (view_start > this->text.size()) view_start = this->text.size();

            sf::Text probe(AssetManager::getFont(AssetManager::FontID::PressStart2P), this->text.substr(view_start), font_size);

            float click_x = mousepos.x - text_x;

            std::size_t best = view_start;
            float best_dist = std::abs(click_x);

            for (std::size_t i = view_start + 1; i <= this->text.size(); ++i)
            {
                float char_x = character_pos_x(probe, i - view_start);
                float dist = std::abs(char_x - click_x);

                if (dist < best_dist)
                {
                    best_dist = dist;
                    best = i;
                }
            }

            cursor_pos = best;

            cursor_timer = 0.0f;
            cursor_visible = true;
        }

        if (focused && !was_focused)
        {
            cursor_timer = 0.0f;
            cursor_visible = true;
        }
    }

    if (focused && event.is<sf::Event::KeyPressed>())
    {
        auto key = event.getIf<sf::Event::KeyPressed>();

        if (key->code == sf::Keyboard::Key::Left)
        {
            if (cursor_pos > 0) cursor_pos--;
            cursor_timer = 0.0f;
            cursor_visible = true;
        }
        else if (key->code == sf::Keyboard::Key::Right)
        {
            if (cursor_pos < text.size()) cursor_pos++;
            cursor_timer = 0.0f;
            cursor_visible = true;
        }
    }

    if (focused && event.is<sf::Event::TextEntered>())
    {
        auto text = event.getIf<sf::Event::TextEntered>();

        if (text->unicode == 8)
        {
            if (cursor_pos > 0)
            {
                this->text.erase(cursor_pos - 1, 1);
                cursor_pos--;
            }
        }
        else if (text->unicode >= 32 && text->unicode < 128)
        {
            this->text.insert(cursor_pos, 1, static_cast<char>(text->unicode));
            cursor_pos++;
        }

        cursor_timer = 0.0f;
        cursor_visible = true;
    }
}

void InputField::update(float dt)
{
    if (focused)
    {
        cursor_timer += dt;

        if (cursor_timer >= 0.53f)
        {
            cursor_visible = !cursor_visible;
            cursor_timer = 0.0f;
        }
    }
    else
    {
        cursor_visible = false;
        cursor_timer = 0.0f;
    }
}

void InputField::render(sf::RenderWindow& window)
{
    sf::Vector2f borderless_size = size - sf::Vector2f{4.0f, 4.0f};
    sf::Vector2f borderless_position = position + sf::Vector2f{2.0f, 2.0f};

    sf::RectangleShape background(borderless_size);
    background.setPosition(borderless_position);
    background.setFillColor(sf::Color::White);

    if (focused)
    {
        background.setOutlineColor(sf::Color(66, 133, 244));
        background.setOutlineThickness(2.0f);
    }
    else
    {
        background.setOutlineColor(sf::Color(160, 160, 160));
        background.setOutlineThickness(1.0f);
    }

    window.draw(background);

    float pad_x = borderless_size.y * 0.25f;
    float text_x = borderless_position.x + pad_x;
    float visible_width = borderless_size.x - pad_x * 2.0f;
    unsigned font_size = static_cast<unsigned>(borderless_size.y * 0.55f);

    if (cursor_pos > text.size()) cursor_pos = text.size();
    if (view_start > text.size()) view_start = text.size();
    if (view_start > cursor_pos) view_start = cursor_pos;

    if (!text.empty())
    {
        sf::Text measure(AssetManager::getFont(AssetManager::FontID::PressStart2P), text, font_size);

        while (view_start < cursor_pos &&
               character_pos_x(measure, cursor_pos) - character_pos_x(measure, view_start) > visible_width)
        {
            view_start++;
        }

        while (view_start > 0 &&
               character_pos_x(measure, text.size()) - character_pos_x(measure, view_start - 1) <= visible_width)
        {
            view_start--;
        }
    }
    else
    {
        view_start = 0;
    }

    bool show_placeholder = text.empty() && !placeholder.empty();
    std::string display = show_placeholder ? placeholder : text.substr(view_start);

    sf::Text text_obj(AssetManager::getFont(AssetManager::FontID::PressStart2P), display, font_size);
    text_obj.setFillColor(show_placeholder ? sf::Color(160, 160, 160) : sf::Color(20, 20, 20));

    while (text_obj.getLocalBounds().size.x > visible_width)
    {
        auto content = text_obj.getString();

        if (content.isEmpty()) break;

        content = content.substring(0, content.getSize() - 1);
        text_obj.setString(content);
    }

    float text_height = text_obj.getLocalBounds().size.y;
    float text_y = borderless_position.y + (borderless_size.y - text_height) * 0.5f - text_obj.getLocalBounds().position.y;

    text_obj.setPosition({text_x, text_y});

    window.draw(text_obj);

    if (focused && cursor_visible)
    {
        float cursor_x;

        if (show_placeholder || text.empty())
        {
            cursor_x = text_x;
        }
        else
        {
            cursor_x = character_pos_x(text_obj, cursor_pos - view_start);
        }

        float cursor_h = borderless_size.y * 0.65f;
        float cursor_y = borderless_position.y + (borderless_size.y - cursor_h) * 0.5f;

        sf::RectangleShape cursor({1.5f, cursor_h});
        cursor.setPosition({cursor_x, cursor_y});
        cursor.setFillColor(sf::Color(20, 20, 20));

        window.draw(cursor);
    }
}

std::string InputField::getText() const
{
    return text;
}
