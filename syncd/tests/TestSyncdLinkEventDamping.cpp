#include <cstdint>
#include <memory>
#include <vector>
#include <thread>

#include <arpa/inet.h>

#include <gtest/gtest.h>

#include <swss/logger.h>
#include <swss/schema.h>
#include "swss/select.h"

#include "Sai.h"
#include "Syncd.h"
#include "MetadataLogger.h"

#include "TestSyncdLib.h"

#include "meta/sai_serialize.h"
#include "sairediscommon.h"
#include "meta/RedisSelectableChannel.h"

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

    commandLineOptions->m_enableSyncMode= true;
    commandLineOptions->m_enableTempView = true;
    commandLineOptions->m_disableExitSleep = true;
    commandLineOptions->m_enableUnittests = true;
    commandLineOptions->m_enableSaiBulkSupport = true;
    commandLineOptions->m_startType = SAI_START_TYPE_COLD_BOOT;
    commandLineOptions->m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;
    commandLineOptions->m_profileMapFile = "./brcm/testprofile.ini";

    auto syncd = std::make_shared<Syncd>(vendorSai, commandLineOptions, isWarmStart);
    syncd->run();

    SWSS_LOG_NOTICE("Started syncd worker.");
}

class LinkEventDampingTest : public ::testing::Test
{
public:
    LinkEventDampingTest()
    {
        SWSS_LOG_ENTER();

        auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);

        m_selectableChannel = std::make_shared<sairedis::RedisSelectableChannel>(
                dbAsic,
                REDIS_TABLE_GETRESPONSE,
                ASIC_STATE_TABLE,
                TEMP_PREFIX,
                false);
    }

    virtual ~LinkEventDampingTest() = default;

public:
    virtual void SetUp() override
    {
        SWSS_LOG_ENTER();

        m_switchId = SAI_NULL_OBJECT_ID;

        // flush ASIC DB
        flushAsicDb();

        syncdStart();
        createSwitch();
    }

    void syncdStart()
    {
        SWSS_LOG_ENTER();

        // start syncd worker
        m_worker = std::make_shared<std::thread>(syncdLinkEventDampingWorkerThread);

        // initialize SAI redis
        m_sairedis = std::make_shared<sairedis::Sai>();

        auto status = m_sairedis->initialize(0, &test_services);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // set communication mode
        sai_attribute_t attr;

        attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
        attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;

        status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // enable recording
        attr.id = SAI_REDIS_SWITCH_ATTR_RECORD;
        attr.value.booldata = true;

        status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    }

    void createSwitch()
    {
        SWSS_LOG_ENTER();

        sai_attribute_t attr;

        // init view
        attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;

        auto status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // apply view
        attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;

        status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // create switch
        attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        attr.value.booldata = true;

        status = m_sairedis->create(SAI_OBJECT_TYPE_SWITCH, &m_switchId, SAI_NULL_OBJECT_ID, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    virtual void TearDown() override
    {
        SWSS_LOG_ENTER();

        // uninitialize SAI redis
        ASSERT_EQ(m_sairedis->uninitialize(), SAI_STATUS_SUCCESS);

        // stop syncd worker
        sendSyncdShutdownNotification();
        m_worker->join();
    }

protected:
    std::shared_ptr<std::thread> m_worker;
    std::shared_ptr<sairedis::Sai> m_sairedis;
    sai_object_id_t m_switchId;
    std::shared_ptr<sairedis::RedisSelectableChannel> m_selectableChannel;
};

sai_status_t getResponseStatus(
        _In_ const std::string& command,
        _In_ sairedis::RedisSelectableChannel *selectable,
        _In_ bool init_view_mode)
{
    SWSS_LOG_ENTER();

    swss::Select s;
    s.addSelectable(selectable);

    while (true)
    {
        swss::Selectable *sel;
        int result = s.select(&sel, 1000);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;
            selectable->pop(kco, init_view_mode);

            const std::string &op = kfvOp(kco);
            const std::string &opkey = kfvKey(kco);

            if (op != command)
            {
                SWSS_LOG_WARN("got not expected response: %s:%s", opkey.c_str(), op.c_str());
                continue;
            }

            sai_status_t status;
            sai_deserialize_status(opkey, status);

            return status;
        }

        SWSS_LOG_ERROR("SELECT operation result: %s on %s", swss::Select::resultToString(result).c_str(), command.c_str());
        break;
    }

    return SAI_STATUS_FAILURE;
}

TEST_F(LinkEventDampingTest, SetLinkEventDampingConfigNotImplemented)
{
    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" + sai_serialize_object_id(SAI_NULL_OBJECT_ID);

    std::string str_attr_id = sai_serialize_redis_port_attr_id(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM);

    std::string str_attr_value = sai_serialize_redis_link_event_damping_algorithm(SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    m_selectableChannel->set(key, {swss::FieldValueTuple(str_attr_id, str_attr_value)}, REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    EXPECT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET, m_selectableChannel.get(), false), SAI_STATUS_NOT_IMPLEMENTED);
}
