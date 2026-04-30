/**
 * Unit tests for Syncd::saiLoglevelNotify — Issue #170
 *
 * Bug: swss::Logger::linkToDb delivers log level as a short swss priority
 * string ("INFO", "NOTICE", …) but sai_deserialize_log_level expects the
 * full SAI enum name ("SAI_LOG_LEVEL_INFO", "SAI_LOG_LEVEL_NOTICE", …).
 * Without the translation the deserializer falls through to
 * sai_deserialize_number(), fails to parse the alphabetic string, and syncd
 * logs:
 *   ERR syncd: :- saiLoglevelNotify: Invalid SAI loglevel SAI_API_NEIGHBOR INFO
 *
 * Fix: swssLogLevelToSaiLogLevel() translates before deserializing.
 *
 * Test strategy
 * ─────────────
 * 1. swss short names  → logSet called with correct SAI level (happy path)
 * 2. SAI full names    → still work (pass-through, backward compat)
 * 3. Unknown string    → no crash, logSet NOT called (graceful error)
 * 4. All swss levels   → every mapping is exercised (parameterized)
 * 5. logSet failure    → handled without exception
 * 6. Regression        → exact inputs from issue #170 report
 *
 * NOTE on FRIEND_TEST access:
 *   saiLoglevelNotify is private in Syncd.  FRIEND_TEST grants access to the
 *   individual gtest test-body class (e.g. SaiLoglevelNotifyTest_Foo_Test),
 *   NOT to the fixture base class.  Therefore every test body calls
 *   m_syncd->saiLoglevelNotify(...) directly — no helper wrapper is used.
 */

#include "Syncd.h"
#include "CommandLineOptions.h"
#include "MockableSaiInterface.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace syncd;
using namespace testing;

// ── Fixture ──────────────────────────────────────────────────────────────────

class SaiLoglevelNotifyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_sai   = std::make_shared<MockableSaiInterface>();
        m_opt   = std::make_shared<CommandLineOptions>();
        m_syncd = std::make_shared<Syncd>(m_sai, m_opt, false);
    }

    std::shared_ptr<MockableSaiInterface> m_sai;
    std::shared_ptr<CommandLineOptions>   m_opt;
    std::shared_ptr<Syncd>                m_syncd;
};

// ── Test 1a: swss "INFO" → SAI_LOG_LEVEL_INFO ────────────────────────────────

TEST_F(SaiLoglevelNotifyTest, SwssShortName_INFO_CallsLogSetWithInfoLevel)
{
    sai_log_level_t capturedLevel = SAI_LOG_LEVEL_NOTICE; // sentinel
    bool logSetCalled = false;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    // Direct call — FRIEND_TEST grants access to this test-body class
    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_NEIGHBOR", "INFO"));

    EXPECT_TRUE(logSetCalled) << "logSet must be called when level is translatable";
    EXPECT_EQ(SAI_LOG_LEVEL_INFO, capturedLevel)
        << "swss 'INFO' must map to SAI_LOG_LEVEL_INFO";
}

// ── Test 1b: swss "NOTICE" → SAI_LOG_LEVEL_NOTICE ────────────────────────────

TEST_F(SaiLoglevelNotifyTest, SwssShortName_NOTICE_CallsLogSetWithNoticeLevel)
{
    sai_log_level_t capturedLevel = SAI_LOG_LEVEL_DEBUG;
    bool logSetCalled = false;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_NEXT_HOP", "NOTICE"));

    EXPECT_TRUE(logSetCalled);
    EXPECT_EQ(SAI_LOG_LEVEL_NOTICE, capturedLevel)
        << "swss 'NOTICE' must map to SAI_LOG_LEVEL_NOTICE";
}

// ── Test 1c: swss "DEBUG" → SAI_LOG_LEVEL_DEBUG ──────────────────────────────

TEST_F(SaiLoglevelNotifyTest, SwssShortName_DEBUG_CallsLogSetWithDebugLevel)
{
    sai_log_level_t capturedLevel = SAI_LOG_LEVEL_NOTICE;
    bool logSetCalled = false;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_ROUTE", "DEBUG"));

    EXPECT_TRUE(logSetCalled);
    EXPECT_EQ(SAI_LOG_LEVEL_DEBUG, capturedLevel);
}

// ── Test 1d: swss "WARN" → SAI_LOG_LEVEL_WARN ────────────────────────────────

TEST_F(SaiLoglevelNotifyTest, SwssShortName_WARN_CallsLogSetWithWarnLevel)
{
    sai_log_level_t capturedLevel = SAI_LOG_LEVEL_NOTICE;
    bool logSetCalled = false;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_ROUTE", "WARN"));

    EXPECT_TRUE(logSetCalled);
    EXPECT_EQ(SAI_LOG_LEVEL_WARN, capturedLevel);
}

// ── Test 1e: swss "ERROR" → SAI_LOG_LEVEL_ERROR ──────────────────────────────

TEST_F(SaiLoglevelNotifyTest, SwssShortName_ERROR_CallsLogSetWithErrorLevel)
{
    sai_log_level_t capturedLevel = SAI_LOG_LEVEL_NOTICE;
    bool logSetCalled = false;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_ROUTE", "ERROR"));

    EXPECT_TRUE(logSetCalled);
    EXPECT_EQ(SAI_LOG_LEVEL_ERROR, capturedLevel);
}

// ── Test 2a: SAI full name "SAI_LOG_LEVEL_INFO" still works ──────────────────

