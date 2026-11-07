#ifndef VIEW_HPP
#define VIEW_HPP

#include "UIElement.hpp"
#include "UUID.hpp"

#include <cstdint>
#include <deque>
#include <unordered_set>
#include <vector>

class World;

enum class ViewType : uint8_t
{
    Chest,
    Inventory,
    Furnace,
    Crafting
};

// SetContentData flows server -> client. It describes a single content slot
// of the open view and its binary payload. The payload bytes are owned by the
// concrete view type; both ends must agree on the layout (eg. a Chest view
// sends the raw bytes of an ItemStack).
struct SetContentData
{
    uint16_t target;
    std::vector<uint8_t> payload;
};

// ViewEventData flows client -> server. The sender identifies what triggered
// the event (eg. a slot index or a button id), the action tells the server
// how to interpret it and the payload carries any extra data agreed on by
// the concrete view type.
struct ViewEventData
{
    uint16_t sender;
    uint8_t  action;
    std::vector<uint8_t> payload;
};

class ServerView
{
public:

    virtual ViewType type() const = 0;

    virtual void handleViewEvent(World& world, const ViewEventData& event_data) = 0;
    virtual void update(World& world) = 0;

    // Called by the server immediately after the view becomes the active view
    // for a client. Lets the view do world-touching setup it couldn't do in its
    // constructor (eg. notifying a block entity that it has a new viewer).
    virtual void onOpen(World& world) {}

    // Called by the server right before the view is destroyed, so views can
    // mutate the world (eg. drop a cursor-held stack as an item entity).
    virtual void onClose(World& world) = 0;

    // Called every server tick. If it returns true the server will close the
    // view (eg. the player has walked out of reach of the backing block).
    virtual bool shouldClose(World& world) const { (void)world; return false; }

    virtual std::deque<SetContentData> drainPendingContentUpdates() = 0;

    // Push the UUIDs of any ItemInstance that is currently held only by
    // transient view state (cursor stack, recipe-result preview, etc.). The
    // itemInstances GC walks inventories and block entities on its own; this
    // hook is the view's chance to say "my cursor_stack also references X,
    // please don't drop it from the global map while I'm open".
    virtual void collectLiveInstanceIDs(std::unordered_set<UUID>& /*out*/) const {}

    virtual ~ServerView() = default;
};

class ClientView : public UIElement
{
public:

    virtual ViewType type() const = 0;

    virtual void handleContentUpdate(const SetContentData& set_content_data) = 0;

    virtual std::deque<ViewEventData> drainPendingEvents() = 0;

    virtual ~ClientView() = default;
};

#endif // VIEW_HPP
