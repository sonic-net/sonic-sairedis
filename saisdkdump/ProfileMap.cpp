#include "ProfileMap.h"

#include <cerrno>
#include <cstring>
#include <fstream>

#include "swss/logger.h"

ProfileMap::ProfileMap()
    : m_iter(m_map.begin())
{
}

bool ProfileMap::loadFromFile(const std::string& profileMapFile)
{
    SWSS_LOG_ENTER();

    m_map.clear();
    m_iter = m_map.begin();

    if (profileMapFile.empty())
    {
        return true;
    }

    std::ifstream profile(profileMapFile);

    if (!profile.is_open())
    {
        SWSS_LOG_ERROR("failed to open profile map file: '%s' : %s",
                profileMapFile.c_str(), strerror(errno));

        return false;
    }

    std::string line;

    while (getline(profile, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        size_t pos = line.find("=");

        if (pos == std::string::npos)
        {
            SWSS_LOG_WARN("not found '=' in line %s", line.c_str());
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        m_map[key] = value;

        SWSS_LOG_INFO("inserted: %s:%s", key.c_str(), value.c_str());
    }

    return true;
}

void ProfileMap::clear()
{
    m_map.clear();
    m_iter = m_map.begin();
}

const char* ProfileMap::getValue(const char* variable) const
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return NULL;
    }

    auto it = m_map.find(variable);

    if (it == m_map.end())
    {
        SWSS_LOG_NOTICE("%s: NULL", variable);
        return NULL;
    }

    SWSS_LOG_NOTICE("%s: %s", variable, it->second.c_str());

    return it->second.c_str();
}

int ProfileMap::getNextValue(const char** variable, const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        m_iter = m_map.begin();
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    if (m_iter == m_map.end())
    {
        SWSS_LOG_INFO("iterator reached end");
        return -1;
    }

    *variable = m_iter->first.c_str();
    *value = m_iter->second.c_str();

    SWSS_LOG_INFO("key: %s:%s", *variable, *value);

    m_iter++;

    return 0;
}
