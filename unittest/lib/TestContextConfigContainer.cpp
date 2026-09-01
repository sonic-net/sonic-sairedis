#include "ContextConfigContainer.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(ContextConfigContainer, get)
{
    ContextConfigContainer ccc;

    EXPECT_EQ(ccc.get(0), nullptr);
}

// A malformed file (not JSON) is caught by the outer try/catch and falls
// back to the default container with a single default context at guid 0.
TEST(ContextConfigContainer, loadFromFile_malformedReturnsDefault)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/ccc_bad.txt");

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
    EXPECT_EQ(cc->m_name, "syncd");
    EXPECT_EQ(cc->m_dbAsic, "ASIC_DB");
}

// A null path argument is treated as "no file specified" and returns the
// default container immediately. Mirrors how syncd starts when -x is omitted.
TEST(ContextConfigContainer, loadFromFile_nullPathReturnsDefault)
{
    auto ccc = ContextConfigContainer::loadFromFile(nullptr);

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
}

// A nonexistent file path fails the ifstream::good() check before any
// parsing happens, and falls back to the default container.
TEST(ContextConfigContainer, loadFromFile_nonexistentPathReturnsDefault)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/does_not_exist.json");

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
}

// The static default container has the EMPTY zmq state on its sole context.
// This is what the reconciliation layer reads when no file is loaded.
TEST(ContextConfigContainer, getDefault_zmqStateIsEmpty)
{
    auto ccc = ContextConfigContainer::getDefault();

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
}

// JSON zmq_enable:true maps to CONTEXT_CONFIG_ZMQ_ENABLED. Endpoints from
// the file override the constructor defaults.
TEST(ContextConfigContainer, loadFromFile_zmqEnableTrueMapsToEnabled)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/context_config_container_ok.json");

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_ENABLED);
    EXPECT_EQ(cc->m_zmqEndpoint, "ipc:///tmp/test_zmq_ep");
    EXPECT_EQ(cc->m_zmqNtfEndpoint, "ipc:///tmp/test_zmq_ntf_ep");
}

// JSON zmq_enable:false maps to CONTEXT_CONFIG_ZMQ_DISABLED. This must
// remain distinguishable from "absent" so the reconciler can demote the
// command line's request.
TEST(ContextConfigContainer, loadFromFile_zmqEnableFalseMapsToDisabled)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/context_config_zmq_false.json");

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_DISABLED);
}

// A valid file that omits zmq_enable leaves the field at EMPTY while the
// rest of the context is preserved. This is the case that codifies "field
// absent is a first-class state, not a silent reset."
TEST(ContextConfigContainer, loadFromFile_zmqEnableAbsentLeavesEmpty)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/context_config_zmq_absent.json");

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);

    // Required fields and the present endpoints all survive the parse.
    EXPECT_EQ(cc->m_name, "syncd");
    EXPECT_EQ(cc->m_dbAsic, "ASIC_DB");
    EXPECT_EQ(cc->m_dbCounters, "COUNTERS_DB");
    EXPECT_EQ(cc->m_dbFlex, "FLEX_COUNTER_DB");
    EXPECT_EQ(cc->m_dbState, "STATE_DB");
    EXPECT_EQ(cc->m_zmqEndpoint, "ipc:///tmp/test_zmq_ep");
    EXPECT_EQ(cc->m_zmqNtfEndpoint, "ipc:///tmp/test_zmq_ntf_ep");
}

// All three zmq fields absent: state stays EMPTY, both endpoints stay at
// their constructor defaults, required fields still parse cleanly.
TEST(ContextConfigContainer, loadFromFile_allZmqFieldsAbsentLeavesDefaults)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/context_config_all_zmq_absent.json");

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
    EXPECT_EQ(cc->m_zmqEndpoint, "tcp://127.0.0.1:5555");
    EXPECT_EQ(cc->m_zmqNtfEndpoint, "tcp://127.0.0.1:5556");

    EXPECT_EQ(cc->m_name, "syncd");
    EXPECT_EQ(cc->m_dbAsic, "ASIC_DB");
}

