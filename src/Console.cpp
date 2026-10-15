#include "../include/Console.hpp"
#include "../include/InputManager.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Command.hpp"
#include "../include/GameCommon.hpp"

#include <iostream>
#include <deque>
#include <algorithm>
#include <cctype>
#include <cstring>

#include <imgui.h>

namespace
{
    std::mutex& sinkMutex()
    {
        static std::mutex m;
        return m;
    }

    std::deque<ConsoleSink::Message>& sinkQueue()
    {
        static std::deque<ConsoleSink::Message> q;
        return q;
    }
}

void ConsoleSink::push(std::wstring text, sf::Color color)
{
    std::lock_guard<std::mutex> lock(sinkMutex());
    sinkQueue().push_back({std::move(text), color});
}

std::vector<ConsoleSink::Message> ConsoleSink::drain()
{
    std::lock_guard<std::mutex> lock(sinkMutex());
    std::vector<Message> out;
    out.reserve(sinkQueue().size());
    while(!sinkQueue().empty())
    {
        out.push_back(std::move(sinkQueue().front()));
        sinkQueue().pop_front();
    }
    return out;
}

void ConsoleSink::installRedirects()
{
    static ConsoleStreambuf coutBuf(std::cout.rdbuf(), sf::Color::White);
    static ConsoleStreambuf cerrBuf(std::cerr.rdbuf(), sf::Color(235, 160, 90));
    static ConsoleStreambuf clogBuf(std::clog.rdbuf(), sf::Color(200, 200, 200));

    std::cout.rdbuf(&coutBuf);
    std::cerr.rdbuf(&cerrBuf);
    std::clog.rdbuf(&clogBuf);
}

ConsoleStreambuf::ConsoleStreambuf(std::streambuf* passthrough, sf::Color color)
    : passthrough(passthrough), color(color)
{
}

ConsoleStreambuf::int_type ConsoleStreambuf::overflow(int_type c)
{
    if(c == traits_type::eof()) return traits_type::not_eof(c);

    char ch = static_cast<char>(c);

    if(passthrough) passthrough->sputc(ch);

    {
        std::lock_guard<std::mutex> lock(mutex);
        if(ch == '\n')
        {
            flushLine();
        }
        else if(ch != '\r')
        {
            buffer.push_back(ch);
        }
    }

    return c;
}

int ConsoleStreambuf::sync()
{
    if(passthrough) passthrough->pubsync();
    return 0;
}

void ConsoleStreambuf::flushLine()
{
    std::wstring wide;
    wide.reserve(buffer.size());
    for(unsigned char c : buffer) wide.push_back(static_cast<wchar_t>(c));
    ConsoleSink::push(std::move(wide), color);
    buffer.clear();
}

void Console::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::KeyPressed>())
    {
        auto key_event_data = event.getIf<sf::Event::KeyPressed>();

        if(key_event_data->code == sf::Keyboard::Key::Grave)
        {
            open = !open;
        }
    }
}

void Console::update(float dt)
{
    for(auto& msg : ConsoleSink::drain())
    {
        appendLog(std::move(msg.text), msg.color);
    }

    if(to_proceed.has_value())
    {
        std::wstringstream stream(to_proceed.value());   

        std::wstring command_name;

        stream >> command_name;

        if(!commandDatabase.contains(command_name))
        {
            std::cerr << "No command found: " << std::string(command_name.begin(), command_name.end()) << '\n';
            to_proceed = std::nullopt;
            return;
        }

        auto& command_data = commandDatabase[command_name];

        if((command_data.requires_game && game == nullptr) || (command_data.requires_world && world == nullptr))
        {
            std::cerr << "Can't call this command now" << '\n';
            to_proceed = std::nullopt;
            return;
        }

        commandDatabase[command_name].on_call(to_proceed.value(), (*this), game, world);

        to_proceed = std::nullopt;
    }
}

void Console::render()
{
    if(!open) return;

    ImGui::Begin("Console", &open, ImGuiWindowFlags_NoCollapse);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, SFMLColorToImGuiColor(sf::Color::Transparent));

    const float suggestionsHeight = (input[0] != '\0') ? ImGui::GetTextLineHeightWithSpacing() * 4.0f + ImGui::GetStyle().FramePadding.y * 2.0f : 0.0f;
    const float reservedBottom = ImGui::GetFrameHeightWithSpacing() + suggestionsHeight;

    ImGui::InputTextMultiline("##ConsoleLogs", logBuffer.data(), logBuffer.size() + 1, ImVec2(-FLT_MIN, -reservedBottom), ImGuiInputTextFlags_ReadOnly);

    ImGui::PopStyleColor();

    if(input[0] != '\0')
    {
        std::string current(input);
        size_t spacePos = current.find(' ');
        std::string prefix = (spacePos == std::string::npos) ? current : current.substr(0, spacePos);
        std::vector<std::string> matches = findMatches(prefix);

        ImGui::BeginChild("##Suggestions", ImVec2(0, suggestionsHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
        if(matches.empty())
        {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "no matching commands");
        }
        else
        {
            for(const auto& match : matches)
            {
                if(ImGui::Selectable(match.c_str()))
                {
                    std::string replacement = match;
                    if(spacePos != std::string::npos)
                    {
                        replacement += current.substr(spacePos);
                    }
                    else
                    {
                        replacement += " ";
                    }
                    std::strncpy(input, replacement.c_str(), INPUT_BUFFER_SIZE - 1);
                    input[INPUT_BUFFER_SIZE - 1] = '\0';
                    reclaimFocus = true;
                }
            }
        }
        ImGui::EndChild();
    }

    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
                                   | ImGuiInputTextFlags_CallbackHistory
                                   | ImGuiInputTextFlags_CallbackCompletion;

    if(reclaimFocus)
    {
        ImGui::SetKeyboardFocusHere();
        reclaimFocus = false;
    }

    bool enter_pressed = ImGui::InputText("##Input", input, INPUT_BUFFER_SIZE, inputFlags, &Console::inputTextCallbackStub, this);
    ImGui::SameLine();
    bool send_clicked = ImGui::Button("Send");

    if(enter_pressed || send_clicked)
    {
        submitInput();
        reclaimFocus = true;
    }

    ImGui::End();
}


