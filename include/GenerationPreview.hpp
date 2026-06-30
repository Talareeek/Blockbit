#ifndef GENERATION_PREVIEW_HPP
#define GENERATION_PREVIEW_HPP

#include "UIElement.hpp"
#include "Slider.hpp"
#include "Button.hpp"
#include "PerlinNoise.hpp"

class GenerationPreview : public UIElement
{
private:

    Button seedButton;

    Slider baseHeightSlider;
    Slider heightScaleSlider;
    Slider frequencySlider;
    Slider amplitudeSlider;
    Slider persistenceSlider;

    PerlinNoise perlin{0};
    unsigned int seed = 0;

    void refreshSeedLabel();
    void rerollSeed();

    void layoutChildren();

    float heightAt(float x) const;

public:

    GenerationPreview() = default;
    GenerationPreview(const sf::Vector2f& position, const sf::Vector2f& size, unsigned int seed = 0);
    GenerationPreview(const UIElement::ScreenRelative& sr, unsigned int seed = 0);

    void setSeed(unsigned int seed);
    unsigned int getSeed() const { return seed; }

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    float getBaseHeight()   const { return baseHeightSlider.getValue(); }
    float getHeightScale()  const { return heightScaleSlider.getValue(); }
    float getFrequency()    const { return frequencySlider.getValue(); }
    float getAmplitude()    const { return amplitudeSlider.getValue(); }
    float getPersistence()  const { return persistenceSlider.getValue(); }
};

#endif // GENERATION_PREVIEW_HPP
