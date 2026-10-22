#include "../include/ChatUI.hpp"
#include "../include/GameCommon.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>

namespace
{
    bool parseHexColor(const std::string& hex, ImVec4& out)
    {
        if(hex.size() != 6 && hex.size() != 8) return false;

        for(char c : hex) if(!std::isxdigit(static_cast<unsigned char>(c))) return false;

        auto hexByte = [&](std::size_t offset) -> int
        {
            auto nibble = [](char c) -> int
            {
                if(c >= '0' && c <= '9') return c - '0';
                if(c >= 'a' && c <= 'f') return 10 + (c - 'a');
                if(c >= 'A' && c <= 'F') return 10 + (c - 'A');
                return 0;
            };
            return (nibble(hex[offset]) << 4) | nibble(hex[offset + 1]);
        };

        int r = hexByte(0);
        int g = hexByte(2);
        int b = hexByte(4);
        int a = (hex.size() == 8) ? hexByte(6) : 255;

        out = ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        return true;
    }
}

std::vector<ChatUI::ColoredSegment> ChatUI::parseBBCodeColors(const std::string& utf8, const ImVec4& defaultColor)
{
    std::vector<ColoredSegment> out;

    std::vector<ImVec4> stack;
    stack.push_back(defaultColor);

    std::string current;
    auto flush = [&]()
    {
        if(current.empty()) return;
        out.push_back({std::move(current), stack.back()});
        current.clear();
    };

    const std::size_t n = utf8.size();
    std::size_t i = 0;

    while(i < n)
    {
        if(utf8[i] != '[')
        {
            current.push_back(utf8[i]);
            ++i;
            continue;
        }

        std::size_t close = utf8.find(']', i + 1);
        if(close == std::string::npos)
        {
            current.push_back(utf8[i]);
            ++i;
            continue;
        }

        std::string tag = utf8.substr(i + 1, close - i - 1);

        if(tag.size() >= 7 && tag.compare(0, 7, "color=#") == 0)
        {
            std::string hex = tag.substr(7);
            ImVec4 col;
            if(parseHexColor(hex, col))
            {
                flush();
                stack.push_back(col);
                i = close + 1;
                continue;
            }
        }
        else if(tag == "/color")
        {
            if(stack.size() > 1)
            {
                flush();
                stack.pop_back();
                i = close + 1;
                continue;
            }
        }

        current.append(utf8, i, close - i + 1);
        i = close + 1;
    }

    flush();
    return out;
}

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
    focusInputNextFrame = true;
    inputBuffer[0] = '\0';
}

void ChatUI::close()
{
    active = false;
    inputBuffer[0] = '\0';
}

void ChatUI::handleEvent(const sf::Event& event)
{
    if(!active) return;

    if(auto key = event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code == sf::Keyboard::Key::Escape)
        {
            close();
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
}

void ChatUI::render(sf::RenderWindow& window)
{
    if(!chat) return;

    const auto& messages = chat->getMessages();

    bool showMessages = active || fadeTimer > 0.0f;
    if(!active && !showMessages && messages.empty()) return;

    float alpha = 1.0f;
    if(!active)
    {
        if(fadeTimer <= 0.0f) alpha = 0.0f;
        else if(fadeTimer < 1.0f) alpha = fadeTimer;
    }

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    const float panelWidth = std::min(600.0f, displaySize.x * 0.45f);
    const float inputHeight = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    const float panelHeight = std::min(displaySize.y * 0.5f, 320.0f);
    const float margin = 10.0f;

    const float panelX = margin;
    const float panelY = displaySize.y - panelHeight - margin - 40.0f;

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar
                                 | ImGuiWindowFlags_NoResize
                                 | ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoCollapse
                                 | ImGuiWindowFlags_NoSavedSettings
                                 | ImGuiWindowFlags_NoNav
                                 | ImGuiWindowFlags_NoScrollbar;

    if(!active) windowFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));
    ImGui::SetNextWindowBgAlpha(active ? 0.78f : 0.4f * alpha);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, active ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    if(!ImGui::Begin("##Chat", nullptr, windowFlags))
    {
        ImGui::End();
        ImGui::PopStyleVar(3);
        return;
    }

    const float listHeight = active ? ImGui::GetContentRegionAvail().y - inputHeight : ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("##ChatMessages", ImVec2(0.0f, listHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    const ImVec4 defaultColor = ImVec4(1.0f, 1.0f, 1.0f, active ? 1.0f : alpha);

    const float wrapWidth = ImGui::GetContentRegionAvail().x;

    for(auto it = messages.rbegin(); it != messages.rend(); ++it)
    {
        std::string line = wstringToString(*it);

        auto segments = parseBBCodeColors(line, defaultColor);
        if(segments.empty()) continue;

        const float startX = ImGui::GetCursorPosX();
        float cursorX = startX;
        bool firstOnLine = true;

        for(const auto& seg : segments)
        {
            if(seg.text.empty()) continue;

            ImVec4 col = seg.color;
            if(!active) col.w *= alpha;

            std::size_t pos = 0;
            while(pos < seg.text.size())
            {
                std::size_t nl = seg.text.find('\n', pos);
                std::string part = seg.text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);

                if(!part.empty())
                {
                    const char* strBegin = part.c_str();
                    const char* strEnd = strBegin + part.size();

                    while(strBegin < strEnd)
                    {
                        float avail = wrapWidth - (cursorX - startX);
                        if(avail < 1.0f)
                        {
                            ImGui::NewLine();
                            cursorX = startX;
                            firstOnLine = true;
                            avail = wrapWidth;
                        }

                        const char* chunkEnd = ImGui::GetFont()->CalcWordWrapPositionA(1.0f, strBegin, strEnd, avail);
                        if(chunkEnd == strBegin)
                        {
                            if(!firstOnLine)
                            {
                                ImGui::NewLine();
                                cursorX = startX;
                                firstOnLine = true;
                                continue;
                            }
                            chunkEnd = strEnd;
                        }

                        if(!firstOnLine) ImGui::SameLine(0.0f, 0.0f);

                        ImGui::PushStyleColor(ImGuiCol_Text, col);
                        ImGui::TextUnformatted(strBegin, chunkEnd);
                        ImGui::PopStyleColor();

                        ImVec2 size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, strBegin, chunkEnd);
                        cursorX += size.x;
                        firstOnLine = false;

                        strBegin = chunkEnd;
                        while(strBegin < strEnd && *strBegin == ' ') ++strBegin;
                    }
                }

                if(nl == std::string::npos) break;

                ImGui::NewLine();
                cursorX = startX;
                firstOnLine = true;
                pos = nl + 1;
            }
        }

        if(!firstOnLine) ImGui::NewLine();
    }

    if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    if(active)
    {
        if(focusInputNextFrame)
        {
            ImGui::SetKeyboardFocusHere();
            focusInputNextFrame = false;
        }

        ImGui::PushItemWidth(-FLT_MIN);
        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;
        bool entered = ImGui::InputText("##ChatInput", inputBuffer, INPUT_BUFFER_SIZE, inputFlags);
        ImGui::PopItemWidth();

        if(entered)
        {
            std::string utf8(inputBuffer);
            if(!utf8.empty() && onSend)
            {
                std::wstring wide(utf8.begin(), utf8.end());
                onSend(std::move(wide));
            }
            close();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
}
