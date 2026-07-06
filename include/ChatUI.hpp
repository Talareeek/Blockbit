#ifndef CHAT_UI_HPP
#define CHAT_UI_HPP

#include "UIElement.hpp"
#include "Chat.hpp"

#include <string>
#include <functional>

class ChatUI : public UIElement
{
private:

    const Chat* chat = nullptr;
    std::function<void(std::wstring)> onSend;

    bool active = false;
    bool justOpened = false;

    std::wstring incoming;

    float cursorTimer = 0.0f;

    float fadeTimer = 0.0f;

    std::size_t lastMessageCount = 0;

    void sendMessage();

public:

    using UIElement::UIElement;

    void assignChat(const Chat* chat);
    void setOnSend(std::function<void(std::wstring)> fn);

    bool isActive() const { return active; }

    void open();
    void close();

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    static constexpr float FADE_DURATION = 10.0f;
    static constexpr std::size_t VISIBLE_MESSAGES = 10;
};

#endif // CHAT_UI_HPP
