#include "../include/Account.hpp"

Account::Account()
{

}

Account::Account(std::filesystem::path file)
{
    std::ifstream inFile(file);

    if(inFile)
    {
        std::getline(inFile, username);
        inFile >> coins;
        registered = true;
    }
}

void Account::registerAccount(const std::string& username)
{
    if (!registered)
    {
        this->username = username;
        registered = true;
    }
}

void Account::addCoins(int amount)
{
    coins += amount;
}

void Account::deductCoins(int amount)
{
    if (coins >= amount)
    {
        coins -= amount;
    }
}

std::string Account::getUsername() const
{
    return username;
}

int Account::getCoins() const
{
    return coins;
}

void Account::serialize(std::ofstream& file) const
{
    file << username << std::endl;
    file << coins << std::endl;
}