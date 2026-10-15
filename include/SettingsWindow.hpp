#ifndef SETTINGS_WINDOW_HPP
#define SETTINGS_WINDOW_HPP

class Game;

class SettingsWindow
{
private:

    bool open = false;

    bool apply_graphics_requested = false;
    bool apply_audio_requested = false;

    Game* game = nullptr;

public:

    void assignGame(Game* game);

    void show();
    void hide();
    void toggle();

    bool isOpen() const;

    void render();

    bool consumeGraphicsRequest();
    bool consumeAudioRequest();
};

#endif // SETTINGS_WINDOW_HPP
