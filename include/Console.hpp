#ifndef CONSOLE_HPP
#define CONSOLE_HPP

#include "UIElement.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <streambuf>
#include <iosfwd>
#include "Command.hpp"

struct ImGuiInputTextCallbackData;

class ConsoleSink
{
public:
    struct Message
    {
        std::wstring text;
        sf::Color color;
    };

    static void push(std::wstring text, sf::Color color);
    static std::vector<Message> drain();

    static void installRedirects();
};

class ConsoleStreambuf : public std::streambuf
{
public:
    ConsoleStreambuf(std::streambuf* passthrough, sf::Color color);

protected:
    int_type overflow(int_type c) override;
    int sync() override;

private:
    void flushLine();

    std::streambuf* passthrough;
    sf::Color color;
    std::string buffer;
    std::mutex mutex;
};

class Console
{
private:

    enum class ResizeEdge
    {
        None,
        Top,
        Bottom,
        Left,
        Right,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    struct LogEntry
    {
        std::wstring text;
        sf::Color color;
    };

    ResizeEdge currentEdge = ResizeEdge::None;
    bool dragging = false;
    sf::Vector2f lastMousePos;

    bool moving = false;

    bool open = false;

    std::vector<LogEntry> logs;

    std::string logBuffer;
    void appendLog(std::wstring text, sf::Color color);


    static constexpr size_t INPUT_BUFFER_SIZE = 256;
    char input[INPUT_BUFFER_SIZE] = {};

    std::optional<std::wstring> to_proceed;

    void submitInput();

    std::vector<std::string> history;
    int historyPos = -1;

    bool reclaimFocus = false;

    static int inputTextCallbackStub(ImGuiInputTextCallbackData* data);
    int inputTextCallback(ImGuiInputTextCallbackData* data);

    std::vector<std::string> findMatches(const std::string& prefix) const;


    float cursorTimer = 0.0f;

    World* world = nullptr;

    Game* game = nullptr;

public:

    void handleEvent(const sf::Event& event);
    void update(float dt);
    void render();

    void writeLine(std::wstring string);
    void writeLine(std::wstring string, sf::Color color);


    void assignWorld(World* world);
    void assignGame(Game* game);
};

#endif // CONSOLE_HPP