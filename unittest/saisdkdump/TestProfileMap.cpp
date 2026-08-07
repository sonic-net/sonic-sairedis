#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "ProfileMap.h"

#include "swss/logger.h"

namespace
{
    class TempProfile
    {
    public:
        explicit TempProfile(const std::string& content)
        {
            SWSS_LOG_ENTER();

            char path[] = "/tmp/saisdkdump_profile_test.XXXXXX";
            int fd = mkstemp(path);
            EXPECT_GE(fd, 0);
            if (fd < 0)
            {
                return;
            }

            m_path = path;
            close(fd);

            std::ofstream out(m_path);
            out << content;
        }

        ~TempProfile()
        {
            SWSS_LOG_ENTER();

            if (!m_path.empty())
            {
                unlink(m_path.c_str());
            }
        }

        const std::string& path() const
        {
            SWSS_LOG_ENTER();

            return m_path;
        }

        TempProfile(const TempProfile&) = delete;
        TempProfile& operator=(const TempProfile&) = delete;

    private:
        std::string m_path;
    };
}

TEST(ProfileMap, emptyPathLoadsNoEntries)
{
    SWSS_LOG_ENTER();

    ProfileMap profileMap;

    EXPECT_TRUE(profileMap.loadFromFile(""));
    EXPECT_EQ(nullptr, profileMap.getValue("CT_TABLE_DUMP_ENABLE"));
}

TEST(ProfileMap, missingFileReturnsFalse)
{
    SWSS_LOG_ENTER();

    ProfileMap profileMap;

    EXPECT_FALSE(profileMap.loadFromFile("/tmp/saisdkdump_profile_missing_file"));
}

TEST(ProfileMap, parsesKeyValuePairs)
{
    SWSS_LOG_ENTER();

    ProfileMap profileMap;
    const TempProfile profile(
            "# comment line\n"
            "; comment line\n"
            "CT_TABLE_DUMP_ENABLE=true\n"
            "OTHER_KEY=value\n");

    EXPECT_TRUE(profileMap.loadFromFile(profile.path()));
    EXPECT_STREQ("true", profileMap.getValue("CT_TABLE_DUMP_ENABLE"));
    EXPECT_STREQ("value", profileMap.getValue("OTHER_KEY"));
    EXPECT_EQ(nullptr, profileMap.getValue("UNKNOWN_KEY"));
}

TEST(ProfileMap, parsesCrLfLineEndings)
{
    SWSS_LOG_ENTER();

    ProfileMap profileMap;
    const TempProfile profile(
            "CT_TABLE_DUMP_ENABLE=true\r\n"
            "OTHER_KEY=value\r\n");

    EXPECT_TRUE(profileMap.loadFromFile(profile.path()));
    EXPECT_STREQ("true", profileMap.getValue("CT_TABLE_DUMP_ENABLE"));
    EXPECT_STREQ("value", profileMap.getValue("OTHER_KEY"));
}

TEST(ProfileMap, skipsMalformedLines)
{
    SWSS_LOG_ENTER();

    ProfileMap profileMap;
    const TempProfile profile(
            "VALID=yes\n"
            "no_equals_sign\n"
            "CT_TABLE_DUMP_ENABLE=true\n");

    EXPECT_TRUE(profileMap.loadFromFile(profile.path()));
    EXPECT_STREQ("yes", profileMap.getValue("VALID"));
    EXPECT_STREQ("true", profileMap.getValue("CT_TABLE_DUMP_ENABLE"));
}

TEST(ProfileMap, getNextValueIteratesAndResets)
{
    SWSS_LOG_ENTER();

    ProfileMap profileMap;
    const TempProfile profile(
            "FIRST=one\n"
            "SECOND=two\n");

    EXPECT_TRUE(profileMap.loadFromFile(profile.path()));

    const char* variable = nullptr;
    const char* value = nullptr;

    EXPECT_EQ(0, profileMap.getNextValue(nullptr, nullptr));

    std::vector<std::pair<std::string, std::string>> entries;
    while (profileMap.getNextValue(&variable, &value) == 0)
    {
        entries.emplace_back(variable, value);
    }

    ASSERT_EQ(2u, entries.size());
    EXPECT_EQ("FIRST", entries[0].first);
    EXPECT_EQ("one", entries[0].second);
    EXPECT_EQ("SECOND", entries[1].first);
    EXPECT_EQ("two", entries[1].second);

    EXPECT_EQ(0, profileMap.getNextValue(nullptr, nullptr));
    EXPECT_EQ(0, profileMap.getNextValue(&variable, &value));
    EXPECT_EQ("FIRST", std::string(variable));
}

TEST(ProfileMap, clearRemovesEntries)
{
    SWSS_LOG_ENTER();

    ProfileMap profileMap;
    const TempProfile profile("CT_TABLE_DUMP_ENABLE=true\n");

    EXPECT_TRUE(profileMap.loadFromFile(profile.path()));
    EXPECT_STREQ("true", profileMap.getValue("CT_TABLE_DUMP_ENABLE"));

    profileMap.clear();
    EXPECT_EQ(nullptr, profileMap.getValue("CT_TABLE_DUMP_ENABLE"));
}
