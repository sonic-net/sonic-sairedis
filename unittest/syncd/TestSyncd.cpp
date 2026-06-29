#include "Syncd.h"
#include "RedisClient.h"
#include "sai_serialize.h"
#include "RequestShutdown.h"
#include "vslib/ContextConfigContainer.h"
#include "vslib/VirtualSwitchSaiInterface.h"
#include "vslib/Sai.h"
#include "lib/Sai.h"

#include "swss/dbconnector.h"

#include "sairediscommon.h"

#include "MockableSaiInterface.h"
#include "CommandLineOptions.h"
#include "sairediscommon.h"
#include "SelectableChannel.h"
#include "swss/dbconnector.h"
#include "swss/redisreply.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <thread>

#include "swss/table.h"

using namespace syncd;
using namespace saivs;
using namespace testing;

static void syncd_thread(
        _In_ std::shared_ptr<Syncd> syncd)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("thread stared");

    syncd->run();

    SWSS_LOG_NOTICE("thread end");
}

static std::map<std::string, std::string> profileMap;
static std::map<std::string, std::string>::iterator profileIter;

static const char* profileGetValue(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return NULL;
    }

    auto it = profileMap.find(variable);

    if (it == profileMap.end())
    {
        SWSS_LOG_NOTICE("%s: NULL", variable);
        return NULL;
    }

    SWSS_LOG_NOTICE("%s: %s", variable, it->second.c_str());

    return it->second.c_str();
}

static int profileGetNextValue(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        profileIter = profileMap.begin();
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    if (profileIter == profileMap.end())
    {
        SWSS_LOG_INFO("iterator reached end");
        return -1;
    }

    *variable = profileIter->first.c_str();
    *value = profileIter->second.c_str();

    SWSS_LOG_INFO("key: %s:%s", *variable, *value);

    profileIter++;

    return 0;
}

TEST(Syncd, inspectAsic)
{
    auto db = std::make_shared<swss::DBConnector>("ASIC_DB", 0, true);

    swss::RedisReply r(db.get(), "FLUSHALL", REDIS_REPLY_STATUS);

    r.checkStatusOK();

    sai_service_method_table_t smt;

    smt.profile_get_value = &profileGetValue;
    smt.profile_get_next_value = &profileGetNextValue;

    auto vssai = std::make_shared<saivs::Sai>();

    auto cmd = std::make_shared<CommandLineOptions>();

    cmd->m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;
    cmd->m_enableTempView = true;
    cmd->m_profileMapFile = "profile.ini";

    auto syncd = std::make_shared<Syncd>(vssai, cmd, false);

    std::thread thread(syncd_thread, syncd);

    auto sai = std::make_shared<sairedis::Sai>();

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->apiInitialize(0, &smt));

    sai_attribute_t attr;

    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;

    // set syncd mode on sairedis

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->set(SAI_OBJECT_TYPE_SWITCH, SAI_NULL_OBJECT_ID, &attr));

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    sai_object_id_t switchId;

    // create switch

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

    // get default virtual router

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, &attr));

    sai_object_id_t routerId = attr.value.oid;

    sai_object_id_t list[32];

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = 32;
    attr.value.objlist.list = list;

    // get port list

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->get(SAI_OBJECT_TYPE_SWITCH, switchId, 1, &attr));

    sai_attribute_t attrs[4];

    attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    attrs[0].value.oid = routerId;
    attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
    attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    attrs[2].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;
    attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    attrs[3].value.oid = list[0];

    // create router interface, we need oid with oid attributes
    // to validate inspect asic routine

    sai_object_id_t rifId;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, &rifId, switchId, 4, attrs));

    attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC;

    // inspect asic on cold boot

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->set(SAI_OBJECT_TYPE_SWITCH, switchId, &attr));

    // request shutdown

    auto opt = std::make_shared<RequestShutdownCommandLineOptions>();

    opt->setRestartType(SYNCD_RESTART_TYPE_WARM);

    RequestShutdown rs(opt);

    rs.send();

    // join thread for syncd

    thread.join();

    syncd = nullptr;

    // TODO inspect asic on warm boot

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->apiUninitialize());
}

TEST(Syncd, zmqSyncWithJsonDisabledFallsBackToRedisSync)
{
    auto sai = std::make_shared<MockableSaiInterface>();
    auto cmd = std::make_shared<CommandLineOptions>();

    cmd->m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;
    cmd->m_contextConfig = "files/ctx_zmq_disabled.json";

    auto syncd = std::make_shared<Syncd>(sai, cmd, false);

    EXPECT_EQ(cmd->m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC);
    EXPECT_TRUE(syncd->m_enableSyncMode);
}

TEST(Syncd, zmqSyncWithJsonEnabledUsesZmq)
{
    auto sai = std::make_shared<MockableSaiInterface>();
    auto cmd = std::make_shared<CommandLineOptions>();

    cmd->m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;
    cmd->m_contextConfig = "files/ctx_zmq_enabled.json";

    auto syncd = std::make_shared<Syncd>(sai, cmd, false);

    EXPECT_EQ(cmd->m_redisCommunicationMode, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);
    EXPECT_TRUE(syncd->m_enableSyncMode);
}

using namespace syncd;