void Console::writeLine(std::wstring string)
{
    appendLog(std::move(string), sf::Color::White);
}

void Console::writeLine(std::wstring string, sf::Color color)
{
    appendLog(std::move(string), color);
}

void Console::submitInput()
{
    if(std::strlen(input) == 0) return;

    std::string entered(input);

    to_proceed = std::wstring(input, input + std::strlen(input));

    for(auto it = history.begin(); it != history.end(); )
    {
        if(*it == entered) it = history.erase(it);
        else ++it;
    }
    history.push_back(entered);
    historyPos = -1;

    std::memset(input, 0, INPUT_BUFFER_SIZE);

    appendLog(L"::> " + to_proceed.value(), sf::Color(255, 255, 255, 127));
}

std::vector<std::string> Console::findMatches(const std::string& prefix) const
{
    std::vector<std::string> matches;
    for(const auto& [key, value] : commandDatabase)
    {
        std::string keyStr = wstringToString(key);
        if(keyStr.size() < prefix.size()) continue;

        bool match = true;
        for(size_t i = 0; i < prefix.size(); ++i)
        {
            if(std::tolower(static_cast<unsigned char>(keyStr[i])) !=
               std::tolower(static_cast<unsigned char>(prefix[i])))
            {
                match = false;
                break;
            }
        }
        if(match) matches.push_back(keyStr);
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

int Console::inputTextCallbackStub(ImGuiInputTextCallbackData* data)
{
    Console* self = static_cast<Console*>(data->UserData);
    return self->inputTextCallback(data);
}

int Console::inputTextCallback(ImGuiInputTextCallbackData* data)
{
    switch(data->EventFlag)
    {
        case ImGuiInputTextFlags_CallbackHistory:
        {
            if(history.empty()) break;

            const int prevPos = historyPos;
            if(data->EventKey == ImGuiKey_UpArrow)
            {
                if(historyPos == -1)
                    historyPos = static_cast<int>(history.size()) - 1;
                else if(historyPos > 0)
                    historyPos--;
            }
            else if(data->EventKey == ImGuiKey_DownArrow)
            {
                if(historyPos != -1)
                {
                    historyPos++;
                    if(historyPos >= static_cast<int>(history.size()))
                        historyPos = -1;
                }
            }

            if(prevPos != historyPos)
            {
                const char* historyStr = (historyPos >= 0) ? history[historyPos].c_str() : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, historyStr);
            }
            break;
        }
        case ImGuiInputTextFlags_CallbackCompletion:
        {
            std::string current(data->Buf, data->BufTextLen);
            size_t spacePos = current.find(' ');
            std::string prefix = (spacePos == std::string::npos) ? current : current.substr(0, spacePos);

            std::vector<std::string> matches = findMatches(prefix);
            if(matches.empty()) break;

            if(matches.size() == 1)
            {
                data->DeleteChars(0, static_cast<int>(prefix.size()));
                data->InsertChars(0, matches[0].c_str());
                if(spacePos == std::string::npos)
                    data->InsertChars(data->CursorPos, " ");
            }
            else
            {
                std::string common = matches[0];
                for(size_t i = 1; i < matches.size(); ++i)
                {
                    size_t j = 0;
                    while(j < common.size() && j < matches[i].size() &&
                          std::tolower(static_cast<unsigned char>(common[j])) ==
                          std::tolower(static_cast<unsigned char>(matches[i][j])))
                    {
                        ++j;
                    }
                    common.resize(j);
                }

                if(common.size() > prefix.size())
                {
                    data->DeleteChars(0, static_cast<int>(prefix.size()));
                    data->InsertChars(0, common.c_str());
                }
            }
            break;
        }
    }
    return 0;
}

void Console::assignWorld(World* world)
{
    this->world = world;
}

void Console::assignGame(Game* game)
{
    this->game = game;
}

void Console::appendLog(std::wstring text, sf::Color color)
{
    logBuffer += wstringToString(text);
    logBuffer += '\n';
    logs.push_back({std::move(text), color});
}