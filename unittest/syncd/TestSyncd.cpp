#include "Syncd.h"
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
                  std::shared_ptr<RedisClient> client,
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
#endif
