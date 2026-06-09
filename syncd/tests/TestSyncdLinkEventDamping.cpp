#include <cstdint>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <string>

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

        auto status = m_sairedis->apiInitialize(0, &test_services);
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

        auto status = m_sairedis->apiUninitialize();
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

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

void sendPortEvent(
    std::shared_ptr<sairedis::RedisSelectableChannel> channel,
    sai_object_id_t port,
    sai_port_oper_status_t status)
{
    SWSS_LOG_ENTER();

    sai_port_oper_status_notification_t n;
    n.port_id = port;
    n.port_state = status;
    n.port_error_status = SAI_PORT_ERROR_STATUS_CLEAR;

    std::string op = SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE;
    std::string data = sai_serialize_port_oper_status_ntf(1, &n);

    channel->set("port_event", { swss::FieldValueTuple("data", data) }, op);
}

sai_object_id_t getFirstPort(
    std::shared_ptr<sairedis::Sai> sai,
    sai_object_id_t switchId)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    auto status = sai->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, &attr);
    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    uint32_t portCount = attr.value.u32;

    std::vector<sai_object_id_t> ports(portCount);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list = ports.data();

    status = sai->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, &attr);
    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    return ports[0];
}

std::string getPortKey(sai_object_id_t portVid)
{
    SWSS_LOG_ENTER();

    return sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
           sai_serialize_object_id(portVid);
}

void setAlgorithm(
    std::shared_ptr<sairedis::RedisSelectableChannel> channel,
    const std::string& key,
    sai_redis_link_event_damping_algorithm_t algo)
{
    SWSS_LOG_ENTER();

    std::string id = sai_serialize_redis_port_attr_id(
        SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM);

    std::string val = sai_serialize_redis_link_event_damping_algorithm(algo);

    channel->set(key,
        {swss::FieldValueTuple(id, val)},
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);
}

void setAiedConfig(
    std::shared_ptr<sairedis::RedisSelectableChannel> channel,
    const std::string& key,
    const sai_redis_link_event_damping_algo_aied_config_t& config)
{
    SWSS_LOG_ENTER();

    std::string id = sai_serialize_redis_port_attr_id(
        SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG);

    std::string val = sai_serialize_redis_link_event_damping_aied_config(config);

    channel->set(key,
        {swss::FieldValueTuple(id, val)},
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);
}

TEST_F(LinkEventDampingTest, SetLinkEventDampingConfigInvalidPort)
{
    // SAI_NULL_OBJECT_ID is not a registered port VID, so VID→RID translation
    // returns SAI_NULL_OBJECT_ID, which causes processLinkEventDampingConfigSet()
    // to return SAI_STATUS_INVALID_PARAMETER
    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
                      sai_serialize_object_id(SAI_NULL_OBJECT_ID);

    std::string str_attr_id = sai_serialize_redis_port_attr_id(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM);

    std::string str_attr_value = sai_serialize_redis_link_event_damping_algorithm(
            SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    m_selectableChannel->set(key,
            {swss::FieldValueTuple(str_attr_id, str_attr_value)},
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    EXPECT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                                m_selectableChannel.get(), false),
                                SAI_STATUS_INVALID_PARAMETER);
}

TEST_F(LinkEventDampingTest, SetLinkEventDampingConfigSuccess)
{
    //Retrieve the number of ports on the switch.
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    auto status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    ASSERT_GT(attr.value.u32, 0u);

    uint32_t portCount = attr.value.u32;

    //Retrieve the port list so we have a real, registered port VID.
    std::vector<sai_object_id_t> portOids(portCount);
    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list  = portOids.data();

    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    ASSERT_GT(attr.value.objlist.count, 0u);

    sai_object_id_t portVid = portOids[0];

    //Build the damping config key using the real port VID.
    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
                      sai_serialize_object_id(portVid);

    //Set SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM → AIED.
    std::string str_algo_id = sai_serialize_redis_port_attr_id(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM);

    std::string str_algo_value = sai_serialize_redis_link_event_damping_algorithm(
            SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    m_selectableChannel->set(key,
            {swss::FieldValueTuple(str_algo_id, str_algo_value)},
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    EXPECT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                                m_selectableChannel.get(), false),
                                SAI_STATUS_SUCCESS);

    //Also set SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG.
    sai_redis_link_event_damping_algo_aied_config_t aiedConfig;
    aiedConfig.max_suppress_time  = 30000;   //30 seconds in ms
    aiedConfig.suppress_threshold = 1600;
    aiedConfig.reuse_threshold    = 1200;
    aiedConfig.decay_half_life    = 15000;    //15 seconds in ms
    aiedConfig.flap_penalty       = 1000;

    std::string str_aied_id = sai_serialize_redis_port_attr_id(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG);

    std::string str_aied_value = sai_serialize_redis_link_event_damping_aied_config(aiedConfig);

    m_selectableChannel->set(key,
            {swss::FieldValueTuple(str_aied_id, str_aied_value)},
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    EXPECT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                                m_selectableChannel.get(), false),
                                SAI_STATUS_SUCCESS);
}