TEST_F(SaiLoglevelNotifyTest, SaiFullName_SAI_LOG_LEVEL_INFO_StillWorks)
{
    sai_log_level_t capturedLevel = SAI_LOG_LEVEL_NOTICE;
    bool logSetCalled = false;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_NEIGHBOR", "SAI_LOG_LEVEL_INFO"));

    EXPECT_TRUE(logSetCalled);
    EXPECT_EQ(SAI_LOG_LEVEL_INFO, capturedLevel)
        << "Full SAI enum name must still be accepted (backward compat)";
}

// ── Test 2b: SAI full name "SAI_LOG_LEVEL_NOTICE" still works ────────────────

TEST_F(SaiLoglevelNotifyTest, SaiFullName_SAI_LOG_LEVEL_NOTICE_StillWorks)
{
    sai_log_level_t capturedLevel = SAI_LOG_LEVEL_DEBUG;
    bool logSetCalled = false;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_NEXT_HOP", "SAI_LOG_LEVEL_NOTICE"));

    EXPECT_TRUE(logSetCalled);
    EXPECT_EQ(SAI_LOG_LEVEL_NOTICE, capturedLevel);
}

// ── Test 3: Unknown / garbage string — no crash, logSet NOT called ────────────

TEST_F(SaiLoglevelNotifyTest, UnknownLogLevel_DoesNotCrash_LogSetNotCalled)
{
    bool logSetCalled = false;

    m_sai->mock_logSet = [&](sai_api_t, sai_log_level_t) -> sai_status_t
    {
        logSetCalled = true;
        return SAI_STATUS_SUCCESS;
    };

    // "INVALID_LEVEL" is neither a swss short name nor a SAI enum name.
    // swssLogLevelToSaiLogLevel passes it through unchanged, then
    // sai_deserialize_log_level throws — the catch block must handle it.
    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_NEIGHBOR", "INVALID_LEVEL"));

    EXPECT_FALSE(logSetCalled)
        << "logSet must NOT be called when log level string is unrecognised";
}

// ── Test 4: All swss → SAI level mappings (parameterized) ────────────────────

struct LevelMapping
{
    std::string    swssName;
    sai_log_level_t expectedSaiLevel;
};

class SaiLoglevelMappingTest
    : public SaiLoglevelNotifyTest,
      public ::testing::WithParamInterface<LevelMapping>
{};

TEST_P(SaiLoglevelMappingTest, AllMappingsAreCorrect)
{
    const auto& param = GetParam();

    sai_log_level_t capturedLevel = static_cast<sai_log_level_t>(-1);
    bool logSetCalled = false;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_ROUTE", param.swssName));

    EXPECT_TRUE(logSetCalled)
        << "logSet not called for swss level: " << param.swssName;
    EXPECT_EQ(param.expectedSaiLevel, capturedLevel)
        << "Wrong SAI level for swss input: " << param.swssName;
}

INSTANTIATE_TEST_SUITE_P(
    SwssToSaiLevelMappings,
    SaiLoglevelMappingTest,
    ::testing::Values(
        LevelMapping{ "EMERG",  SAI_LOG_LEVEL_CRITICAL },
        LevelMapping{ "ALERT",  SAI_LOG_LEVEL_CRITICAL },
        LevelMapping{ "CRIT",   SAI_LOG_LEVEL_CRITICAL },
        LevelMapping{ "ERROR",  SAI_LOG_LEVEL_ERROR    },
        LevelMapping{ "WARN",   SAI_LOG_LEVEL_WARN     },
        LevelMapping{ "NOTICE", SAI_LOG_LEVEL_NOTICE   },
        LevelMapping{ "INFO",   SAI_LOG_LEVEL_INFO     },
        LevelMapping{ "DEBUG",  SAI_LOG_LEVEL_DEBUG    }
    )
);

// ── Test 5: logSet returns failure — no exception propagated ──────────────────

TEST_F(SaiLoglevelNotifyTest, LogSetFailure_IsHandledGracefully)
{
    m_sai->mock_logSet = [](sai_api_t, sai_log_level_t) -> sai_status_t
    {
        return SAI_STATUS_NOT_SUPPORTED;
    };

    // Must not throw even when the vendor SAI rejects the log level
    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_NEIGHBOR", "INFO"));
}

// ── Test 6: Regression — exact error strings from issue #170 ─────────────────
// Before the fix these two calls produced:
//   ERR syncd: :- saiLoglevelNotify: Invalid SAI loglevel SAI_API_NEIGHBOR INFO
//   ERR syncd: :- saiLoglevelNotify: Invalid SAI loglevel SAI_API_NEXT_HOP INFO

TEST_F(SaiLoglevelNotifyTest, Regression_Issue170_NeighborInfoDoesNotError)
{
    bool logSetCalled = false;
    sai_log_level_t capturedLevel = SAI_LOG_LEVEL_NOTICE;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    // Exact inputs from the issue report
    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_NEIGHBOR", "INFO"));
    EXPECT_TRUE(logSetCalled)  << "logSet must succeed for SAI_API_NEIGHBOR INFO";
    EXPECT_EQ(SAI_LOG_LEVEL_INFO, capturedLevel);
}

TEST_F(SaiLoglevelNotifyTest, Regression_Issue170_NextHopInfoDoesNotError)
{
    bool logSetCalled = false;
    sai_log_level_t capturedLevel = SAI_LOG_LEVEL_NOTICE;

    m_sai->mock_logSet = [&](sai_api_t /*api*/, sai_log_level_t level) -> sai_status_t
    {
        capturedLevel = level;
        logSetCalled  = true;
        return SAI_STATUS_SUCCESS;
    };

    EXPECT_NO_THROW(m_syncd->saiLoglevelNotify("SAI_API_NEXT_HOP", "INFO"));
    EXPECT_TRUE(logSetCalled)  << "logSet must succeed for SAI_API_NEXT_HOP INFO";
    EXPECT_EQ(SAI_LOG_LEVEL_INFO, capturedLevel);
}