#ifdef MOCK_METHOD
class MockSelectableChannel : public sairedis::SelectableChannel {
public:
    MOCK_METHOD(bool, empty, (), (override));
    MOCK_METHOD(void, pop, (swss::KeyOpFieldsValuesTuple& kco, bool initViewMode), (override));
    MOCK_METHOD(void, set, (const std::string& key, const std::vector<swss::FieldValueTuple>& values, const std::string& op), (override));
    MOCK_METHOD(int, getFd, (), (override));
    MOCK_METHOD(uint64_t, readData, (), (override));
};

void clearDB()
{
    SWSS_LOG_ENTER();

    swss::DBConnector db("ASIC_DB", 0, true);
    swss::RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);

    r.checkStatusOK();
}

class MockSaiSwitch : public SaiSwitch {
public:
    MockSaiSwitch(sai_object_id_t switchVid, sai_object_id_t switchRid,
                  std::shared_ptr<BaseRedisClient> client,
                  std::shared_ptr<VirtualOidTranslator> translator,
                  std::shared_ptr<MockableSaiInterface> sai, bool warmBoot)
        : SaiSwitch(switchVid, switchRid, client, translator, sai, warmBoot) {}

    MOCK_METHOD(void, postPortRemove, (sai_object_id_t portRid), (override));
    MOCK_METHOD(void, removeExistingObjectReference, (sai_object_id_t rid), (override));
    MOCK_METHOD(void, eraseRidAndVid, (sai_object_id_t rid, sai_object_id_t vid));
};

class SyncdTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_sai = std::make_shared<MockableSaiInterface>();
        m_opt = std::make_shared<CommandLineOptions>();
        m_syncd = std::make_shared<Syncd>(m_sai, m_opt, false);

        m_opt->m_enableTempView = true;
        m_opt->m_startType = SAI_START_TYPE_FASTFAST_BOOT;
        clearDB();
    }
    void TearDown() override
    {
        clearDB();
    }

    std::shared_ptr<MockableSaiInterface> m_sai;
    std::shared_ptr<CommandLineOptions> m_opt;
    std::shared_ptr<Syncd> m_syncd;
};

TEST_F(SyncdTest, processNotifySyncd)
{
    auto sai = std::make_shared<MockableSaiInterface>();
    auto opt = std::make_shared<syncd::CommandLineOptions>();
    opt->m_enableTempView = true;
    opt->m_startType = SAI_START_TYPE_FASTFAST_BOOT;
    syncd::Syncd syncd_object(sai, opt, false);

    MockSelectableChannel consumer;
    EXPECT_CALL(consumer, empty()).WillOnce(testing::Return(true));
    EXPECT_CALL(consumer, pop(testing::_, testing::_)).WillOnce(testing::Invoke([](swss::KeyOpFieldsValuesTuple& kco, bool initViewMode) {
        kfvKey(kco) = SYNCD_APPLY_VIEW;
        kfvOp(kco) = REDIS_ASIC_STATE_COMMAND_NOTIFY;
    }));
    syncd_object.processEvent(consumer);
}

TEST_F(SyncdTest, processStatsCapabilityQuery)
{
    auto sai = std::make_shared<MockableSaiInterface>();
    auto opt = std::make_shared<syncd::CommandLineOptions>();
    opt->m_enableTempView = true;
    opt->m_startType = SAI_START_TYPE_FASTFAST_BOOT;
    syncd::Syncd syncd_object(sai, opt, false);

    auto translator = syncd_object.m_translator;
    translator->insertRidAndVid(0x11000000000000, 0x21000000000000);

    MockSelectableChannel consumer;
    EXPECT_CALL(consumer, empty()).WillOnce(testing::Return(true));
    EXPECT_CALL(consumer, pop(testing::_, testing::_))
        .Times(1)
        .WillRepeatedly(testing::Invoke([](swss::KeyOpFieldsValuesTuple& kco, bool initViewMode) {
        static int callCount = 0;
        if (callCount == 0)
        {
            kfvKey(kco) = "oid:0x21000000000000";
            kfvOp(kco) = REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_QUERY;
            kfvFieldsValues(kco) = {
                std::make_pair("OBJECT_TYPE", "SAI_OBJECT_TYPE_QUEUE"),
                std::make_pair("LIST_SIZE", "1")
        };
        }
        else
        {
                kfvKey(kco) = "";
                kfvOp(kco) = "";
                kfvFieldsValues(kco).clear();
        }
        ++callCount;
    }));
    syncd_object.processEvent(consumer);
}

TEST_F(SyncdTest, processStatsStCapabilityQuery)
{
    auto sai = std::make_shared<MockableSaiInterface>();
    auto opt = std::make_shared<syncd::CommandLineOptions>();
    opt->m_enableTempView = true;
    opt->m_startType = SAI_START_TYPE_FASTFAST_BOOT;
    syncd::Syncd syncd_object(sai, opt, false);

    auto translator = syncd_object.m_translator;
    translator->insertRidAndVid(0x11000000000000, 0x21000000000000);

    MockSelectableChannel consumer;
    EXPECT_CALL(consumer, empty()).WillOnce(testing::Return(true));
    EXPECT_CALL(consumer, pop(testing::_, testing::_))
        .Times(1)
        .WillRepeatedly(testing::Invoke([](swss::KeyOpFieldsValuesTuple &kco, bool initViewMode)
                                        {
        static int callCount = 0;
        if (callCount == 0)
        {
            kfvKey(kco) = "oid:0x21000000000000";
            kfvOp(kco) = REDIS_ASIC_STATE_COMMAND_STATS_ST_CAPABILITY_QUERY;
            kfvFieldsValues(kco) = {
                std::make_pair("OBJECT_TYPE", "SAI_OBJECT_TYPE_QUEUE"),
                std::make_pair("LIST_SIZE", "1")
        };
        }
        else
        {
                kfvKey(kco) = "";
                kfvOp(kco) = "";
                kfvFieldsValues(kco).clear();
        }
        ++callCount; }));
    syncd_object.processEvent(consumer);
}