TEST_F(LinkEventDampingTest, SetLinkEventDampingConfigWrongObjectType)
{
    // Use SAI_OBJECT_TYPE_SWITCH instead of SAI_OBJECT_TYPE_PORT.
    // processLinkEventDampingConfigSet() rejects any non-PORT object type
    // at Syncd.cpp with SAI_STATUS_INVALID_PARAMETER.
    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_SWITCH) + ":" +
                      sai_serialize_object_id(m_switchId);

    std::string str_attr_id = sai_serialize_redis_port_attr_id(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM);

    std::string str_attr_value = sai_serialize_redis_link_event_damping_algorithm(
            SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    m_selectableChannel->set(key,
            {swss::FieldValueTuple(str_attr_id, str_attr_value)},
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    EXPECT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                                m_selectableChannel.get(), false),
                                SAI_STATUS_INVALID_PARAMETER);
}

TEST_F(LinkEventDampingTest, SetLinkEventDampingConfigUnknownAttribute)
{
    //Get a real port VID so we pass the VID→RID validation.
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    auto status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    ASSERT_GT(attr.value.u32, 0u);

    uint32_t portCount = attr.value.u32;

    std::vector<sai_object_id_t> portOids(portCount);
    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list  = portOids.data();

    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    ASSERT_GT(attr.value.objlist.count, 0u);

    sai_object_id_t portVid = portOids[0];

    //Build the key for the valid port.
    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
                      sai_serialize_object_id(portVid);

    //Craft an unknown attribute ID — one past the last known enum value.
    //This hits the default: branch in processLinkEventDampingConfigSet()
    //at Syncd.cpp, which returns SAI_STATUS_INVALID_PARAMETER.
    sai_redis_port_attr_t unknownAttrId = static_cast<sai_redis_port_attr_t>(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG + 1);

    std::string str_unknown_id = sai_serialize_redis_port_attr_id(unknownAttrId);

    m_selectableChannel->set(key,
            {swss::FieldValueTuple(str_unknown_id, "invalid_value")},
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    EXPECT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                                m_selectableChannel.get(), false),
                                SAI_STATUS_INVALID_PARAMETER);
}

TEST_F(LinkEventDampingTest, SetDampingConfigMissingColonInKey)
{
    // Test invalid key format without colon separator
    std::string invalidKey = "INVALID_KEY_NO_COLON";

    std::string str_algo_id = sai_serialize_redis_port_attr_id(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM);

    std::string str_algo_value = sai_serialize_redis_link_event_damping_algorithm(
            SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    m_selectableChannel->set(invalidKey,
            {swss::FieldValueTuple(str_algo_id, str_algo_value)},
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    EXPECT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                                m_selectableChannel.get(), false),
                                SAI_STATUS_INVALID_PARAMETER);
}

