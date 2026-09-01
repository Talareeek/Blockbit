#ifndef JUKEBOX_HPP
#define JUKEBOX_HPP

#include <SFML/Audio.hpp>

#include <vector>

#include "AssetManager.hpp"

class Jukebox
{
private:

    std::optional<sf::Music> music;
    std::vector<sf::Sound> sounds;

    void updateSounds();

public:

    void setMusic(sf::Music& music);
    void setMusic(AssetManager::MusicID music_id);

    void playMusic(sf::Music& music);
    void playMusic(AssetManager::MusicID music_id);

    void playMusic();
    void pauseMusic();
    void stopMusic();

    bool playingMusic();
    
    void playSound(sf::Sound sound);
    void playSound(AssetManager::SoundID sound_id);

    void reset();
};

#endif // JUKEBOX_HPP