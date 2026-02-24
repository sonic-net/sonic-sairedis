// includes -----------------------------------------------------------------------------------------------------------

#include <cstdint>
#include <memory>
#include <vector>
#include <array>
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

TEST_F(SyncdBrcmTest, neighborBulkTest)
{
    sai_object_id_t switchId;
    sai_object_id_t rif;
    sai_object_id_t port;
    sai_attribute_t attrs[3];

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

    // create port
    static uint32_t id = 1;
    id++;

    uint32_t hw_lane_list[1] = { id };

    attrs[0].id = SAI_PORT_ATTR_HW_LANE_LIST;
    attrs[0].value.u32list.count = 1;
    attrs[0].value.u32list.list = hw_lane_list;

    attrs[1].id = SAI_PORT_ATTR_SPEED;
    attrs[1].value.u32 = 10000;

    status = m_sairedis->create(SAI_OBJECT_TYPE_PORT, &port, switchId, 2, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // create rif
    attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    attrs[0].value.oid = vr;

    attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    attrs[1].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;

    attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    attrs[2].value.oid = port;

    status = m_sairedis->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, &rif, switchId, 3, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // create neighbor bulk neighbors in init view mode

    std::vector<std::vector<sai_attribute_t>> neighbor_attrs;
    std::vector<const sai_attribute_t *> neighbor_attrs_array;
    std::vector<uint32_t> neighbor_attrs_count;
    std::vector<sai_neighbor_entry_t> neighbors;

    uint32_t count = 3;
    for (uint32_t i = 0; i < count; ++i)
    {
        sai_neighbor_entry_t neighbor_entry;

        neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        neighbor_entry.ip_address.addr.ip4 = 0x10000001 + i;
        neighbor_entry.rif_id = rif;
        neighbor_entry.switch_id = switchId;

        neighbors.push_back(neighbor_entry);

        std::vector<sai_attribute_t> list(1); // no attributes
        sai_attribute_t &neigh_attr = list[0];

        sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        neigh_attr.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
        memcpy(neigh_attr.value.mac, mac, 6);

        neighbor_attrs.push_back(list);
        neighbor_attrs_count.push_back(1);
    }

    for (size_t j = 0; j < neighbor_attrs.size(); j++)
    {
        neighbor_attrs_array.push_back(neighbor_attrs[j].data());
    }

    std::vector<sai_status_t> statuses(count);

    status = m_sairedis->bulkCreate(
            count,
            neighbors.data(),
            neighbor_attrs_count.data(),
            neighbor_attrs_array.data(),
            SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
            statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t j = 0; j < statuses.size(); j++)
    {
        status = statuses[j];
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    statuses.clear();

    for (uint32_t i = 0; i < count; ++i)
    {
        attrs[i].id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
        attrs[i].value.s32 = SAI_PACKET_ACTION_FORWARD;
    }

    status = m_sairedis->bulkSet(
        count,
        neighbors.data(),
        attrs,
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        statuses.data());

    for (size_t j = 0; j < statuses.size(); j++)
    {
        status = statuses[j];
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    statuses.clear();

    status = m_sairedis->bulkRemove(
        count,
        neighbors.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        statuses.data());

    for (size_t j = 0; j < statuses.size(); j++)
    {
        status = statuses[j];
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }
}

TEST_F(SyncdBrcmTest, nexthopBulkTest)
{
    sai_object_id_t switchId;
    sai_object_id_t rif;
    sai_object_id_t port;
    sai_attribute_t attrs[3];

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

    // create port
    static uint32_t id = 1;
    id++;

    uint32_t hw_lane_list[1] = { id };

    attrs[0].id = SAI_PORT_ATTR_HW_LANE_LIST;
    attrs[0].value.u32list.count = 1;
    attrs[0].value.u32list.list = hw_lane_list;

    attrs[1].id = SAI_PORT_ATTR_SPEED;
    attrs[1].value.u32 = 10000;

    status = m_sairedis->create(SAI_OBJECT_TYPE_PORT, &port, switchId, 2, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    // create rif
    attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    attrs[0].value.oid = vr;

    attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    attrs[1].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;

    attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    attrs[2].value.oid = port;

    status = m_sairedis->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, &rif, switchId, 3, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    std::vector<std::vector<sai_attribute_t>> nh_attrs;
    std::vector<const sai_attribute_t *> nh_attrs_array;
    std::vector<uint32_t> nh_attrs_count;
    uint32_t count = 3;
    for (uint32_t i = 0; i < count; ++i)
    {
        std::vector<sai_attribute_t> list(3);
        sai_attribute_t &nhattr0 = list[0];
        sai_attribute_t &nhattr1 = list[1];
        sai_attribute_t &nhattr2 = list[2];

        nhattr0.id = SAI_NEXT_HOP_ATTR_TYPE;
        nhattr0.value.s32 = SAI_NEXT_HOP_TYPE_IP;
        nhattr1.id = SAI_NEXT_HOP_ATTR_IP;
        nhattr1.value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        nhattr1.value.ipaddr.addr.ip4 = 0x10000001;
        nhattr2.id = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID;
        nhattr2.value.oid = rif;

        nh_attrs.push_back(list);
        nh_attrs_count.push_back(3);
    }

    for (size_t j = 0; j < nh_attrs.size(); j++)
    {
        nh_attrs_array.push_back(nh_attrs[j].data());
    }

    std::vector<sai_status_t> statuses(count);
    std::vector<sai_object_id_t> object_id(count);
    status = m_sairedis->bulkCreate(
        (sai_object_type_t) SAI_OBJECT_TYPE_NEXT_HOP, switchId,
        count, nh_attrs_count.data(), nh_attrs_array.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        object_id.data(), statuses.data());

    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t j = 0; j < statuses.size(); j++)
    {
        status = statuses[j];
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    statuses.clear();

    status = m_sairedis->bulkRemove(
        (sai_object_type_t) SAI_OBJECT_TYPE_NEXT_HOP,
        count, object_id.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        statuses.data());
    for (size_t j = 0; j < statuses.size(); j++)
    {
        status = statuses[j];
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }
}

TEST_F(SyncdBrcmTest, portBufferBulkSet)
{
    sai_object_id_t switchId;
    sai_attribute_t attrs[1];

    struct
    {
        std::vector<sai_object_id_t> oids;
        std::vector<sai_attribute_t> attrs;
        std::vector<sai_status_t> statuses;

        void resize(size_t size)
        {
            SWSS_LOG_ENTER();

            oids.resize(size);
            attrs.resize(size);
            statuses.resize(size, SAI_STATUS_NOT_EXECUTED);
        }
    } ports, pgs, queues;

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

    attrs[0].id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    ports.resize(attrs[0].value.u32);

    ASSERT_TRUE(ports.oids.size() > 1);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(ports.oids.size());
    attrs[0].value.objlist.list = ports.oids.data();
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Set port admin status in bulk

    for (size_t i = 0; i < ports.oids.size(); i++)
    {
        ports.attrs[i].id = SAI_PORT_ATTR_ADMIN_STATE;
        ports.attrs[i].value.booldata = true;
    }

    status = m_sairedis->bulkSet(SAI_OBJECT_TYPE_PORT, static_cast<uint32_t>(ports.oids.size()), ports.oids.data(),
        ports.attrs.data(), SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, ports.statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < ports.oids.size(); i++)
    {
        ASSERT_EQ(ports.statuses[i], SAI_STATUS_SUCCESS);
    }

    // Create buffer pool

    std::array<sai_attribute_t, 4> buffer_pool_attrs;
    sai_object_id_t buffer_pool;

    buffer_pool_attrs[0].id = SAI_BUFFER_POOL_ATTR_THRESHOLD_MODE;
    buffer_pool_attrs[0].value.u32 = SAI_BUFFER_POOL_THRESHOLD_MODE_DYNAMIC;

    buffer_pool_attrs[1].id = SAI_BUFFER_POOL_ATTR_SIZE;
    buffer_pool_attrs[1].value.u32 = 47218432;

    buffer_pool_attrs[2].id = SAI_BUFFER_POOL_ATTR_TYPE;
    buffer_pool_attrs[2].value.u32 = SAI_BUFFER_POOL_TYPE_INGRESS;

    buffer_pool_attrs[3].id = SAI_BUFFER_POOL_ATTR_XOFF_SIZE;
    buffer_pool_attrs[3].value.u32 = 17708800;

    status = m_sairedis->create(SAI_OBJECT_TYPE_BUFFER_POOL, &buffer_pool, switchId,
        static_cast<uint32_t>(buffer_pool_attrs.size()), buffer_pool_attrs.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Create buffer profile

    std::array<sai_attribute_t, 4> buffer_profile_attrs;
    sai_object_id_t buffer_profile;

    buffer_profile_attrs[0].id = SAI_BUFFER_PROFILE_ATTR_POOL_ID;
    buffer_profile_attrs[0].value.oid = buffer_pool;

    buffer_profile_attrs[1].id = SAI_BUFFER_PROFILE_ATTR_THRESHOLD_MODE;
    buffer_profile_attrs[1].value.u32 = SAI_BUFFER_PROFILE_THRESHOLD_MODE_DYNAMIC;

    buffer_profile_attrs[2].id = SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH;
    buffer_profile_attrs[2].value.s8 = -8;

    buffer_profile_attrs[3].id = SAI_BUFFER_PROFILE_ATTR_RESERVED_BUFFER_SIZE;
    buffer_profile_attrs[3].value.u64 = 6755399441055744;

    status = m_sairedis->create(SAI_OBJECT_TYPE_BUFFER_PROFILE, &buffer_profile, switchId,
        static_cast<uint32_t>(buffer_profile_attrs.size()), buffer_profile_attrs.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Priority group configuration

    attrs[0].id = SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS;
    status = m_sairedis->get(SAI_OBJECT_TYPE_PORT, ports.oids[0], 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    pgs.resize(attrs[0].value.u32);

    attrs[0].id = SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(pgs.oids.size());
    attrs[0].value.objlist.list = pgs.oids.data();

    status = m_sairedis->get(SAI_OBJECT_TYPE_PORT, ports.oids[0], 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < pgs.oids.size(); i++)
    {
        pgs.attrs[i].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE;
        pgs.attrs[i].value.oid = buffer_profile;
    }

    status = m_sairedis->bulkSet(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, static_cast<uint32_t>(pgs.oids.size()), pgs.oids.data(),
        pgs.attrs.data(), SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, pgs.statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < pgs.oids.size(); i++)
    {
        ASSERT_EQ(pgs.statuses[i], SAI_STATUS_SUCCESS);
    }

    // Queue configuration

    attrs[0].id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;
    status = m_sairedis->get(SAI_OBJECT_TYPE_PORT, ports.oids[0], 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    queues.resize(attrs[0].value.u32);

    attrs[0].id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(queues.oids.size());
    attrs[0].value.objlist.list = queues.oids.data();

    status = m_sairedis->get(SAI_OBJECT_TYPE_PORT, ports.oids[0], 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < queues.oids.size(); i++)
    {
        queues.attrs[i].id = SAI_QUEUE_ATTR_BUFFER_PROFILE_ID;
        queues.attrs[i].value.oid = buffer_profile;
    }

    status = m_sairedis->bulkSet(SAI_OBJECT_TYPE_QUEUE, static_cast<uint32_t>(queues.oids.size()), queues.oids.data(),
        queues.attrs.data(), SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, queues.statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < queues.oids.size(); i++)
    {
        ASSERT_EQ(queues.statuses[i], SAI_STATUS_SUCCESS);
    }

    // apply view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);
}

TEST_F(SyncdBrcmTest, portBulkSetInInitView)
{
    sai_object_id_t switchId;
    sai_attribute_t attrs[1];

    struct
    {
        std::vector<sai_object_id_t> oids;
        std::vector<sai_attribute_t> attrs;
        std::vector<sai_status_t> statuses;

        void resize(size_t size)
        {
            SWSS_LOG_ENTER();

            oids.resize(size);
            attrs.resize(size);
            statuses.resize(size, SAI_STATUS_NOT_EXECUTED);
        }
    } ports;

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

    // apply view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // init view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // create switch

    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = true;

    status = m_sairedis->create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    ports.resize(attrs[0].value.u32);

    ASSERT_TRUE(ports.oids.size() > 1);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(ports.oids.size());
    attrs[0].value.objlist.list = ports.oids.data();
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Set port admin status in bulk

    for (size_t i = 0; i < ports.oids.size(); i++)
    {
        ports.attrs[i].id = SAI_PORT_ATTR_ADMIN_STATE;
        ports.attrs[i].value.booldata = true;
    }

    status = m_sairedis->bulkSet(SAI_OBJECT_TYPE_PORT, static_cast<uint32_t>(ports.oids.size()), ports.oids.data(),
        ports.attrs.data(), SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, ports.statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < ports.oids.size(); i++)
    {
        ASSERT_EQ(ports.statuses[i], SAI_STATUS_SUCCESS);
    }
}

TEST_F(SyncdBrcmTest, pgBulkSetInInitView)
{
    sai_object_id_t switchId;
    sai_attribute_t attrs[1];

    std::vector<sai_object_id_t> portOids;

    struct
    {
        std::vector<sai_object_id_t> oids;
        std::vector<sai_attribute_t> attrs;
        std::vector<sai_status_t> statuses;

        void resize(size_t size)
        {
            SWSS_LOG_ENTER();

            oids.resize(size);
            attrs.resize(size);
            statuses.resize(size, SAI_STATUS_NOT_EXECUTED);
        }
    } pgs;

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

    // apply view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // init view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // create switch

    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = true;

    status = m_sairedis->create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    portOids.resize(attrs[0].value.u32);

    ASSERT_TRUE(portOids.size() > 1);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(portOids.size());
    attrs[0].value.objlist.list = portOids.data();
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(portOids.size());
    attrs[0].value.objlist.list = portOids.data();
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Create buffer pool

    std::array<sai_attribute_t, 4> buffer_pool_attrs;
    sai_object_id_t buffer_pool;

    buffer_pool_attrs[0].id = SAI_BUFFER_POOL_ATTR_THRESHOLD_MODE;
    buffer_pool_attrs[0].value.u32 = SAI_BUFFER_POOL_THRESHOLD_MODE_DYNAMIC;

    buffer_pool_attrs[1].id = SAI_BUFFER_POOL_ATTR_SIZE;
    buffer_pool_attrs[1].value.u32 = 47218432;

    buffer_pool_attrs[2].id = SAI_BUFFER_POOL_ATTR_TYPE;
    buffer_pool_attrs[2].value.u32 = SAI_BUFFER_POOL_TYPE_INGRESS;

    buffer_pool_attrs[3].id = SAI_BUFFER_POOL_ATTR_XOFF_SIZE;
    buffer_pool_attrs[3].value.u32 = 17708800;

    status = m_sairedis->create(SAI_OBJECT_TYPE_BUFFER_POOL, &buffer_pool, switchId,
        static_cast<uint32_t>(buffer_pool_attrs.size()), buffer_pool_attrs.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Create buffer profile

    std::array<sai_attribute_t, 4> buffer_profile_attrs;
    sai_object_id_t buffer_profile;

    buffer_profile_attrs[0].id = SAI_BUFFER_PROFILE_ATTR_POOL_ID;
    buffer_profile_attrs[0].value.oid = buffer_pool;

    buffer_profile_attrs[1].id = SAI_BUFFER_PROFILE_ATTR_THRESHOLD_MODE;
    buffer_profile_attrs[1].value.u32 = SAI_BUFFER_PROFILE_THRESHOLD_MODE_DYNAMIC;

    buffer_profile_attrs[2].id = SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH;
    buffer_profile_attrs[2].value.s8 = -8;

    buffer_profile_attrs[3].id = SAI_BUFFER_PROFILE_ATTR_RESERVED_BUFFER_SIZE;
    buffer_profile_attrs[3].value.u64 = 6755399441055744;

    status = m_sairedis->create(SAI_OBJECT_TYPE_BUFFER_PROFILE, &buffer_profile, switchId,
        static_cast<uint32_t>(buffer_profile_attrs.size()), buffer_profile_attrs.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Priority group configuration

    attrs[0].id = SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS;
    status = m_sairedis->get(SAI_OBJECT_TYPE_PORT, portOids[0], 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    pgs.resize(attrs[0].value.u32);

    attrs[0].id = SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(pgs.oids.size());
    attrs[0].value.objlist.list = pgs.oids.data();

    status = m_sairedis->get(SAI_OBJECT_TYPE_PORT, portOids[0], 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < pgs.oids.size(); i++)
    {
        pgs.attrs[i].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE;
        pgs.attrs[i].value.oid = buffer_profile;
    }

    status = m_sairedis->bulkSet(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, static_cast<uint32_t>(pgs.oids.size()), pgs.oids.data(),
        pgs.attrs.data(), SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, pgs.statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < pgs.oids.size(); i++)
    {
        ASSERT_EQ(pgs.statuses[i], SAI_STATUS_SUCCESS);
    }
}

TEST_F(SyncdBrcmTest, bulkSetInInitViewForUnsupportedObjects)
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

    // apply view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // init view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // create switch

    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = true;

    status = m_sairedis->create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_SWITCH_ATTR_UNINIT_DATA_PLANE_ON_REMOVAL;
    attrs[0].value.booldata = true;

    sai_object_id_t oids[1] = {switchId};
    sai_status_t statuses[1] = {SAI_STATUS_NOT_EXECUTED};

    ASSERT_THROW(m_sairedis->bulkSet(SAI_OBJECT_TYPE_SWITCH, 1, oids, attrs,
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses), std::runtime_error);
}

struct GetBulk
{
    std::vector<sai_object_id_t> oids;
    std::vector<uint32_t> attr_counts;
    std::vector<sai_attribute_t*> attrs;
    std::vector<sai_status_t> statuses;

    std::vector<std::vector<sai_attribute_t>> attrsContainer;

    void resize(size_t size, uint32_t attr_count)
    {
        SWSS_LOG_ENTER();

        oids.resize(size);
        attr_counts.resize(size);
        attrs.resize(size);
        attrsContainer.resize(size);
        for (uint32_t i = 0; i < size; i++)
        {
            attrsContainer[i].resize(attr_count);
            attr_counts[i] = attr_count;
        }
        statuses.resize(size, SAI_STATUS_NOT_EXECUTED);
    }
};

TEST_F(SyncdBrcmTest, portBulkGet)
{
    sai_object_id_t switchId;
    sai_attribute_t attrs[1];

    GetBulk portGetBulk;

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

    // Get port number

    attrs[0].id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    const auto portNumber = attrs[0].value.u32;
    ASSERT_TRUE(attrs[0].value.u32 > 1);

    portGetBulk.resize(portNumber, 2);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(portGetBulk.oids.size());
    attrs[0].value.objlist.list = portGetBulk.oids.data();
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Get port attributes in bulk

    for (size_t i = 0; i < portGetBulk.oids.size(); i++)
    {
        portGetBulk.attrsContainer[i][0].id = SAI_PORT_ATTR_ADMIN_STATE;
        portGetBulk.attrsContainer[i][1].id = SAI_PORT_ATTR_SPEED;

        portGetBulk.attrs[i] = portGetBulk.attrsContainer[i].data();
    }

    status = m_sairedis->bulkGet(SAI_OBJECT_TYPE_PORT, static_cast<uint32_t>(portGetBulk.oids.size()), portGetBulk.oids.data(),
        portGetBulk.attr_counts.data(), portGetBulk.attrs.data(), SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, portGetBulk.statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < portGetBulk.oids.size(); i++)
    {
        ASSERT_EQ(portGetBulk.statuses[i], SAI_STATUS_SUCCESS);
        ASSERT_EQ(portGetBulk.attrs[i][0].value.booldata, false);
        ASSERT_EQ(portGetBulk.attrs[i][1].value.u32, 40000);
    }
}

TEST_F(SyncdBrcmTest, portBulkGetInInitView)
{
    sai_object_id_t switchId;
    sai_attribute_t attrs[1];

    GetBulk portGetBulk;

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

    // apply view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // init view

    attrs[0].id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attrs[0].value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;

    status = m_sairedis->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // create switch

    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = true;

    status = m_sairedis->create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Get port number

    attrs[0].id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    const auto portNumber = attrs[0].value.u32;
    ASSERT_TRUE(attrs[0].value.u32 > 1);

    portGetBulk.resize(portNumber, 2);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(portGetBulk.oids.size());
    attrs[0].value.objlist.list = portGetBulk.oids.data();
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Get port attributes in bulk

    for (size_t i = 0; i < portGetBulk.oids.size(); i++)
    {
        portGetBulk.attrsContainer[i][0].id = SAI_PORT_ATTR_ADMIN_STATE;
        portGetBulk.attrsContainer[i][1].id = SAI_PORT_ATTR_SPEED;

        portGetBulk.attrs[i] = portGetBulk.attrsContainer[i].data();
    }

    status = m_sairedis->bulkGet(SAI_OBJECT_TYPE_PORT, static_cast<uint32_t>(portGetBulk.oids.size()), portGetBulk.oids.data(),
        portGetBulk.attr_counts.data(), portGetBulk.attrs.data(), SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, portGetBulk.statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < portGetBulk.oids.size(); i++)
    {
        ASSERT_EQ(portGetBulk.statuses[i], SAI_STATUS_SUCCESS);
        ASSERT_EQ(portGetBulk.attrs[i][0].value.booldata, false);
        ASSERT_EQ(portGetBulk.attrs[i][1].value.u32, 40000);
    }
}

TEST_F(SyncdBrcmTest, portBulkGetObjectList)
{
    sai_object_id_t switchId;
    sai_attribute_t attrs[1];

    GetBulk portGetBulk;

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

    attrs[0].id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    const auto portNumber = attrs[0].value.u32;
    ASSERT_TRUE(attrs[0].value.u32 > 1);

    portGetBulk.resize(portNumber, 2);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(portGetBulk.oids.size());
    attrs[0].value.objlist.list = portGetBulk.oids.data();
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    // Query number of queues

    attrs[0].id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;
    status = m_sairedis->get(SAI_OBJECT_TYPE_PORT, portGetBulk.oids[0], 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    std::vector<std::vector<sai_object_id_t>> queues;
    queues.resize(portGetBulk.oids.size());

    for (size_t i = 0; i < queues.size(); i++)
    {
        queues[i].resize(attrs[0].value.u32);
    }

    // Get port attributes in bulk

    portGetBulk.resize(portNumber, 1);
    for (size_t i = 0; i < portGetBulk.oids.size(); i++)
    {
        // Query QUEUE_LIST to test object list get
        portGetBulk.attrsContainer[i][0].id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
        portGetBulk.attrsContainer[i][0].value.objlist.count = static_cast<uint32_t>(queues[i].size());
        portGetBulk.attrsContainer[i][0].value.objlist.list = queues[i].data();

        portGetBulk.attrs[i] = portGetBulk.attrsContainer[i].data();
    }

    status = m_sairedis->bulkGet(SAI_OBJECT_TYPE_PORT, static_cast<uint32_t>(portGetBulk.oids.size()), portGetBulk.oids.data(),
        portGetBulk.attr_counts.data(), portGetBulk.attrs.data(), SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, portGetBulk.statuses.data());
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (size_t i = 0; i < portGetBulk.oids.size(); i++)
    {
        ASSERT_EQ(portGetBulk.statuses[i], SAI_STATUS_SUCCESS);
    }
}

TEST_F(SyncdBrcmTest, portBulkGetObjectListInsufficientBuffer)
{
    sai_object_id_t switchId;
    sai_attribute_t attrs[1];

    GetBulk portGetBulk;

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

    attrs[0].id = SAI_SWITCH_ATTR_PORT_NUMBER;
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    const auto portNumber = attrs[0].value.u32;
    ASSERT_TRUE(attrs[0].value.u32 > 1);

    portGetBulk.resize(portNumber, 1);

    attrs[0].id = SAI_SWITCH_ATTR_PORT_LIST;
    attrs[0].value.objlist.count = static_cast<uint32_t>(portGetBulk.oids.size());
    attrs[0].value.objlist.list = portGetBulk.oids.data();
    status = m_sairedis->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, attrs);
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    std::vector<std::vector<sai_object_id_t>> queues;
    queues.resize(portGetBulk.oids.size());

    for (size_t i = 0; i < queues.size(); i++)
    {
        queues[i].resize(1); // Insufficient size
    }

    // Get port attributes in bulk

    portGetBulk.resize(portNumber, 1);
    for (size_t i = 0; i < portGetBulk.oids.size(); i++)
    {
        // Query QUEUE_LIST to test object list get
        portGetBulk.attrsContainer[i][0].id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
        portGetBulk.attrsContainer[i][0].value.objlist.count = static_cast<uint32_t>(queues[i].size());
        portGetBulk.attrsContainer[i][0].value.objlist.list = queues[i].data();

        portGetBulk.attrs[i] = portGetBulk.attrsContainer[i].data();
    }

    m_sairedis->bulkGet(SAI_OBJECT_TYPE_PORT, static_cast<uint32_t>(portGetBulk.oids.size()), portGetBulk.oids.data(),
        portGetBulk.attr_counts.data(), portGetBulk.attrs.data(), SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, portGetBulk.statuses.data());

    for (size_t i = 0; i < portGetBulk.oids.size(); i++)
    {
        ASSERT_EQ(portGetBulk.statuses[i], SAI_STATUS_BUFFER_OVERFLOW);
    }
}