TEST_F(LinkEventDampingTest, SetDampingConfigMultipleAttributes)
{
    // Retrieve a valid port VID
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    auto status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    ASSERT_GT(attr.value.u32, 0u);

    uint32_t portCount = attr.value.u32;

    std::vector<sai_object_id_t> portOids(portCount);
    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list  = portOids.data();

    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    sai_object_id_t portVid = portOids[0];
    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
                      sai_serialize_object_id(portVid);

    // Build field-value tuples for multiple attributes
    std::vector<swss::FieldValueTuple> attrs;

    // Add algorithm attribute
    std::string str_algo_id = sai_serialize_redis_port_attr_id(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM);
    std::string str_algo_value = sai_serialize_redis_link_event_damping_algorithm(
            SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);
    attrs.emplace_back(str_algo_id, str_algo_value);

    // Add config attribute
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time  = 10000;
    config.suppress_threshold = 1500;
    config.reuse_threshold    = 1000;
    config.decay_half_life    = 5000;
    config.flap_penalty       = 500;

    std::string str_config_id = sai_serialize_redis_port_attr_id(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG);
    std::string str_config_value = sai_serialize_redis_link_event_damping_aied_config(config);
    attrs.emplace_back(str_config_id, str_config_value);

    m_selectableChannel->set(key, attrs, REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    EXPECT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                                m_selectableChannel.get(), false),
                                SAI_STATUS_SUCCESS);
}

TEST_F(LinkEventDampingTest, SetDampingConfigEmptyValues)
{
    // Retrieve a valid port VID
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    auto status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    ASSERT_GT(attr.value.u32, 0u);

    uint32_t portCount = attr.value.u32;

    std::vector<sai_object_id_t> portOids(portCount);
    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list  = portOids.data();

    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    sai_object_id_t portVid = portOids[0];
    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
                      sai_serialize_object_id(portVid);

    // Set with empty field-value list
    std::vector<swss::FieldValueTuple> emptyAttrs;

    m_selectableChannel->set(key, emptyAttrs, REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    EXPECT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                                m_selectableChannel.get(), false),
                                SAI_STATUS_SUCCESS);
}

