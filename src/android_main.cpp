// Android entry point stub.
//
// On Android, ANativeActivity_onCreate is provided by SFML::Main (whole-archive
// linked into libBlockbit.so) and ultimately dispatches into this main(). The
// desktop entry in main.cpp is excluded from the Android build by the
// if(ANDROID) branch in the top-level CMakeLists.txt.
//
// Phase 3: just open a window and clear it green to prove the build pipeline.
// Phase 4 will route into the real ClientGameState constructor.

#ifdef __ANDROID__

#include <SFML/Graphics.hpp>

#include <optional>

int main(int /*argc*/, char* /*argv*/[])
{
    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "Blockbit");
    window.setFramerateLimit(30);

    bool active = true;

    while (window.isOpen())
    {
        while (const std::optional event = active ? window.pollEvent() : window.waitEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
            else if (event->is<sf::Event::MouseLeft>())
                active = false;
            else if (event->is<sf::Event::MouseEntered>())
                active = true;
        }

        if (active)
        {
            window.clear(sf::Color::Green);
            window.display();
        }
        else
        {
            sf::sleep(sf::milliseconds(100));
        }
    }

    return 0;
}

#endif // __ANDROID__
