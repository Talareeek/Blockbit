#ifndef VIEW_HPP
#define VIEW_HPP

#include "UIElement.hpp"
#include "Item.hpp"

#include <deque>

enum class ViewType
{
    Chest,
};

struct SetContentData
{
    uint16_t target;
    std::variant<ItemStack, std::string> value;
};

struct ViewEventData
{
    uint16_t sender;
    uint8_t action;
    std::variant<ItemStack, std::string> value;
};

struct ViewEventPacket;
struct SetContentPacket;

class ServerView
{
private:

public:

    virtual void handleViewEvent(const ViewEventData& event_data) = 0;
    virtual void update(World& world) = 0;
    virtual std::deque<SetContentData> pendingContentUpdates() = 0; 

    virtual ~ServerView() = default;
};

class ClientView : public UIElement
{
public:

    virtual std::deque<ViewEventData> pendingEvents() = 0;
    virtual void handleContentUpdate(const SetContentData& set_content_data) = 0;

    virtual ~ClientView() = default;
};

#endif // VIEW_HPP