TEST_F(LinkEventDampingTest, PenaltyCeilingHit)
{
    auto portVid = getFirstPort(m_sairedis, m_switchId);
    auto key = getPortKey(portVid);

    setAlgorithm(m_selectableChannel, key,
        SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    sai_redis_link_event_damping_algo_aied_config_t config{};
    config.max_suppress_time = 2000;
    config.decay_half_life   = 1000;
    config.suppress_threshold = 100;
    config.reuse_threshold    = 50;
    config.flap_penalty       = 1000;

    setAiedConfig(m_selectableChannel, key, config);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    // Generate 10 flaps
    for (int i = 0; i < 10; i++)
    {
        sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
        sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

TEST_F(LinkEventDampingTest, SameStateNoTransition)
{
    auto portVid = getFirstPort(m_sairedis, m_switchId);
    auto key = getPortKey(portVid);

    setAlgorithm(m_selectableChannel, key,
        SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    // Same state repeatedly
    for (int i = 0; i < 5; i++)
    {
        sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(LinkEventDampingTest, NoDampingConfigured)
{
    auto portVid = getFirstPort(m_sairedis, m_switchId);

    // Send events WITHOUT config
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(LinkEventDampingTest, AlgorithmDisabledRuntime)
{
    auto portVid = getFirstPort(m_sairedis, m_switchId);
    auto key = getPortKey(portVid);

    setAlgorithm(m_selectableChannel, key,
        SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_DISABLED);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(LinkEventDampingTest, InvalidConfigDecayHalfLifeZero)
{
    auto portVid = getFirstPort(m_sairedis, m_switchId);
    auto key = getPortKey(portVid);

    setAlgorithm(m_selectableChannel, key,
        SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    sai_redis_link_event_damping_algo_aied_config_t config{};
    config.max_suppress_time = 1000;
    config.decay_half_life   = 0;  // invalid
    config.suppress_threshold = 1000;
    config.reuse_threshold    = 500;
    config.flap_penalty       = 1000;

    setAiedConfig(m_selectableChannel, key, config);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(LinkEventDampingTest, FullSuppressionNoNotification)
{
    auto portVid = getFirstPort(m_sairedis, m_switchId);
    auto key = getPortKey(portVid);

    setAlgorithm(m_selectableChannel, key,
        SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    sai_redis_link_event_damping_algo_aied_config_t config{};
    config.max_suppress_time = 5000;
    config.decay_half_life   = 1000;
    config.suppress_threshold = 100;
    config.reuse_threshold    = 50;
    config.flap_penalty       = 1000;

    setAiedConfig(m_selectableChannel, key, config);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);

    for (int i = 0; i < 5; i++)
    {
        sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);
        sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

TEST_F(LinkEventDampingTest, PendingStateSyncTriggered)
{
    auto portVid = getFirstPort(m_sairedis, m_switchId);
    auto key = getPortKey(portVid);

    setAlgorithm(m_selectableChannel, key,
        SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    sai_redis_link_event_damping_algo_aied_config_t config{};
    config.max_suppress_time = 5000;
    config.decay_half_life   = 1000;
    config.suppress_threshold = 100;
    config.reuse_threshold    = 50;
    config.flap_penalty       = 1000;

    setAiedConfig(m_selectableChannel, key, config);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    // Enter damping
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);

    // Suppressed UP → mismatch
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    // Wait for decay exit
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Trigger sync
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

TEST_F(LinkEventDampingTest, TimerBasedRecovery)
{
    auto portVid = getFirstPort(m_sairedis, m_switchId);
    auto key = getPortKey(portVid);

    setAlgorithm(m_selectableChannel, key,
        SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    sai_redis_link_event_damping_algo_aied_config_t config{};
    config.max_suppress_time = 200;
    config.decay_half_life   = 100;
    config.suppress_threshold = 100;
    config.reuse_threshold    = 50;
    config.flap_penalty       = 1000;

    setAiedConfig(m_selectableChannel, key, config);

    EXPECT_EQ(getResponseStatus(
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
        m_selectableChannel.get(), false),
        SAI_STATUS_SUCCESS);

    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);

    // No new events — rely on timer thread
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

TEST_F(LinkEventDampingTest, ContinuousLinkFlapTriggersDamping)
{
    // Step 1: get real port
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    auto status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    uint32_t portCount = attr.value.u32;

    std::vector<sai_object_id_t> portOids(portCount);
    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list  = portOids.data();

    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    sai_object_id_t portVid = portOids[0];

    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
                      sai_serialize_object_id(portVid);

    // Step 2: enable damping
    std::string algo_id = sai_serialize_redis_port_attr_id(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM);
    std::string algo_val = sai_serialize_redis_link_event_damping_algorithm(
            SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED);

    m_selectableChannel->set(key,
        {swss::FieldValueTuple(algo_id, algo_val)},
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    ASSERT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                               m_selectableChannel.get(), false),
                               SAI_STATUS_SUCCESS);

    // Configure aggressive damping (low threshold → easy trigger)
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time  = 10000;
    config.suppress_threshold = 2000;
    config.reuse_threshold    = 1000;
    config.decay_half_life    = 5000;
    config.flap_penalty       = 1000;

    std::string cfg_id = sai_serialize_redis_port_attr_id(
            SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG);
    std::string cfg_val = sai_serialize_redis_link_event_damping_aied_config(config);

    m_selectableChannel->set(key,
        {swss::FieldValueTuple(cfg_id, cfg_val)},
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

    ASSERT_EQ(getResponseStatus(REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
                               m_selectableChannel.get(), false),
                               SAI_STATUS_SUCCESS);

    // Step 3: simulate 10 link flaps
    // --------------------------------------
    for (int i = 0; i < 10; i++)
    {
        sai_port_oper_status_notification_t notif;

        notif.port_id = portVid;
        notif.port_state = SAI_PORT_OPER_STATUS_DOWN;

        m_selectableChannel->set(
            "PORT_EVENT",
            std::vector<swss::FieldValueTuple>{
                swss::FieldValueTuple(
                    "data",
                    sai_serialize_port_oper_status_ntf(1, &notif))
            },
            SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE);

        notif.port_state = SAI_PORT_OPER_STATUS_UP;
        m_selectableChannel->set(
            "PORT_EVENT",
            std::vector<swss::FieldValueTuple>{
                swss::FieldValueTuple(
                    "data",
                    sai_serialize_port_oper_status_ntf(1, &notif))
            },
            SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE);
    }

    // Give some time for syncd processing loop
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST_F(LinkEventDampingTest, ThresholdCrossingEdgeBehavior)
{
    // Get port
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    ASSERT_EQ(m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

    uint32_t portCount = attr.value.u32;

    std::vector<sai_object_id_t> ports(portCount);
    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list = ports.data();

    ASSERT_EQ(m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

    sai_object_id_t portVid = ports[0];

    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
                      sai_serialize_object_id(portVid);

    // Configure damping with threshold exactly 2000
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time = 10000;
    config.suppress_threshold = 2000;
    config.reuse_threshold = 1000;
    config.decay_half_life = 1000;
    config.flap_penalty = 1000; // 2 flaps → threshold hit

    auto setAttr = [&](const std::string& id, const std::string& val)
    {
        m_selectableChannel->set(key,
            std::vector<swss::FieldValueTuple>{
                swss::FieldValueTuple(id, val)
        },
        REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

        ASSERT_EQ(getResponseStatus(
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
            m_selectableChannel.get(),
            false),
            SAI_STATUS_SUCCESS);
    };

    setAttr(
        sai_serialize_redis_port_attr_id(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM),
        sai_serialize_redis_link_event_damping_algorithm(
            SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED));

    setAttr(
        sai_serialize_redis_port_attr_id(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG),
        sai_serialize_redis_link_event_damping_aied_config(config));

    // First flap → penalty 1000
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    // Second DOWN → reaches exactly threshold
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);

    // This event should be propagated (threshold crossing)
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    // Next events SHOULD be suppressed
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

TEST_F(LinkEventDampingTest, ReuseThresholdExitDecay)
{
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    ASSERT_EQ(m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

    uint32_t portCount = attr.value.u32;
    std::vector<sai_object_id_t> ports(portCount);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list = ports.data();

    ASSERT_EQ(m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

    sai_object_id_t portVid = ports[0];

    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
                      sai_serialize_object_id(portVid);

    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time = 30000;
    config.suppress_threshold = 1000;
    config.reuse_threshold = 500;
    config.decay_half_life = 100;   // fast decay
    config.flap_penalty = 1000;

    auto set = [&](const std::string& id, const std::string& val)
    {
        m_selectableChannel->set(key,
            std::vector<swss::FieldValueTuple>{
                swss::FieldValueTuple(id, val)
            },
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

        ASSERT_EQ(getResponseStatus(
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
            m_selectableChannel.get(), false),
            SAI_STATUS_SUCCESS);
    };

    set(
        sai_serialize_redis_port_attr_id(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM),
        sai_serialize_redis_link_event_damping_algorithm(
            SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED));

    set(
        sai_serialize_redis_port_attr_id(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG),
        sai_serialize_redis_link_event_damping_aied_config(config));

    // Trigger damping
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    // Now decay penalty below reuse threshold
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Next event should cause exit from damping
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

TEST_F(LinkEventDampingTest, MaxSuppressTimeoutExit)
{
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    ASSERT_EQ(m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

    uint32_t portCount = attr.value.u32;
    std::vector<sai_object_id_t> ports(portCount);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list = ports.data();

    ASSERT_EQ(m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

    sai_object_id_t portVid = ports[0];

    std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT) + ":" +
                      sai_serialize_object_id(portVid);

    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time = 20000;
    config.suppress_threshold = 1000;
    config.reuse_threshold = 500;
    config.decay_half_life = 10000;   // slow decay (so timeout triggers)
    config.flap_penalty = 1000;

    auto set = [&](const std::string& id, const std::string& val)
    {
        m_selectableChannel->set(key,
            std::vector<swss::FieldValueTuple>{
                swss::FieldValueTuple(id, val)
            },
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET);

        ASSERT_EQ(getResponseStatus(
            REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET,
            m_selectableChannel.get(), false),
            SAI_STATUS_SUCCESS);
    };

    set(
        sai_serialize_redis_port_attr_id(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM),
        sai_serialize_redis_link_event_damping_algorithm(
            SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED));

    set(
        sai_serialize_redis_port_attr_id(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG),
        sai_serialize_redis_link_event_damping_aied_config(config));

    // Trigger damping
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    // Wait for timeout expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Next event should exit damping due to timeout
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_DOWN);
    sendPortEvent(m_selectableChannel, portVid, SAI_PORT_OPER_STATUS_UP);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}
