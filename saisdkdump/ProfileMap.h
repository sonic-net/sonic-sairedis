#pragma once

#include <map>
#include <string>

class ProfileMap
{
public:
    ProfileMap();

    bool loadFromFile(const std::string& profileMapFile);

    void clear();

    const char* getValue(const char* variable) const;

    int getNextValue(const char** variable, const char** value);

private:
    std::map<std::string, std::string> m_map;
    mutable std::map<std::string, std::string>::iterator m_iter;
};
