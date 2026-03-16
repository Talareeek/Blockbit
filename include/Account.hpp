#ifndef ACCOUNT_HPP
#define ACCOUNT_HPP

#include <string>
#include <fstream>
#include <filesystem>

class Account
{
private:

    std::string username;

    int coins = 0;

    bool registered = false;

public:

    Account();

    Account(std::filesystem::path file);

    void registerAccount(const std::string& username);
    void addCoins(int amount);
    void deductCoins(int amount);

    std::string getUsername() const;
    int getCoins() const;

    void serialize(std::ofstream& file) const;

};

#endif // ACCOUNT_HPP