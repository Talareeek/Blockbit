#ifndef CONSOLE_HPP
#define CONSOLE_HPP

#include "UIElement.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <streambuf>
#include <iosfwd>
#include <functional>
#include "Command.hpp"

struct ImGuiInputTextCallbackData;
struct ImFont;

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

    struct LogEntry
    {
        std::wstring text;
        sf::Color color;
    };

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


    World* world = nullptr;

    Game* game = nullptr;

public:

    void handleEvent(const sf::Event& event);
    void update(float dt);
    void render();

    void writeLine(std::wstring string);
    void writeLine(std::wstring string, sf::Color color);

    // Loads a TTF for the console and rebuilds the ImGui font atlas. Call
    // after ImGui::SFML::Init (and after any re-init, since the atlas is
    // destroyed with the context and the returned ImFont* becomes invalid).
    void setupFont(const std::string& path, float size_pixels = 16.0f);

    void assignWorld(World* world);
    void assignGame(Game* game);

    // When set, '/' prefixed input is forwarded to the server through this
    // relay (serialized as CommandMessagePacket inside the implementation),
    // instead of being executed locally.
    void assignCommandRelay(std::function<void(const std::wstring&)> relay);

    void setOpen(bool open) { this->open = open; }

private:
    std::function<void(const std::wstring&)> command_relay;

    ImFont* console_font = nullptr;
};

#endif // CONSOLE_HPP