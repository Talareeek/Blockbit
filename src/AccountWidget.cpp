#include "../include/AccountWidget.hpp"
#include "../include/AssetManager.hpp"

AccountWidget::AccountWidget() : UIElement({0, 0}, {200, 50})
{

}

AccountWidget::AccountWidget(Account* account) : UIElement({0, 0}, {200, 50}), account(account)
{

}

void AccountWidget::handleEvent(const sf::Event& event)
{
    if(const auto* mouse = event.getIf<sf::Event::MouseMoved>())
    {
        sf::FloatRect widgetRect(position, size);
        hovered = widgetRect.contains(sf::Vector2f(mouse->position));
    }
}

void AccountWidget::update(float dt)
{
    
}

void AccountWidget::render(sf::RenderWindow& window)
{
    window.setView(window.getDefaultView());

    size = {window.getSize().x / 4.0f, window.getSize().y / 12.0f};
    position = {static_cast<float>(window.getSize().x) - size.x, 0.0f};

    float padding = size.y / 6.0f;

    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setFillColor(hovered ? sf::Color::White : sf::Color::Black);
    window.draw(background);

    sf::RectangleShape background2(size - sf::Vector2f(padding, padding));
    background2.setPosition(position + sf::Vector2f(padding / 2.0f, padding / 2.0f));
    background2.setFillColor(sf::Color(86, 86, 86));
    window.draw(background2);

    if(account == nullptr) return;

    float textSize = size.y * 0.45f;

    sf::Text usernameText(AssetManager::getFont(0), account->getUsername(), textSize);
    sf::Text balanceText(AssetManager::getFont(0), std::to_string(account->getCoins()), textSize);

    auto usernameBounds = usernameText.getLocalBounds();
    auto balanceBounds = balanceText.getLocalBounds();

    float centerY = position.y + size.y / 2.0f;

    usernameText.setPosition({position.x + padding, centerY - usernameBounds.size.y / 2.0f - usernameBounds.position.y});

    window.draw(usernameText);

    sf::Sprite coinSprite(AssetManager::getTexture(7));
    const sf::Texture* coinTexture = &coinSprite.getTexture();

    float coinScale = (textSize * 1.2f) / coinTexture->getSize().y;
    coinSprite.setScale({coinScale, coinScale});

    float coinWidth = coinTexture->getSize().x * coinScale;

    float balanceX = position.x + size.x - padding - balanceBounds.size.x;
    float coinX = balanceX - coinWidth - padding / 2.0f;

    balanceText.setPosition({balanceX, centerY - balanceBounds.size.y / 2.0f - balanceBounds.position.y});

    coinSprite.setPosition({coinX, centerY - (coinTexture->getSize().y * coinScale) / 2.0f});

    window.draw(coinSprite);
    window.draw(balanceText);

    /*
    size = {window.getSize().x / 4.0f, window.getSize().y / 12.0f};
    position = {static_cast<float>(window.getSize().x) - size.x, 0.0f};

    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setFillColor(hovered ? sf::Color::White : sf::Color::Black);
    window.draw(background);

    sf::RectangleShape background2(size - sf::Vector2f(size.x / 32.0f, size.y / 32.0f));
    background2.setPosition(position + sf::Vector2f(size.x / 32.0f, size.y / 32.0f));
    background2.setFillColor(sf::Color(86, 86, 86));
    window.draw(background2);

    if(account == nullptr) return;

    sf::Text usernameText(AssetManager::getFont(0), account->getUsername(), size.y * (0.75f));
    usernameText.setPosition(position + sf::Vector2f(size.x / 32.0f, size.y - usernameText.getLocalBounds().size.y / 2.0f));
    window.draw(usernameText);

    sf::Text balanceText(AssetManager::getFont(0), std::to_string(account->getCoins()), size.y * (0.75f));
    balanceText.setPosition(position + sf::Vector2f(size.x - balanceText.getLocalBounds().size.x - size.x / 32.0f, size.y - balanceText.getLocalBounds().size.y / 2.0f));
    window.draw(balanceText);

    sf::Sprite coinSprite(AssetManager::getTexture(7));
    const sf::Texture* coinTexture = &coinSprite.getTexture();
    float coinScale = size.y / 32.0f * 1.5f;
    coinSprite.setPosition(position + sf::Vector2f(size.x - balanceText.getLocalBounds().size.x - size.x / 32.0f - coinTexture->getSize().x * coinScale, size.y - coinTexture->getSize().y * coinScale));
    coinSprite.setScale({coinScale, coinScale});
    window.draw(coinSprite);
    */
}