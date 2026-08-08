#include "../include/ChatUI.hpp"
#include "../include/AssetManager.hpp"

#include <algorithm>
#include <utility>

void ChatUI::assignChat(const Chat* c)
{
    chat = c;
    lastMessageCount = c ? c->getMessages().size() : 0;
}

void ChatUI::setOnSend(std::function<void(std::wstring)> fn)
{
    onSend = std::move(fn);
}

void ChatUI::open()
{
    active = true;
    justOpened = true;
    cursorTimer = 0.0f;
    incoming.clear();
}

void ChatUI::close()
{
    active = false;
    incoming.clear();
}

void ChatUI::sendMessage()
{
    if(incoming.empty()) return;
    if(!onSend) return;

    onSend(std::move(incoming));

    incoming.clear();
}

void ChatUI::handleEvent(const sf::Event& event)
{
    if(!active) return;

    if(auto key = event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code == sf::Keyboard::Key::Escape)
        {
            close();
            return;
        }
    }

    if(auto text = event.getIf<sf::Event::TextEntered>())
    {
        if(justOpened)
        {
            justOpened = false;
            return;
        }

        if(text->unicode == L'\r' || text->unicode == L'\n')
        {
            sendMessage();
            close();
            return;
        }

        if(text->unicode == 8)
        {
            if(!incoming.empty()) incoming.pop_back();
            return;
        }

        if(text->unicode >= 32)
        {
            incoming.push_back(static_cast<wchar_t>(text->unicode));
        }
    }
}

void ChatUI::update(float dt)
{
    if(chat)
    {
        std::size_t current = chat->getMessages().size();
        if(current != lastMessageCount)
        {
            fadeTimer = FADE_DURATION;
            lastMessageCount = current;
        }
    }

    if(fadeTimer > 0.0f)
    {
        fadeTimer -= dt;
        if(fadeTimer < 0.0f) fadeTimer = 0.0f;
    }

    if(active) cursorTimer += dt;
}

void ChatUI::render(sf::RenderWindow& window)
{
    if(active)
    {
        sf::RectangleShape background({WIDTH * static_cast<float>(window.getSize().x), HEIGHT * static_cast<float>(window.getSize().y)});
        background.setFillColor(sf::Color(0, 0, 0, 200));
        background.setPosition({0.0f, static_cast<float>(window.getSize().y) - background.getLocalBounds().size.y});

        window.draw(background);
    }

    if(!chat) return;

    const auto& messages = chat->getMessages();

    bool showMessages = active || fadeTimer > 0.0f;
    if(!showMessages && messages.empty()) return;

    sf::Vector2u windowSize = window.getSize();

    const float padding = 6.0f;
    const unsigned int fontSize = 16;
    const float lineHeight = static_cast<float>(fontSize) + 4.0f;
    const float chatWidth = std::min(600.0f, windowSize.x * 0.45f);
    const float inputHeight = static_cast<float>(fontSize) + 10.0f;

    float inputTop = windowSize.y - padding - inputHeight - 40.0f;
    float logBottom = inputTop - padding;

    std::uint8_t alpha = 255;
    if(!active)
    {
        if(fadeTimer <= 0.0f) alpha = 0;
        else if(fadeTimer < 1.0f) alpha = static_cast<std::uint8_t>(fadeTimer * 255.0f);
    }

    if(alpha > 0)
    {
        float y = logBottom;
        std::size_t drawn = 0;

        for(auto it = messages.begin(); it != messages.end() && drawn < VISIBLE_MESSAGES; ++it, ++drawn)
        {
            sf::Text line(AssetManager::getFont(AssetManager::FontID::PressStart2P), *it, fontSize);
            auto bounds = line.getLocalBounds();

            float lineW = bounds.size.x + padding * 2.0f;

            sf::RectangleShape bg({std::max(chatWidth, lineW), lineHeight});
            bg.setPosition({padding, y - lineHeight});
            bg.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(alpha * 120 / 255)));
            window.draw(bg);

            line.setFillColor(sf::Color(255, 255, 255, alpha));
            line.setOutlineColor(sf::Color(0, 0, 0, alpha));
            line.setOutlineThickness(1.0f);
            line.setOrigin(bounds.position);
            line.setPosition({padding * 2.0f, y - lineHeight + (lineHeight - bounds.size.y) * 0.5f});
            window.draw(line);

            y -= lineHeight;

            if(y < padding) break;
        }
    }

    if(!active) return;

    sf::RectangleShape inputBg({chatWidth, inputHeight});
    inputBg.setPosition({padding, inputTop});
    inputBg.setFillColor(sf::Color(0, 0, 0, 180));
    inputBg.setOutlineColor(sf::Color(80, 200, 220));
    inputBg.setOutlineThickness(1.0f);
    window.draw(inputBg);

    sf::Text prompt(AssetManager::getFont(AssetManager::FontID::PressStart2P), incoming, fontSize);
    auto promptBounds = prompt.getLocalBounds();
    prompt.setFillColor(sf::Color::White);
    prompt.setOutlineColor(sf::Color::Black);
    prompt.setOutlineThickness(1.0f);
    prompt.setOrigin(promptBounds.position);
    prompt.setPosition({padding + 6.0f, inputTop + (inputHeight - promptBounds.size.y) * 0.5f});
    window.draw(prompt);

    if(static_cast<int>(cursorTimer * 2.0f) % 2 == 0)
    {
        sf::RectangleShape cursor({2.0f, static_cast<float>(fontSize)});
        cursor.setFillColor(sf::Color::White);
        cursor.setPosition({padding + 6.0f + promptBounds.size.x + 2.0f, inputTop + (inputHeight - static_cast<float>(fontSize)) * 0.5f});
        window.draw(cursor);
    }
}
