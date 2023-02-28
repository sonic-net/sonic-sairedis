// includes -----------------------------------------------------------------------------------------------------------

#include <cstdint>
#include <memory>
#include <vector>
#include <thread>

#include <arpa/inet.h>

#include <gtest/gtest.h>

#include <swss/logger.h>

#include "Sai.h"
#include "Syncd.h"
#include "MetadataLogger.h"

#include "TestSyncdLib.h"

using namespace syncd;

// functions ----------------------------------------------------------------------------------------------------------

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

// Broadcom ASIC ------------------------------------------------------------------------------------------------------

void syncdBrcmWorkerThread()
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
    commandLineOptions->m_enableUnittests = false;
    commandLineOptions->m_enableSaiBulkSupport = true;
    commandLineOptions->m_startType = SAI_START_TYPE_COLD_BOOT;
    commandLineOptions->m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;
    commandLineOptions->m_profileMapFile = "./brcm/testprofile.ini";

    auto syncd = std::make_shared<Syncd>(vendorSai, commandLineOptions, isWarmStart);
    syncd->run();

    SWSS_LOG_NOTICE("Started syncd worker");
}

class SyncdBrcmTest : public ::testing::Test
{
public:
    SyncdBrcmTest() = default;
    virtual ~SyncdBrcmTest() = default;

public:
    virtual void SetUp() override
    {
        SWSS_LOG_ENTER();

        // flush ASIC DB

        flushAsicDb();

        // start syncd worker

        m_worker = std::make_shared<std::thread>(syncdBrcmWorkerThread);

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

    virtual void TearDown() override
    {
        SWSS_LOG_ENTER();

        // uninitialize SAI redis

        auto status = m_sairedis->uninitialize();
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        // stop syncd worker

        sendSyncdShutdownNotification();
        m_worker->join();
    }

protected:
    std::shared_ptr<std::thread> m_worker;
    std::shared_ptr<sairedis::Sai> m_sairedis;
};

TEST_F(SyncdBrcmTest, routeBulkCreate)
{
    sai_object_id_t switchId;
    sai_attribute_t attrs[1];

    // init view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;

    auto status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // create switch

    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = true;

    status = m_sairedis->create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    sai_object_id_t vr = attrs[0].value.oid;

    // create routes bulk routes in init view mode

    std::vector<std::vector<sai_attribute_t>> route_attrs;
    std::vector<const sai_attribute_t *> route_attrs_array;
    std::vector<uint32_t> route_attrs_count;
    std::vector<sai_route_entry_t> routes;

    uint32_t count = 3;

    for (uint32_t i = 0; i < count; ++i)
    {
        sai_route_entry_t route_entry;

        route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        route_entry.destination.addr.ip4 = htonl(0x0a000000 | i);
        route_entry.destination.mask.ip4 = htonl(0xffffffff);
        route_entry.vr_id = vr;
        route_entry.switch_id = switchId;
        route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;

        routes.push_back(route_entry);

        std::vector<sai_attribute_t> list; // no attributes

        route_attrs.push_back(list);
        route_attrs_count.push_back(0);
    }

    for (size_t j = 0; j < route_attrs.size(); j++)
    {
        route_attrs_array.push_back(route_attrs[j].data());
    }

    std::vector<sai_status_t> statuses(count);

    status = m_sairedis->bulkCreate(
            count,
            routes.data(),
            route_attrs_count.data(),
            route_attrs_array.data(),
            SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
            statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // create single route in init view

    sai_route_entry_t route;
    route.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route.destination.addr.ip4 = htonl(0x0b000000);
    route.destination.mask.ip4 = htonl(0xffffffff);
    route.vr_id = vr;
    route.switch_id = switchId;
    route.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;

    status = m_sairedis->create(&route, 0, nullptr);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // apply view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
}
