#include "ContextConfig.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(ContextConfig, hasConflict)
{
    auto cc = std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB");

    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DB","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DD","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DD","FLEX_DD", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DD","FLEX_DD", "STATE_DD")));

    auto aa = std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DD","FLEX_DD", "STATE_DD");

    // check m_zmqEndpoint
    auto originalValue = aa->m_zmqEndpoint;
    aa->m_zmqEndpoint = "AA";

    EXPECT_TRUE(cc->hasConflict(aa));
    aa->m_zmqEndpoint = originalValue;

    // check m_zmqNtfEndpoint
    originalValue = aa->m_zmqNtfEndpoint;
    aa->m_zmqNtfEndpoint = "AA";

    EXPECT_FALSE(cc->hasConflict(aa));
    aa->m_zmqNtfEndpoint = originalValue;

    // check m_shmName
    auto originalValue = aa->m_shmName;
    aa->m_shmName = "AA";

    EXPECT_TRUE(cc->hasConflict(aa));
    aa->m_shmName = originalValue;

    // check m_shmNtfName
    originalValue = aa->m_shmNtfName;
    aa->m_shmNtfName = "AA";

    EXPECT_FALSE(cc->hasConflict(aa));
    aa->m_shmNtfName = originalValue;
}

