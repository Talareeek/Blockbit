#ifndef CHAT_UI_HPP
#define CHAT_UI_HPP

#include "UIElement.hpp"
#include "Chat.hpp"

#include <imgui.h>

#include <string>
#include <functional>
#include <vector>

class ChatUI : public UIElement
{
public:

    struct ColoredSegment
    {
        std::string text;
        ImVec4 color;
    };

    static std::vector<ColoredSegment> parseBBCodeColors(const std::string& utf8, const ImVec4& defaultColor);

private:

    const Chat* chat = nullptr;
    std::function<void(std::wstring)> onSend;

    bool active = false;
    bool focusInputNextFrame = false;

    static constexpr std::size_t INPUT_BUFFER_SIZE = 512;
    char inputBuffer[INPUT_BUFFER_SIZE] = {0};

    float fadeTimer = 0.0f;
    std::size_t lastMessageCount = 0;

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
