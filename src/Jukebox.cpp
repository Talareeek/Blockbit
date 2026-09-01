#include "../include/Jukebox.hpp"

void Jukebox::setMusic(sf::Music& music)
{
    stopMusic();

    this->music = std::move(music);
}

void Jukebox::setMusic(AssetManager::MusicID music_id)
{
    stopMusic();

    this->music = std::move(AssetManager::getMusic(music_id));
}

void Jukebox::playMusic(sf::Music& music)
{
    stopMusic();

    this->music = std::move(music);

    this->music->play();
}

void Jukebox::playMusic(AssetManager::MusicID music_id)
{
    stopMusic();

    this->music = std::move(AssetManager::getMusic(music_id));

    this->music->play();
}

void Jukebox::playMusic()
{
    this->music->play();
}

void Jukebox::pauseMusic()
{
    this->music->pause();
}

void Jukebox::stopMusic()
{
    if(music) this->music->pause();
}

bool Jukebox::playingMusic()
{
    return this->music->getStatus() == sf::SoundSource::Status::Playing;
}

void Jukebox::updateSounds()
{
    std::erase_if(sounds, [](sf::Sound& sound)
    {
        return sound.getStatus() == sf::SoundSource::Status::Stopped;
    });
}

void Jukebox::playSound(sf::Sound sound)
{
    sounds.push_back(sound);

    sounds.back().play();
}

void Jukebox::playSound(AssetManager::SoundID sound_id)
{
    sounds.emplace_back(AssetManager::getSound(sound_id));

    sounds.back().play();    
}

void Jukebox::reset()
{
    stopMusic();

    sounds.clear();

    music = std::nullopt;
}