TEST_F(SyncdTest, BulkCreateTest)
{
    m_opt->m_enableSaiBulkSupport = true;

    sai_object_id_t switchVid = 0x21000000000000;
    sai_object_id_t switchRid = 0x11000000000001;

    sai_object_id_t portVid = 0x10000000000002;
    sai_object_id_t portRid = 0x11000000000002;

    auto translator = m_syncd->m_translator;
    translator->insertRidAndVid(switchRid, switchVid);
    translator->insertRidAndVid(portRid, portVid);

    std::vector<uint32_t> lanes = {52, 53};
    m_syncd->m_client->setPortLanes(switchVid, portRid, lanes);

    m_sai->mock_objectTypeQuery = [switchRid, portRid](sai_object_id_t oid) {
        sai_object_type_t ot = SAI_OBJECT_TYPE_NULL;
        if (oid == switchRid)
            ot = SAI_OBJECT_TYPE_SWITCH;
        else if (oid == portRid)
            ot = SAI_OBJECT_TYPE_PORT;
        else
            ot = SAI_OBJECT_TYPE_QUEUE;
        return ot;
    };

    m_sai->mock_switchIdQuery = [switchVid](sai_object_id_t oid) {
        return switchVid;
    };

    m_sai->mock_get = [switchRid, portVid, portRid](sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attrCount, sai_attribute_t* attrList) -> sai_status_t {
        if (attrCount != 1) {
            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (attrList[0].id == SAI_SWITCH_ATTR_PORT_LIST) {
            if (objectType == SAI_OBJECT_TYPE_SWITCH && objectId == switchRid) {
                attrList[0].value.objlist.count = 1;
                attrList[0].value.objlist.list[0] = portRid;
                return SAI_STATUS_SUCCESS;
            }
            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (attrList[0].id == SAI_SWITCH_ATTR_PORT_NUMBER) {
            attrList[0].value.u32 = 1;
            return SAI_STATUS_SUCCESS;
        }

        if (objectType == SAI_OBJECT_TYPE_PORT) {
            if (attrList[0].id == SAI_PORT_ATTR_HW_LANE_LIST) {
                if (objectId == portRid) {
                    if (attrList[0].value.u32list.list != nullptr) {
                        attrList[0].value.u32list.count = 2;
                        static uint32_t hw_lanes[2] = {52, 53};
                        memcpy(attrList[0].value.u32list.list, hw_lanes, sizeof(uint32_t) * 2);
                    } else {
                        return SAI_STATUS_BUFFER_OVERFLOW;
                    }
                }
            }

            if (attrList[0].id == SAI_PORT_ATTR_PORT_SERDES_ID) {
                attrList[0].value.oid = SAI_NULL_OBJECT_ID;
                return SAI_STATUS_SUCCESS;
            }

            return SAI_STATUS_SUCCESS;
        }

        if (attrList[0].id == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS) {
            if (objectType == SAI_OBJECT_TYPE_SWITCH && objectId == switchRid) {
                static sai_mac_t mac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
                memcpy(attrList[0].value.mac, mac, sizeof(sai_mac_t));
                return SAI_STATUS_SUCCESS;
            }
            return SAI_STATUS_INVALID_PARAMETER;
        }

        return SAI_STATUS_NOT_SUPPORTED;
    };

    // Create a mock switch and set expectations
    auto mockSwitch = std::make_shared<MockSaiSwitch>(switchVid, switchRid, m_syncd->m_client, translator, m_sai, false);
    m_syncd->m_switches[switchVid] = mockSwitch;
    EXPECT_CALL(*mockSwitch, postPortRemove(testing::_))
        .WillRepeatedly(testing::Invoke([](sai_object_id_t rid) {
        }));

    EXPECT_CALL(*mockSwitch, removeExistingObjectReference(testing::_))
        .WillRepeatedly(testing::Invoke([](sai_object_id_t rid) {
        }));

    EXPECT_CALL(*mockSwitch, eraseRidAndVid(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke([](sai_object_id_t rid, sai_object_id_t vid) {
        }));

    swss::KeyOpFieldsValuesTuple kco;
    std::string key = "SAI_OBJECT_TYPE_PORT:bulk:1";
    std::string op = "bulkcreate";
    std::vector<swss::FieldValueTuple> values = {
        {"oid:0x10000000000002", "SAI_PORT_ATTR_ADMIN_STATE=true"}
    };
    kco = std::make_tuple(key, op, values);

    auto channel = std::make_shared<MockSelectableChannel>();
    int popCallCount = 0;

    EXPECT_CALL(*channel, empty())
        .WillRepeatedly([&popCallCount]() {
            return popCallCount > 0; // Return true after the first pop call
        });
    EXPECT_CALL(*channel, pop(testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::DoAll(
            testing::SetArgReferee<0>(kco),
            testing::Invoke([&popCallCount](swss::KeyOpFieldsValuesTuple&, bool) {
                popCallCount++;
            })
        ));

    m_sai->mock_bulkCreate = [](
        sai_object_type_t,
        sai_object_id_t,
        uint32_t,
        const uint32_t*,
        const sai_attribute_t**,
        sai_bulk_op_error_mode_t,
        sai_object_id_t*,
        sai_status_t*) -> sai_status_t {
            return SAI_STATUS_NOT_IMPLEMENTED;
    };

    m_syncd->processEvent(*channel);
}

TEST_F(SyncdTest, BulkSetTest)
{
    m_opt->m_enableSaiBulkSupport = true;

    auto translator = m_syncd->m_translator;
    translator->insertRidAndVid(0x11000000000001, 0x21000000000000); // Switch
    translator->insertRidAndVid(0x1100000000000d, 0x1000000000000d); // Port 1
    translator->insertRidAndVid(0x1100000000000e, 0x1000000000000e); // Port 2

    swss::KeyOpFieldsValuesTuple kco;
    std::string key = "SAI_OBJECT_TYPE_PORT:bulk:1";
    std::string op = "bulkset";
    std::vector<swss::FieldValueTuple> values = {
        {"oid:0x1000000000000d", "SAI_PORT_ATTR_ADMIN_STATE=true"},
        {"oid:0x1000000000000e", "SAI_PORT_ATTR_ADMIN_STATE=false"}
    };
    kco = std::make_tuple(key, op, values);

    auto channel = std::make_shared<MockSelectableChannel>();
    EXPECT_CALL(*channel, empty())
        .WillOnce(testing::Return(false))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*channel, pop(testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::DoAll(
            testing::SetArgReferee<0>(kco),
            testing::Return()
    ));

    m_sai->mock_bulkSet = [](
        sai_object_type_t,
        uint32_t,
        const sai_object_id_t*,
        const sai_attribute_t*,
        sai_bulk_op_error_mode_t,
        sai_status_t*) -> sai_status_t {
            return SAI_STATUS_NOT_IMPLEMENTED;
    };

    m_syncd->processEvent(*channel);
}

TEST_F(SyncdTest, BulkRemoveTest)
{
    m_opt->m_enableSaiBulkSupport = true;

    sai_object_id_t switchVid = 0x21000000000000;
    sai_object_id_t switchRid = 0x11000000000001;

    sai_object_id_t portVid = 0x10000000000002;
    sai_object_id_t portRid = 0x11000000000002;

    auto translator = m_syncd->m_translator;
    translator->insertRidAndVid(switchRid, switchVid);
    translator->insertRidAndVid(portRid, portVid);

    std::vector<uint32_t> lanes = {52, 53};
    m_syncd->m_client->setPortLanes(switchVid, portRid, lanes);

    std::set<sai_object_id_t> removedRids;
    m_sai->mock_objectTypeQuery = [switchRid, portRid, &removedRids](sai_object_id_t oid) {
        sai_object_type_t ot = SAI_OBJECT_TYPE_NULL;
        if (removedRids.find(oid) != removedRids.end()) {
            return SAI_OBJECT_TYPE_NULL;
        }
        if (oid == switchRid)
            ot = SAI_OBJECT_TYPE_SWITCH;
        else if (oid == portRid)
            ot = SAI_OBJECT_TYPE_PORT;
        else
            ot = SAI_OBJECT_TYPE_QUEUE;
        return ot;
    };

    m_sai->mock_switchIdQuery = [switchVid](sai_object_id_t oid) {
        return switchVid;
    };

    m_sai->mock_get = [switchRid, portVid, portRid](sai_object_type_t objectType, sai_object_id_t objectId, uint32_t attrCount, sai_attribute_t* attrList) -> sai_status_t {
        if (attrCount != 1) {
            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (attrList[0].id == SAI_SWITCH_ATTR_PORT_LIST) {
            if (objectType == SAI_OBJECT_TYPE_SWITCH && objectId == switchRid) {
                attrList[0].value.objlist.count = 1;
                attrList[0].value.objlist.list[0] = portRid;
                return SAI_STATUS_SUCCESS;
            }
            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (attrList[0].id == SAI_SWITCH_ATTR_PORT_NUMBER) {
            attrList[0].value.u32 = 1;
            return SAI_STATUS_SUCCESS;
        }

        if (objectType == SAI_OBJECT_TYPE_PORT) {
            if (attrList[0].id == SAI_PORT_ATTR_HW_LANE_LIST) {
                if (objectId == portRid) {
                    if (attrList[0].value.u32list.list != nullptr) {
                        attrList[0].value.u32list.count = 2;
                        static uint32_t hw_lanes[2] = {52, 53};
                        memcpy(attrList[0].value.u32list.list, hw_lanes, sizeof(uint32_t) * 2);
                    } else {
                        return SAI_STATUS_BUFFER_OVERFLOW;
                    }
                }
            }

            if (attrList[0].id == SAI_PORT_ATTR_PORT_SERDES_ID) {
                attrList[0].value.oid = SAI_NULL_OBJECT_ID;
                return SAI_STATUS_SUCCESS;
            }

            return SAI_STATUS_SUCCESS;
        }

        if (attrList[0].id == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS) {
            if (objectType == SAI_OBJECT_TYPE_SWITCH && objectId == switchRid) {
                static sai_mac_t mac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
                memcpy(attrList[0].value.mac, mac, sizeof(sai_mac_t));
                return SAI_STATUS_SUCCESS;
            }
            return SAI_STATUS_INVALID_PARAMETER;
        }

        return SAI_STATUS_NOT_SUPPORTED;
    };

    // Create a mock switch and set expectations
    auto mockSwitch = std::make_shared<MockSaiSwitch>(switchVid, switchRid, m_syncd->m_client, translator, m_sai, false);
    m_syncd->m_switches[switchVid] = mockSwitch;
    EXPECT_CALL(*mockSwitch, postPortRemove(testing::_))
        .WillRepeatedly(testing::Invoke([](sai_object_id_t rid) {
        }));

    EXPECT_CALL(*mockSwitch, removeExistingObjectReference(testing::_))
        .WillRepeatedly(testing::Invoke([](sai_object_id_t rid) {
        }));

    EXPECT_CALL(*mockSwitch, eraseRidAndVid(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke([](sai_object_id_t rid, sai_object_id_t vid) {
        }));

    swss::KeyOpFieldsValuesTuple kco;
    std::string key = "SAI_OBJECT_TYPE_PORT:bulk:1";
    std::string op = "bulkremove";
    std::vector<swss::FieldValueTuple> values = {
        {"oid:0x10000000000002", "SAI_PORT_ATTR_ADMIN_STATE=true"}
    };
    kco = std::make_tuple(key, op, values);

    auto channel = std::make_shared<MockSelectableChannel>();
    int popCallCount = 0;

    EXPECT_CALL(*channel, empty())
        .WillRepeatedly([&popCallCount]() {
            return popCallCount > 0; // Return true after the first pop call
        });
    EXPECT_CALL(*channel, pop(testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::DoAll(
            testing::SetArgReferee<0>(kco),
            testing::Invoke([&popCallCount](swss::KeyOpFieldsValuesTuple&, bool) {
                popCallCount++;
            })
        ));

    m_sai->mock_bulkRemove = [](
        sai_object_type_t,
        uint32_t,
        const sai_object_id_t*,
        sai_bulk_op_error_mode_t,
        sai_status_t*) -> sai_status_t {
            return SAI_STATUS_NOT_IMPLEMENTED;
    };

    m_syncd->processEvent(*channel);
}

TEST_F(SyncdTest, processEventInShutdownWaitMode_NotifyCommand)
{
    // Test that NOTIFY commands receive FAILURE response in shutdown-wait mode
    auto channel = std::make_shared<MockSelectableChannel>();

    // Set up the mock channel as the syncd's selectable channel
    // so sendNotifyResponse will use it
    m_syncd->m_selectableChannel = channel;

    int popCallCount = 0;
    EXPECT_CALL(*channel, empty())
        .WillRepeatedly([&popCallCount]() {
            return popCallCount > 0;
        });

    EXPECT_CALL(*channel, pop(testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::Invoke([](swss::KeyOpFieldsValuesTuple& kco, bool initViewMode) {
                kfvKey(kco) = SYNCD_INIT_VIEW;
                kfvOp(kco) = REDIS_ASIC_STATE_COMMAND_NOTIFY;
            }),
            testing::Invoke([&popCallCount](swss::KeyOpFieldsValuesTuple&, bool) {
                popCallCount++;
            })
        ));

    // Verify that set() is called with FAILURE status response
    std::string expectedStatus = sai_serialize_status(SAI_STATUS_FAILURE);
    EXPECT_CALL(*channel, set(expectedStatus, testing::_, REDIS_ASIC_STATE_COMMAND_NOTIFY))
        .Times(1);

    m_syncd->processEventInShutdownWaitMode(*channel);
}

TEST_F(SyncdTest, processEventInShutdownWaitMode_NonNotifyCommand)
{
    // Test that non-NOTIFY commands are ignored (no response sent) in shutdown-wait mode
    auto channel = std::make_shared<MockSelectableChannel>();

    // Set up the mock channel as the syncd's selectable channel
    m_syncd->m_selectableChannel = channel;

    int popCallCount = 0;
    EXPECT_CALL(*channel, empty())
        .WillRepeatedly([&popCallCount]() {
            return popCallCount > 0;
        });

    EXPECT_CALL(*channel, pop(testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::Invoke([](swss::KeyOpFieldsValuesTuple& kco, bool initViewMode) {
                kfvKey(kco) = "SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000";
                kfvOp(kco) = REDIS_ASIC_STATE_COMMAND_CREATE;
            }),
            testing::Invoke([&popCallCount](swss::KeyOpFieldsValuesTuple&, bool) {
                popCallCount++;
            })
        ));

    // Verify that set() is NOT called for non-notify commands
    EXPECT_CALL(*channel, set(testing::_, testing::_, testing::_))
        .Times(0);

    m_syncd->processEventInShutdownWaitMode(*channel);
}

// ----------------------------------------------------------------------------
// Link event damping tests
//
// These tests exercise the real damping call chain:
//
//   NotificationProcessor::syncProcessNotification
//     -> handle_port_state_change
//     -> process_on_port_state_change
//     -> m_linkEventDampingApplier (bound to Syncd::applyLinkEventDamping)
//     -> applyAiedAlgorithm / decayPenalty / writeDampingCountersToStateDb
//
// Notes:
// - Notifications must carry the port RID (the processor translates RID->VID).
// - Damping config is applied through Syncd::processEvent with op
//   REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET, which covers
//   processLinkEventDampingConfigSet as well.
// - Results are asserted via the LINK_EVENT_DAMPING_STATS table in STATE_DB,
//   which is written by writeDampingCountersToStateDb.
// ----------------------------------------------------------------------------

class SyncdLinkEventDampingTest : public SyncdTest
{
protected:
    static constexpr sai_object_id_t PORT_VID = 0x10000000000002;
    static constexpr sai_object_id_t PORT_RID = 0x11000000000002;

    void SetUp() override
    {
        SWSS_LOG_ENTER();

        SyncdTest::SetUp();

        m_syncd->m_translator->insertRidAndVid(PORT_RID, PORT_VID);
    }

    // Apply damping config via the regular command path
    // (covers Syncd::processLinkEventDampingConfigSet).
    void setDampingConfig(
            const sai_redis_link_event_damping_algo_aied_config_t& config)
    {
        SWSS_LOG_ENTER();

        std::string key = sai_serialize_object_type(SAI_OBJECT_TYPE_PORT)
                        + ":" + sai_serialize_object_id(PORT_VID);

        std::vector<swss::FieldValueTuple> values = {
            { sai_serialize_redis_port_attr_id(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM),
              sai_serialize_redis_link_event_damping_algorithm(SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED) },
            { sai_serialize_redis_port_attr_id(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG),
              sai_serialize_redis_link_event_damping_aied_config(config) },
        };

        swss::KeyOpFieldsValuesTuple kco(key, REDIS_ASIC_STATE_COMMAND_DAMPING_CONFIG_SET, values);

        MockSelectableChannel consumer;
        EXPECT_CALL(consumer, empty()).WillOnce(testing::Return(true));
        EXPECT_CALL(consumer, pop(testing::_, testing::_))
            .WillOnce(testing::SetArgReferee<0>(kco));

        m_syncd->processEvent(consumer);
    }

    // Inject a port state change notification the same way the vendor SAI
    // callback path would deliver it (using the port RID).
    void sendPortStateChange(
            sai_port_oper_status_t status)
    {
        SWSS_LOG_ENTER();

        sai_port_oper_status_notification_t n;

        memset(&n, 0, sizeof(n));

        n.port_id = PORT_RID; // notification carries RID, processor translates to VID
        n.port_state = status;

        std::string data = sai_serialize_port_oper_status_ntf(1, &n);

        std::vector<swss::FieldValueTuple> entry;
        swss::KeyOpFieldsValuesTuple item(SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE, data, entry);

        m_syncd->m_processor->syncProcessNotification(item);
    }

    // Read a damping counter field from STATE_DB written by
    // writeDampingCountersToStateDb.
    std::string getDampingField(
            const std::string& field)
    {
        SWSS_LOG_ENTER();

        swss::DBConnector db("STATE_DB", 0);
        swss::Table table(&db, "LINK_EVENT_DAMPING_STATS");

        std::string value;
        bool found = table.hget(sai_serialize_object_id(PORT_VID), field, value);

        return found ? value : "";
    }

    std::mutex& dampingStateMutex()
    {
        SWSS_LOG_ENTER();

        return m_syncd->m_linkEventDampingMutex;
    }

    std::map<sai_object_id_t, LinkEventDampingPortState>& portDampingStates()
    {
        SWSS_LOG_ENTER();

        return m_syncd->m_portLinkEventDampingStates;
    }

    std::mutex& pendingNotificationsMutex()
    {
        SWSS_LOG_ENTER();

        return m_syncd->m_pendingNotificationsMutex;
    }

    std::queue<std::vector<sai_port_oper_status_notification_t>>& pendingNotifications()
    {
        SWSS_LOG_ENTER();

        return m_syncd->m_pendingNotifications;
    }

    void invokeProcessPendingDampingSync()
    {
        SWSS_LOG_ENTER();

        m_syncd->processPendingDampingSync();
    }

    void invokeFlushPendingDampingNotifications()
    {
        SWSS_LOG_ENTER();

        m_syncd->flushPendingDampingNotifications();
    }
};

// Define static constant expression members for linkage
constexpr sai_object_id_t SyncdLinkEventDampingTest::PORT_VID;
constexpr sai_object_id_t SyncdLinkEventDampingTest::PORT_RID;

TEST_F(SyncdLinkEventDampingTest, flapsEnterDampingAndSuppress)
{
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time  = 10000;
    config.suppress_threshold = 1500;
    config.reuse_threshold    = 1000;
    config.decay_half_life    = 5000;
    config.flap_penalty       = 1000;

    setDampingConfig(config);

    // UNKNOWN -> DOWN: no penalty (penalty only on UP -> DOWN), propagated
    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    // DOWN -> UP: propagated
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);
    // UP -> DOWN: penalty = 1000 < 1500, propagated
    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);
    // UP -> DOWN: penalty ~2000 >= 1500 -> damping activated,
    // threshold-crossing event itself is propagated
    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    // damping active -> this UP must be suppressed
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);

    EXPECT_EQ(getDampingField("is_damping_active"), "true");

    // physical status followed the flaps, advertised stayed at last
    // propagated state (DOWN), proving the UP was suppressed
    EXPECT_EQ(getDampingField("physical_status"), "SAI_PORT_OPER_STATUS_UP");
    EXPECT_EQ(getDampingField("advertised_status"), "SAI_PORT_OPER_STATUS_DOWN");

    // pre-damping counters see all transitions, post-damping counters
    // miss the suppressed UP
    EXPECT_EQ(getDampingField("pre_damping_down_events"), "3");
    EXPECT_EQ(getDampingField("pre_damping_up_events"), "3");
    EXPECT_EQ(getDampingField("post_damping_down_events"), "3");
    EXPECT_EQ(getDampingField("post_damping_up_events"), "2");
}

TEST_F(SyncdLinkEventDampingTest, maxSuppressTimeoutExitsDamping)
{
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time  = 200;  // short timeout for the test
    config.suppress_threshold = 100;
    config.reuse_threshold    = 50;
    config.decay_half_life    = 100;
    config.flap_penalty       = 1000;

    setDampingConfig(config);

    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);
    // UP -> DOWN: penalty 1000 >= 100 -> damping activated
    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    // suppressed, physical becomes UP while advertised stays DOWN
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);

    ASSERT_EQ(getDampingField("is_damping_active"), "true");

    // wait past max_suppress_time
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // same-state event (no DOWN, so damping timer is not reset):
    // timeout branch in applyAiedAlgorithm exits damping and the event
    // is propagated, synchronizing advertised with physical state
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);

    EXPECT_EQ(getDampingField("is_damping_active"), "false");
    EXPECT_EQ(getDampingField("physical_status"), "SAI_PORT_OPER_STATUS_UP");
    EXPECT_EQ(getDampingField("advertised_status"), "SAI_PORT_OPER_STATUS_UP");
}

TEST_F(SyncdLinkEventDampingTest, penaltyDecayExitsDamping)
{
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time  = 10000; // large, so decay exit wins
    config.suppress_threshold = 1000;
    config.reuse_threshold    = 600;
    config.decay_half_life    = 1000;   // fast decay
    config.flap_penalty       = 1000;

    setDampingConfig(config);

    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);
    // UP -> DOWN: penalty 1000 >= 1000 -> damping activated
    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    // suppressed
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);

    EXPECT_EQ(getDampingField("is_damping_active"), "true");

    // wait ~3 half lives: penalty 1000 -> ~125 < reuse_threshold (600)
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // same-state event triggers decayPenalty + reuse-threshold exit branch
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);

    EXPECT_EQ(getDampingField("is_damping_active"), "false");
    EXPECT_EQ(getDampingField("advertised_status"), "SAI_PORT_OPER_STATUS_UP");
}

