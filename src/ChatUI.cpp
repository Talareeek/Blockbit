#include "../include/ChatUI.hpp"
#include "../include/GameCommon.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
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

    std::size_t utf8CharLen(unsigned char lead)
    {
        if((lead & 0x80) == 0x00) return 1;
        if((lead & 0xE0) == 0xC0) return 2;
        if((lead & 0xF0) == 0xE0) return 3;
        if((lead & 0xF8) == 0xF0) return 4;
        return 1;
    }

    ImVec4 rainbowColor(std::size_t idx, float alpha)
    {
        float hue = static_cast<float>((idx * 25) % 360) / 360.0f;
        float r = 0.0f, g = 0.0f, b = 0.0f;
        ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, r, g, b);
        return ImVec4(r, g, b, alpha);
    }

    char secretGlyph(std::size_t idx, int bucket)
    {
        static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*?";
        constexpr std::size_t charsetSize = sizeof(charset) - 1;

        std::uint32_t h = static_cast<std::uint32_t>(idx) * 2654435761u;
        h ^= static_cast<std::uint32_t>(bucket) * 0x9E3779B1u;
        h = (h ^ (h >> 16)) * 0x85EBCA6Bu;
        h = (h ^ (h >> 13)) * 0xC2B2AE35u;
        h ^= h >> 16;
        return charset[h % charsetSize];
    }
}

std::vector<ChatUI::StyledSegment> ChatUI::parseBBCode(const std::string& utf8, const ImVec4& defaultColor)
{
    struct Style
    {
        ImVec4 color;
        bool bold = false;
        bool rainbow = false;
        bool secret = false;
    };

    std::vector<StyledSegment> out;

    std::vector<Style> stack;
    stack.push_back({defaultColor, false, false, false});

    std::string current;
    auto flush = [&]()
    {
        if(current.empty()) return;
        const Style& s = stack.back();
        out.push_back({std::move(current), s.color, s.bold, s.rainbow, s.secret});
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
        bool consumed = false;

        if(tag.size() >= 7 && tag.compare(0, 7, "color=#") == 0)
        {
            ImVec4 col;
            if(parseHexColor(tag.substr(7), col))
            {
                flush();
                Style s = stack.back();
                s.color = col;
                stack.push_back(s);
                consumed = true;
            }
        }
        else if(tag == "bold" || tag == "rainbow" || tag == "secret")
        {
            flush();
            Style s = stack.back();
            if(tag == "bold")    s.bold = true;
            if(tag == "rainbow") s.rainbow = true;
            if(tag == "secret")  s.secret = true;
            stack.push_back(s);
            consumed = true;
        }
        else if(tag == "/color" || tag == "/bold" || tag == "/rainbow" || tag == "/secret")
        {
            if(stack.size() > 1)
            {
                flush();
                stack.pop_back();
                consumed = true;
            }
        }

        if(consumed)
        {
            i = close + 1;
            continue;
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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, active ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0.0f);

    if(!ImGui::Begin("##Chat", nullptr, windowFlags))
    {
        ImGui::End();
        ImGui::PopStyleVar(5);
        return;
    }

    const float listHeight = active ? ImGui::GetContentRegionAvail().y - inputHeight : ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGuiWindowFlags childFlags = active ? ImGuiWindowFlags_HorizontalScrollbar : ImGuiWindowFlags_NoScrollbar;
    ImGui::BeginChild("##ChatMessages", ImVec2(0.0f, listHeight), false, childFlags);

    const ImVec4 defaultColor = ImVec4(1.0f, 1.0f, 1.0f, active ? 1.0f : alpha);

    const float wrapWidth = ImGui::GetContentRegionAvail().x;

    const int secretBucket = static_cast<int>(ImGui::GetTime() * 15.0);

    for(auto it = messages.rbegin(); it != messages.rend(); ++it)
    {
        std::string line = wstringToString(*it);

        auto segments = parseBBCode(line, defaultColor);
        if(segments.empty()) continue;

        const float startX = ImGui::GetCursorPosX();
        float cursorX = startX;
        bool firstOnLine = true;
        std::size_t rainbowIdx = 0;
        std::size_t secretIdx = 0;

        auto drawChunk = [&](const char* begin, const char* end, const ImVec4& col, bool bold)
        {
            if(!firstOnLine) ImGui::SameLine(0.0f, 0.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(begin, end);
            ImGui::PopStyleColor();

            if(bold)
            {
                ImVec2 rmin = ImGui::GetItemRectMin();
                ImGui::GetWindowDrawList()->AddText(ImVec2(rmin.x + 1.0f, rmin.y), ImGui::GetColorU32(col), begin, end);
            }

            ImVec2 size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, begin, end);
            cursorX += size.x;
            firstOnLine = false;
        };

        for(const auto& seg : segments)
        {
            if(seg.text.empty()) continue;

            ImVec4 baseCol = seg.color;
            if(!active) baseCol.w *= alpha;

            std::size_t pos = 0;
            while(pos < seg.text.size())
            {
                std::size_t nl = seg.text.find('\n', pos);
                std::size_t partEnd = (nl == std::string::npos) ? seg.text.size() : nl;

                if(seg.rainbow || seg.secret)
                {
                    std::size_t p = pos;
                    while(p < partEnd)
                    {
                        std::size_t clen = utf8CharLen(static_cast<unsigned char>(seg.text[p]));
                        if(p + clen > partEnd) clen = partEnd - p;

                        char buf[8];
                        const char* begin;
                        const char* end;

                        if(seg.secret)
                        {
                            buf[0] = secretGlyph(secretIdx, secretBucket);
                            begin = buf;
                            end = buf + 1;
                        }
                        else
                        {
                            begin = seg.text.data() + p;
                            end = begin + clen;
                        }

                        ImVec4 col = seg.rainbow ? rainbowColor(rainbowIdx, baseCol.w) : baseCol;

                        ImVec2 size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, begin, end);
                        float avail = wrapWidth - (cursorX - startX);
                        if(size.x > avail && !firstOnLine)
                        {
                            ImGui::NewLine();
                            cursorX = startX;
                            firstOnLine = true;
                        }

                        if(seg.text[p] != ' ' || !firstOnLine)
                        {
                            drawChunk(begin, end, col, seg.bold);
                        }

                        ++rainbowIdx;
                        ++secretIdx;
                        p += clen > 0 ? clen : 1;
                    }
                }
                else if(partEnd > pos)
                {
                    const char* strBegin = seg.text.data() + pos;
                    const char* strEnd = seg.text.data() + partEnd;

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

                        drawChunk(strBegin, chunkEnd, baseCol, seg.bold);

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

    ImGui::SetScrollHereY(1.0f);

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
    ImGui::PopStyleVar(5);
}
