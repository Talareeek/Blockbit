#include "../include/Console.hpp"
#include "../include/InputManager.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Command.hpp"
#include "../include/GameCommon.hpp"

#include <iostream>
#include <deque>

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
        logs.push_back({std::move(msg.text), msg.color});
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

    ImGui::Begin("Console", &open);

    ImGui::BeginChild("ConsoleLogs", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

    for (const auto& log : logs)
    {
        ImGui::TextColored(SFMLColorToImGuiColor(log.color), "%s", wstringToString(log.text).c_str());
    }

    ImGui::EndChild();

    ImGui::BeginChild("InputCollection");

    bool enter_pressed = ImGui::InputText("##Input", input, INPUT_BUFFER_SIZE, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    bool send_clicked = ImGui::Button("Send");

    if(enter_pressed || send_clicked) submitInput();

    ImGui::EndChild();

    ImGui::End();
}


void Console::writeLine(std::wstring string)
{
    logs.push_back({std::move(string), sf::Color::White});
}

void Console::writeLine(std::wstring string, sf::Color color)
{
    logs.push_back({std::move(string), color});
}

void Console::assignWorld(World* world)
{
    this->world = world;
}

void Console::assignGame(Game* game)
{
    this->game = game;
}

void Console::submitInput()
{
    if(std::strlen(input) == 0) return;

    to_proceed = std::wstring(input, input + std::strlen(input));

    std::memset(input, 0, INPUT_BUFFER_SIZE);

    logs.push_back({L"::> " + to_proceed.value(), sf::Color(255, 255, 255, 127)});
}