TEST_F(SyncdLinkEventDampingTest, noDampingConfiguredPropagates)
{
    // No damping config applied: applyLinkEventDamping returns early
    // (port not in m_portLinkEventDampingStates) and events propagate
    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);

    // no STATE_DB entry is written for non-configured ports
    EXPECT_EQ(getDampingField("is_damping_active"), "");
}

TEST_F(SyncdLinkEventDampingTest, processPendingDampingSyncWithNotifications)
{
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time  = 2000;
    config.suppress_threshold = 100;
    config.reuse_threshold    = 50;
    config.decay_half_life    = 500;
    config.flap_penalty       = 1000;

    setDampingConfig(config);

    // Create the scenario: port goes DOWN (advertised), then UP (suppressed)
    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);
    sendPortStateChange(SAI_PORT_OPER_STATUS_DOWN);
    sendPortStateChange(SAI_PORT_OPER_STATUS_UP);

    // Verify damping is active (UP was suppressed)
    EXPECT_EQ(getDampingField("is_damping_active"), "true");

    // Set up preconditions
    {
        std::lock_guard<std::mutex> lock(dampingStateMutex());

        auto it = portDampingStates().find(PORT_VID);
        ASSERT_NE(it, portDampingStates().end())
            << "Port damping state not found";

        auto& state = it->second;

        // Simulate what the timer thread does when damping exits via timeout
        state.pending_state_sync = true;
        state.is_damping_active = false;

        // Verify the mismatch exists (this is what triggers the notification)
        EXPECT_NE(state.advertised_status, state.physical_status)
            << "Expected status mismatch for notification generation";
    }

    // Call the function
    invokeProcessPendingDampingSync();

    // Verify the notification was enqueued
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());
        EXPECT_EQ(pendingNotifications().size(), 1)
            << "Expected 1 notification batch to be enqueued";

        if (!pendingNotifications().empty())
        {
            auto& batch = pendingNotifications().front();
            EXPECT_EQ(batch.size(), 1) << "Expected 1 notification in batch";
            EXPECT_EQ(batch[0].port_id, PORT_VID) << "Notification for wrong port";
        }
    }

    // flush the notifications
    invokeFlushPendingDampingNotifications();

    // Verify pending_state_sync was cleared and advertised was updated
    {
        std::lock_guard<std::mutex> lock(dampingStateMutex());
        auto& state = portDampingStates()[PORT_VID];
        EXPECT_FALSE(state.pending_state_sync) << "pending_state_sync should be cleared";
        EXPECT_EQ(state.advertised_status, state.physical_status)
            << "Advertised should match physical after sync";
    }
}

