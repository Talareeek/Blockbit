#include "../include/GenerationPreview.hpp"
#include "../include/Button.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace
{
    constexpr float SEED_AREA_RATIO   = 0.08f;
    constexpr float GRAPH_AREA_RATIO  = 0.45f;
    constexpr float SLIDER_AREA_RATIO = 1.0f - SEED_AREA_RATIO - GRAPH_AREA_RATIO;

    constexpr float GRAPH_Y_MAX = 200.0f;

    constexpr int OCTAVES = 4;
}

GenerationPreview::GenerationPreview(const sf::Vector2f& position, const sf::Vector2f& size, unsigned int seed) : UIElement(position, size), perlin(seed), seed(seed)
{
    seedButton = Button({}, {}, sf::Color(70, 90, 140), "Seed: 0");

    baseHeightSlider  = Slider({}, {}, 0.0f,   200.0f, 30.0f,  "Base Height");
    heightScaleSlider = Slider({}, {}, 0.0f,   100.0f, 35.0f,  "Height Scale");
    frequencySlider   = Slider({}, {}, 0.001f, 0.200f, 0.030f, "Frequency");
    amplitudeSlider   = Slider({}, {}, 0.0f,   2.0f,   1.0f,   "Amplitude");
    persistenceSlider = Slider({}, {}, 0.0f,   1.0f,   0.5f,   "Persistence");

    frequencySlider.setPrecision(3);
    amplitudeSlider.setPrecision(2);
    persistenceSlider.setPrecision(2);

    refreshSeedLabel();
}

GenerationPreview::GenerationPreview(const UIElement::ScreenRelative& sr, unsigned int seed) : UIElement(sr), perlin(seed), seed(seed)
{
    seedButton = Button({}, {}, sf::Color(70, 90, 140), "Seed: 0");

    baseHeightSlider  = Slider({}, {}, 0.0f,   200.0f, 55.0f,  "Base Height");
    heightScaleSlider = Slider({}, {}, 0.0f,   100.0f, 35.0f,  "Height Scale");
    frequencySlider   = Slider({}, {}, 0.001f, 0.200f, 0.035f, "Frequency");
    amplitudeSlider   = Slider({}, {}, 0.0f,   2.0f,   1.0f,   "Amplitude");
    persistenceSlider = Slider({}, {}, 0.0f,   1.0f,   0.55f,   "Persistence");

    frequencySlider.setPrecision(3);
    amplitudeSlider.setPrecision(2);
    persistenceSlider.setPrecision(2);

    refreshSeedLabel();
}

void GenerationPreview::setSeed(unsigned int s)
{
    seed = s;
    perlin = PerlinNoise(seed);
    refreshSeedLabel();
}

void GenerationPreview::rerollSeed()
{
    setSeed(static_cast<unsigned int>(std::rand()));
}

void GenerationPreview::refreshSeedLabel()
{
    seedButton.setText("Seed: " + std::to_string(seed));
}

void GenerationPreview::layoutChildren()
{
    seedButton.position = { position.x, position.y };
    seedButton.size = { size.x, size.y * SEED_AREA_RATIO };

    float slider_area_y = position.y + size.y * (SEED_AREA_RATIO + GRAPH_AREA_RATIO);
    float slider_area_h = size.y * SLIDER_AREA_RATIO;

    float gap = slider_area_h * 0.02f;
    float slider_h = (slider_area_h - gap * 6.0f) / 5.0f;
    float slider_w = size.x;

    Slider* sliders[5] = {
        &baseHeightSlider,
        &heightScaleSlider,
        &frequencySlider,
        &amplitudeSlider,
        &persistenceSlider
    };

    for (int i = 0; i < 5; i++)
    {
        sliders[i]->position = { position.x, slider_area_y + gap + i * (slider_h + gap) };
        sliders[i]->size = { slider_w, slider_h };
    }
}

float GenerationPreview::heightAt(float x) const
{
    float total = 0.0f;
    float frequency   = frequencySlider.getValue();
    float amplitude   = amplitudeSlider.getValue();
    float persistence = persistenceSlider.getValue();

    for (int i = 0; i < OCTAVES; i++)
    {
        total += perlin.noise(x * frequency, 0.0f) * amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return baseHeightSlider.getValue() + total * heightScaleSlider.getValue();
}

void GenerationPreview::handleEvent(const sf::Event& event)
{
    bool wasClicked = seedButton.clicked();
    seedButton.handleEvent(event);
    if (!wasClicked && seedButton.clicked())
    {
        rerollSeed();
    }

    baseHeightSlider.handleEvent(event);
    heightScaleSlider.handleEvent(event);
    frequencySlider.handleEvent(event);
    amplitudeSlider.handleEvent(event);
    persistenceSlider.handleEvent(event);
}

void GenerationPreview::update(float dt)
{
    layoutChildren();

    seedButton.update(dt);

    baseHeightSlider.update(dt);
    heightScaleSlider.update(dt);
    frequencySlider.update(dt);
    amplitudeSlider.update(dt);
    persistenceSlider.update(dt);
}

void GenerationPreview::render(sf::RenderWindow& window)
{
    float graph_w = size.x;
    float graph_h = size.y * GRAPH_AREA_RATIO - size.y * 0.02f;
    sf::Vector2f graph_pos{ position.x, position.y + size.y * SEED_AREA_RATIO };

    sf::RectangleShape graph_bg({ graph_w, graph_h });
    graph_bg.setPosition(graph_pos);
    graph_bg.setFillColor(sf::Color(20, 30, 45));
    graph_bg.setOutlineThickness(1.5f);
    graph_bg.setOutlineColor(sf::Color(80, 100, 130));
    window.draw(graph_bg);

    float base = baseHeightSlider.getValue();
    if (base >= 0.0f && base <= GRAPH_Y_MAX)
    {
        float base_y = graph_pos.y + graph_h * (1.0f - base / GRAPH_Y_MAX);
        sf::RectangleShape baseline({ graph_w, 1.0f });
        baseline.setPosition({ graph_pos.x, base_y });
        baseline.setFillColor(sf::Color(120, 120, 80, 180));
        window.draw(baseline);
    }

    int sample_count = std::max(2, static_cast<int>(graph_w));

    sf::VertexArray curve(sf::PrimitiveType::TriangleStrip);
    sf::VertexArray line(sf::PrimitiveType::LineStrip);

    for (int i = 0; i < sample_count; i++)
    {
        float t = static_cast<float>(i) / static_cast<float>(sample_count - 1);
        float world_x = (t - 0.5f) * 1000.0f;
        float h = heightAt(world_x);

        float h_norm = std::clamp(h / GRAPH_Y_MAX, 0.0f, 1.0f);
        float pixel_x = graph_pos.x + t * graph_w;
        float pixel_y = graph_pos.y + graph_h * (1.0f - h_norm);

        curve.append(sf::Vertex({ { pixel_x, pixel_y }, sf::Color(80, 200, 120, 130) }));
        curve.append(sf::Vertex({ { pixel_x, graph_pos.y + graph_h }, sf::Color(80, 200, 120, 200) }));

        line.append(sf::Vertex({ { pixel_x, pixel_y }, sf::Color(140, 240, 160) }));
    }

    window.draw(curve);
    window.draw(line);

    seedButton.render(window);

    baseHeightSlider.render(window);
    heightScaleSlider.render(window);
    frequencySlider.render(window);
    amplitudeSlider.render(window);
    persistenceSlider.render(window);
}
