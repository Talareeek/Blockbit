#ifndef WORLD_ENTRY_HPP
#define WORLD_ENTRY_HPP

#include "UIElement.hpp"
#include "AssetManager.hpp"

#include <SFML/Audio.hpp>

#include <filesystem>
#include <string>

class WorldEntry : public UIElement
{
private:

    std::string name;
    std::filesystem::path path;

    bool hovering_play   = false;
    bool hovering_host   = false;
    bool hovering_delete = false;

    bool play_requested   = false;
    bool host_requested   = false;
    bool delete_requested = false;

    sf::Sound click_sound{AssetManager::getSound(AssetManager::SoundID::Click)};

public:

    WorldEntry() = default;
    WorldEntry(const std::string& name, const std::filesystem::path& path);

    const std::string& getName() const;
    const std::filesystem::path& getPath() const;

    bool wasPlayRequested() const;
    bool wasHostRequested() const;
    bool wasDeleteRequested() const;
    void clearRequests();

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

extern void drawFitText(sf::RenderWindow& window, const std::string& str, const sf::FloatRect& box, bool center, sf::Color fill, float outline_thickness, sf::Color outline);

#endif // WORLD_ENTRY_HPP