TEST_F(SyncdLinkEventDampingTest, processPendingDampingSyncQueueOverflow)
{
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time  = 2000;
    config.suppress_threshold = 100;
    config.reuse_threshold    = 50;
    config.decay_half_life    = 500;
    config.flap_penalty       = 1000;

    setDampingConfig(config);

    // Directly populate the queue to 1000 entries
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());

        sai_port_oper_status_notification_t dummy_ntf;
        dummy_ntf.port_id = PORT_VID;
        dummy_ntf.port_state = SAI_PORT_OPER_STATUS_UP;

        std::vector<sai_port_oper_status_notification_t> batch = {dummy_ntf};

        // Fill queue to exactly 1000 entries
        for (int i = 0; i < 1000; ++i)
        {
            pendingNotifications().push(batch);
        }

        EXPECT_EQ(pendingNotifications().size(), 1000);
    }

    // Set up the scenario to trigger one more notification
    {
        std::lock_guard<std::mutex> lock(dampingStateMutex());

        auto& state = portDampingStates().at(PORT_VID);

        // Set up the condition for processPendingDampingSync to queue a notification
        state.pending_state_sync = true;
        state.advertised_status = SAI_PORT_OPER_STATUS_DOWN;
        state.physical_status = SAI_PORT_OPER_STATUS_UP;
    }

    // This should trigger overflow protection
    invokeProcessPendingDampingSync();

    // Verify the overflow protection worked
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());

        // Queue should still be 1000 (dropped oldest, added newest)
        EXPECT_EQ(pendingNotifications().size(), 1000)
            << "Queue should be capped at 1000 after overflow";
    }
}