// A wrong-type value for zmq_enable (string instead of bool) cannot be
// converted by nlohmann to a JSON bool, which throws type_error. The outer
// try/catch then collapses the entire load to the default container. This
// preserves the design's "malformed required structure → default" rule.
TEST(ContextConfigContainer, loadFromFile_zmqEnableWrongTypeFallsBackToDefault)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/context_config_zmq_wrong_type.json");

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    // The default container has name "syncd" but no relationship to the
    // fixture's values. The single observable invariant is that the
    // resulting zmq state is EMPTY rather than ENABLED or DISABLED.
    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
    EXPECT_EQ(cc->m_zmqEndpoint, "tcp://127.0.0.1:5555");
}

// An integer value for zmq_enable is also rejected. Without the explicit
// .get<bool>() call, nlohmann's contextual conversion in a ternary would
// silently truthy-coerce 1 to ENABLED and 0 to DISABLED, bypassing strict
// type checking. The parser uses .get<bool>() so any non-boolean JSON
// value, integer included, throws type_error.302 and falls back to the
// default container.
TEST(ContextConfigContainer, loadFromFile_zmqEnableIntegerFallsBackToDefault)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/context_config_zmq_integer.json");

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
}

// A genuinely required field (guid) is missing. nlohmann's bracket access
// throws, the outer try/catch collapses to default. Codifies that required
// structure is strict even after the ZMQ fields became optional.
TEST(ContextConfigContainer, loadFromFile_missingRequiredFieldFallsBackToDefault)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/context_config_missing_required.json");

    ASSERT_NE(ccc, nullptr);

    auto cc = ccc->get(0);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
}

// Two contexts in one file with independent zmq states is the gearbox
// configuration: main ASIC at guid 0 with zmq_enable:true, gbsyncd at
// guid 1 with zmq_enable:false. Each context's field is honored
// independently, which is the entire point of per-field precedence.
TEST(ContextConfigContainer, loadFromFile_gearboxTwoContextsHaveIndependentZmqState)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/context_config_gearbox.json");

    ASSERT_NE(ccc, nullptr);

    auto cc0 = ccc->get(0);
    auto cc1 = ccc->get(1);

    ASSERT_NE(cc0, nullptr);
    ASSERT_NE(cc1, nullptr);

    EXPECT_EQ(cc0->m_zmqEnable, CONTEXT_CONFIG_ZMQ_ENABLED);
    EXPECT_EQ(cc0->m_name, "syncd_main");

    EXPECT_EQ(cc1->m_zmqEnable, CONTEXT_CONFIG_ZMQ_DISABLED);
    EXPECT_EQ(cc1->m_name, "gbsyncd");
}

