#ifndef CHAT_HPP
#define CHAT_HPP

#include <string>
#include <deque>

class Chat
{
private:

    std::deque<std::wstring> messages;

public:

    void pushMessage(std::wstring message);
    const std::deque<std::wstring>& getMessages() const;

    constexpr static int MAX_MESSAGES = 100;

};

#endif // CHAT_HPP