TEST_F(SyncdLinkEventDampingTest, flushPendingNotificationsWithBatches)
{
    // Directly populate the queue with multiple batches
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());

        // Create 3 different notification batches
        for (int batch_num = 0; batch_num < 3; ++batch_num)
        {
            sai_port_oper_status_notification_t ntf;
            ntf.port_id = PORT_VID;
            ntf.port_state = (batch_num % 2 == 0)
                ? SAI_PORT_OPER_STATUS_DOWN
                : SAI_PORT_OPER_STATUS_UP;

            std::vector<sai_port_oper_status_notification_t> batch = {ntf};
            pendingNotifications().push(batch);
        }

        EXPECT_EQ(pendingNotifications().size(), 3);
    }

    invokeFlushPendingDampingNotifications();

    // Verify all batches were flushed
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());
        EXPECT_EQ(pendingNotifications().size(), 0)
            << "All batches should be flushed";
    }
}

TEST_F(SyncdLinkEventDampingTest, flushPendingNotificationsDrainsQueue)
{
    // Queue 2 notification batches
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());

        for (int i = 0; i < 2; ++i)
        {
            sai_port_oper_status_notification_t ntf;
            ntf.port_id = PORT_VID;
            ntf.port_state = SAI_PORT_OPER_STATUS_UP;

            std::vector<sai_port_oper_status_notification_t> batch = {ntf};
            pendingNotifications().push(batch);
        }
    }

    // Tests normal path
    invokeFlushPendingDampingNotifications();

    // Verify all batches were processed
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());
        EXPECT_EQ(pendingNotifications().size(), 0);
    }
}

