#ifndef WORLD_INPUT_STREAM_HPP
#define WORLD_INPUT_STREAM_HPP

#include <string>

class World;

class WorldInputStream
{
private:

    std::string file_path;

public:

    WorldInputStream(const std::string& file_path) : file_path{file_path}
    {

    }

    void read(World& world);

};

#endif // WORLD_INPUT_STREAM_HPP