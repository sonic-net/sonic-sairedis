#include "Syncd.h"
#include "RequestShutdown.h"
#include "vslib/ContextConfigContainer.h"
#include "vslib/VirtualSwitchSaiInterface.h"
#include "vslib/Sai.h"
#include "lib/Sai.h"

#include "swss/dbconnector.h"

#include "sairediscommon.h"

#include <gtest/gtest.h>

using namespace syncd;
using namespace saivs;

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
