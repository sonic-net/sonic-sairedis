#include <cstdint>
#include <memory>
#include <vector>
#include <thread>

#include <gtest/gtest.h>

#include <swss/logger.h>

#include "Sai.h"
#include "Syncd.h"
#include "MetadataLogger.h"

#include "TestSyncdLib.h"

using namespace syncd;

static const char* profile_get_value(
    _In_ sai_switch_profile_id_t profile_id,
    _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    return NULL;
}

static int profile_get_next_value(
    _In_ sai_switch_profile_id_t profile_id,
    _Out_ const char** variable,
    _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    SWSS_LOG_INFO("iterator reached end");
    return -1;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

void syncdLinkEventDampingWorkerThread()
{
    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);
    MetadataLogger::initialize();

    auto vendorSai = std::make_shared<VendorSai>();
    auto commandLineOptions = std::make_shared<CommandLineOptions>();
    auto isWarmStart = false;

    commandLineOptions->m_enableSyncMode = true;
    commandLineOptions->m_enableTempView = true;
    commandLineOptions->m_disableExitSleep = true;
    commandLineOptions->m_enableUnittests = false;
    commandLineOptions->m_enableSaiBulkSupport = true;
    commandLineOptions->m_startType = SAI_START_TYPE_COLD_BOOT;
    commandLineOptions->m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;
    commandLineOptions->m_profileMapFile = "./brcm/testprofile.ini";

    auto syncd = std::make_shared<Syncd>(vendorSai, commandLineOptions, isWarmStart);
    syncd->run();

    SWSS_LOG_NOTICE("Started syncd link event damping worker");
}

class SyncdLinkEventDampingTest : public ::testing::Test
{
public:
    SyncdLinkEventDampingTest() = default;
    virtual ~SyncdLinkEventDampingTest() = default;

public:
    virtual void SetUp() override
    {
        SWSS_LOG_ENTER();

        flushAsicDb();

        m_worker = std::make_shared<std::thread>(syncdLinkEventDampingWorkerThread);

        m_sairedis = std::make_shared<sairedis::Sai>();

        auto status = m_sairedis->apiInitialize(0, &test_services);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        sai_attribute_t attr;

        attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
        attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;

        status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        attr.id = SAI_REDIS_SWITCH_ATTR_RECORD;
        attr.value.booldata = true;

        status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    virtual void TearDown() override
    {
        SWSS_LOG_ENTER();

        auto status = m_sairedis->apiUninitialize();
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        sendSyncdShutdownNotification();
        m_worker->join();
    }

protected:
    std::shared_ptr<std::thread> m_worker;
    std::shared_ptr<sairedis::Sai> m_sairedis;
};

TEST_F(SyncdLinkEventDampingTest, SetLinkEventDampingAlgorithm)
{
    sai_attribute_t attr;

    attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;

    auto status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    sai_object_id_t switchId;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    status = m_sairedis->create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM;
    attr.value.s32 = SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED;

    status = m_sairedis->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr);
    EXPECT_EQ(status, SAI_STATUS_NOT_IMPLEMENTED);
}

TEST_F(SyncdLinkEventDampingTest, SetLinkEventDampingAiedConfig)
{
    sai_attribute_t attr;

    attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;

    auto status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    sai_object_id_t switchId;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    status = m_sairedis->create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    sai_redis_link_event_damping_algo_aied_config_t config = {};
    config.max_suppress_time = 30000;
    config.suppress_threshold = 3000;
    config.reuse_threshold = 750;
    config.decay_half_life = 5000;
    config.flap_penalty = 1000;

    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG;
    attr.value.ptr = &config;

    status = m_sairedis->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr);
    EXPECT_EQ(status, SAI_STATUS_NOT_IMPLEMENTED);
}
