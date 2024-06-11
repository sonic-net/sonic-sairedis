#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>

#include "meta/DummySaiInterface.h"

#include "Sai.h"
#include "Proxy.h"

using namespace saiproxy;

TEST(Proxy, ctr)
{
    Sai sai;

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    // will test loadProfileMap

    auto proxy = std::make_shared<Proxy>(dummy);
}

static void fun(std::shared_ptr<Proxy> proxy)
{
    SWSS_LOG_ENTER();

    proxy->run();
}

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
        return NULL;

    return nullptr;
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    return 0;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

static int ntfCounter = 0;

static void onSwitchStateChange(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received onSwitchStateChange");

    ntfCounter++;
}

TEST(Proxy, notifications)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<saimeta::DummySaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);
//    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY), SAI_STATUS_SUCCESS);
//    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);
//    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY), SAI_STATUS_SUCCESS);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_object_id_t switch_id;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    // create oid
    auto status = sai.create(
            SAI_OBJECT_TYPE_SWITCH,
            &switch_id,
            SAI_NULL_OBJECT_ID, // creating switch
            1,
            &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);
    EXPECT_NE(switch_id, SAI_NULL_OBJECT_ID);

    attr.id = SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY;
    attr.value.ptr = (void*)&onSwitchStateChange;

    // set notification pointer
    EXPECT_EQ(sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr), SAI_STATUS_SUCCESS);

    // dummy start sending notifications
    EXPECT_EQ(dummy->start(), SAI_STATUS_SUCCESS);

    sleep(1); // give some time for proxy to receive notification

    // dummy stop sending notifications
    EXPECT_EQ(dummy->stop(), SAI_STATUS_SUCCESS);

    EXPECT_EQ(proxy->getNotificationsSentCount(), 1);

    proxy->stop();

    thread->join();
}