TEST_F(SyncdLinkEventDampingTest, fullNotificationFlowIntegrated)
{
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time  = 2000;
    config.suppress_threshold = 100;
    config.reuse_threshold    = 50;
    config.decay_half_life    = 500;
    config.flap_penalty       = 1000;

    setDampingConfig(config);

    // Set up with pending notification
    {
        std::lock_guard<std::mutex> lock(dampingStateMutex());

        auto& state = portDampingStates().at(PORT_VID);
        state.pending_state_sync = true;
        state.advertised_status = SAI_PORT_OPER_STATUS_DOWN;
        state.physical_status = SAI_PORT_OPER_STATUS_UP;
    }

    // Step 1: processPendingDampingSync queues the notification
    invokeProcessPendingDampingSync();

    // Verify notification was queued
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());
        EXPECT_EQ(pendingNotifications().size(), 1);
    }

    // Step 2: flushPendingDampingNotifications sends it
    invokeFlushPendingDampingNotifications();

    // Verify queue was flushed
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());
        EXPECT_EQ(pendingNotifications().size(), 0);
    }
}

TEST_F(SyncdLinkEventDampingTest, flushPendingNotificationsEmptyQueue)
{
    // Ensure queue is empty
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());
        EXPECT_EQ(pendingNotifications().size(), 0);
    }

    // Call flush with empty queue
    invokeFlushPendingDampingNotifications();

    // Queue should still be empty
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());
        EXPECT_EQ(pendingNotifications().size(), 0);
    }
}

TEST_F(SyncdLinkEventDampingTest, processPendingDampingSyncNoPending)
{
    sai_redis_link_event_damping_algo_aied_config_t config;
    config.max_suppress_time  = 2000;
    config.suppress_threshold = 100;
    config.reuse_threshold    = 50;
    config.decay_half_life    = 500;
    config.flap_penalty       = 1000;

    setDampingConfig(config);

    // Set up state WITHOUT pending_state_sync
    {
        std::lock_guard<std::mutex> lock(dampingStateMutex());

        auto& state = portDampingStates().at(PORT_VID);
        state.pending_state_sync = false;  // No pending sync
        state.advertised_status = SAI_PORT_OPER_STATUS_DOWN;
        state.physical_status = SAI_PORT_OPER_STATUS_UP;
    }

    // Call the function - should NOT queue any notifications
    invokeProcessPendingDampingSync();

    // Verify NO notification was queued
    {
        std::lock_guard<std::mutex> lock(pendingNotificationsMutex());
        EXPECT_EQ(pendingNotifications().size(), 0)
            << "No notification should be queued when pending_state_sync=false";
    }
}
#endif