// Two contexts that both omit zmq_enable and the endpoint fields land
// in CONTEXT_CONFIG_ZMQ_EMPTY with identical constructor-default
// endpoints. The endpoint conflict check must skip this case so the
// loader does not collapse the file to the single-context default.
TEST(ContextConfigContainer, loadFromFile_twoEmptyContextsWithDefaultEndpointsDoNotConflict)
{
    auto ccc = ContextConfigContainer::loadFromFile("files/context_config_two_empty_no_endpoints.json");

    ASSERT_NE(ccc, nullptr);

    auto cc0 = ccc->get(0);
    auto cc1 = ccc->get(1);

    ASSERT_NE(cc0, nullptr);
    ASSERT_NE(cc1, nullptr);

    EXPECT_EQ(cc0->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
    EXPECT_EQ(cc1->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);

    EXPECT_EQ(cc0->m_zmqEndpoint, cc1->m_zmqEndpoint);
    EXPECT_EQ(cc0->m_zmqNtfEndpoint, cc1->m_zmqNtfEndpoint);

    EXPECT_EQ(cc0->m_name, "syncd_main");
    EXPECT_EQ(cc1->m_name, "gbsyncd");
}

// Two contexts that both opt into ZMQ but share the same endpoint
// remain a real conflict: the bind sites collide. hasConflict() must
// flag this and the container falls back to the default.
TEST(ContextConfigContainer, hasConflict_twoEnabledContextsWithSameEndpointConflict)
{
    auto a = std::make_shared<ContextConfig>(0, "a", "ASIC_DB_A", "COUNTERS_DB_A", "FLEX_DB_A", "STATE_DB");
    auto b = std::make_shared<ContextConfig>(1, "b", "ASIC_DB_B", "COUNTERS_DB_B", "FLEX_DB_B", "STATE_DB");

    a->m_zmqEnable = CONTEXT_CONFIG_ZMQ_ENABLED;
    b->m_zmqEnable = CONTEXT_CONFIG_ZMQ_ENABLED;

    EXPECT_TRUE(a->hasConflict(b));
}

// Two contexts both EMPTY with identical defaults do not conflict; the
// direct hasConflict() call codifies the same rule the loader relies
// on, so the skip cannot regress without breaking this test.
TEST(ContextConfigContainer, hasConflict_twoEmptyContextsWithDefaultEndpointsDoNotConflict)
{
    auto a = std::make_shared<ContextConfig>(0, "a", "ASIC_DB_A", "COUNTERS_DB_A", "FLEX_DB_A", "STATE_DB");
    auto b = std::make_shared<ContextConfig>(1, "b", "ASIC_DB_B", "COUNTERS_DB_B", "FLEX_DB_B", "STATE_DB");

    EXPECT_EQ(a->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);
    EXPECT_EQ(b->m_zmqEnable, CONTEXT_CONFIG_ZMQ_EMPTY);

    EXPECT_FALSE(a->hasConflict(b));
}

// An ENABLED context sharing endpoints with a DISABLED context is not a
// conflict. The DISABLED context is locked to Redis and never binds a ZMQ
// socket, so there is nothing for the ENABLED context to collide with. The
// check is symmetric, so it holds regardless of which side is DISABLED.
TEST(ContextConfigContainer, hasConflict_enabledAndDisabledShareEndpointsDoNotConflict)
{
    auto a = std::make_shared<ContextConfig>(0, "a", "ASIC_DB_A", "COUNTERS_DB_A", "FLEX_DB_A", "STATE_DB");
    auto b = std::make_shared<ContextConfig>(1, "b", "ASIC_DB_B", "COUNTERS_DB_B", "FLEX_DB_B", "STATE_DB");

    a->m_zmqEnable = CONTEXT_CONFIG_ZMQ_ENABLED;
    b->m_zmqEnable = CONTEXT_CONFIG_ZMQ_DISABLED;

    // both keep the shared constructor default endpoints
    EXPECT_EQ(a->m_zmqEndpoint, b->m_zmqEndpoint);
    EXPECT_EQ(a->m_zmqNtfEndpoint, b->m_zmqNtfEndpoint);

    EXPECT_FALSE(a->hasConflict(b));
    EXPECT_FALSE(b->hasConflict(a));
}

// loadFromFile is idempotent for the same file (no global state leak).
TEST(ContextConfigContainer, loadFromFile_isIdempotent)
{
    auto first  = ContextConfigContainer::loadFromFile("files/context_config_container_ok.json");
    auto second = ContextConfigContainer::loadFromFile("files/context_config_container_ok.json");

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    auto a = first->get(0);
    auto b = second->get(0);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_EQ(a->m_zmqEnable, b->m_zmqEnable);
    EXPECT_EQ(a->m_zmqEndpoint, b->m_zmqEndpoint);
}

TEST(ContextConfigContainer, insert)
{
    ContextConfigContainer ccc;

    auto cc = std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB");

    ccc.insert(cc);

    EXPECT_THROW(ccc.insert(cc), std::runtime_error);
}
