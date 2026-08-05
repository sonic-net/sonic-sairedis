#include "ContextConfig.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(ContextConfig, hasConflict)
{
    auto cc = std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB");

    // hasConflict() suppresses the endpoint-equality check when both
    // contexts are still in the constructor-default CONTEXT_CONFIG_ZMQ_EMPTY
    // state, since the matching endpoints in that case are unspecified
    // defaults rather than a real collision. Marking cc as ENABLED keeps
    // the assertions below that depend on endpoint conflict detection.
    cc->m_zmqEnable = CONTEXT_CONFIG_ZMQ_ENABLED;

    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DB","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DD","FLEX_DB", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DD","FLEX_DD", "STATE_DB")));
    EXPECT_TRUE(cc->hasConflict(std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DD","FLEX_DD", "STATE_DD")));

    auto aa = std::make_shared<ContextConfig>(1, "syncb", "ASIC_DD", "COUNTERS_DD","FLEX_DD", "STATE_DD");

    aa->m_zmqEndpoint = "AA";

    EXPECT_TRUE(cc->hasConflict(aa));

    aa->m_zmqNtfEndpoint = "AA";

    EXPECT_FALSE(cc->hasConflict(aa));
}

// A freshly constructed ContextConfig has the EMPTY zmq state, meaning
// "the operator has not expressed an opinion." The reconciliation layer
// must then defer to the command-line mode.
TEST(ContextConfig, defaultZmqStateIsEmpty)
{
    auto cc = std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB", "FLEX_DB", "STATE_DB");

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
}

// The constructor seeds the canonical endpoint strings used when
// context_config.json omits the endpoint fields. These constants are the
// upstream defaults documented in the SAI redis communication mode header.
TEST(ContextConfig, defaultZmqEndpointsMatchSpec)
{
    auto cc = std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB", "FLEX_DB", "STATE_DB");

    EXPECT_EQ(cc->m_zmqEndpoint, "tcp://127.0.0.1:5555");
    EXPECT_EQ(cc->m_zmqNtfEndpoint, "tcp://127.0.0.1:5556");
}

// The three enum values have stable distinct integer representations.
// Several reads in the codebase (the SWSS_LOG_NOTICE label mapping in the
// parser, for instance) depend on EMPTY/ENABLED/DISABLED being recognizable.
TEST(ContextConfig, zmqStateEnumValuesAreDistinct)
{
    EXPECT_NE(CONTEXT_CONFIG_ZMQ_EMPTY, CONTEXT_CONFIG_ZMQ_ENABLED);
    EXPECT_NE(CONTEXT_CONFIG_ZMQ_EMPTY, CONTEXT_CONFIG_ZMQ_DISABLED);
    EXPECT_NE(CONTEXT_CONFIG_ZMQ_ENABLED, CONTEXT_CONFIG_ZMQ_DISABLED);
}

// EMPTY is the zero value of the enum so a default-constructed
// ContextConfig (or any zero-initialized memory) lands on it.
TEST(ContextConfig, zmqEmptyIsZero)
{
    EXPECT_EQ(static_cast<uint8_t>(CONTEXT_CONFIG_ZMQ_EMPTY), 0u);
}
