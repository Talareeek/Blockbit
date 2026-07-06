#include "../include/Chat.hpp"

void Chat::pushMessage(std::wstring message)
{
    messages.push_front(message);

    if(messages.size() > MAX_MESSAGES)
    {
        messages.pop_back();
    }
}

const std::deque<std::wstring>& Chat::getMessages() const
{
    return messages;
}