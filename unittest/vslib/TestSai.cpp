#include "Sai.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <memory>

using namespace saivs;

namespace
{
class PlatformEventCountingSwitchState final : public SwitchStateBase
{
public:
    using SwitchStateBase::SwitchStateBase;

    void syncOnPlatformEvent(
            _In_ std::shared_ptr<EventPayloadPlatformEvent> payload) override
    {
        SWSS_LOG_ENTER();

        m_events++;
    }

    std::atomic<int> m_events{0};
};
}

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
        return NULL;

    if (strcmp(variable, SAI_KEY_VS_SWITCH_TYPE) == 0)
        return SAI_VALUE_VS_SWITCH_TYPE_BCM56850;

    return nullptr;
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
        return 0;

    return -1;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};


TEST(Sai, bulkGet)
{
    Sai sai;

    sai.apiInitialize(0, &test_services);

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sai.create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 1, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    EXPECT_EQ(sai.get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr), SAI_STATUS_SUCCESS);

    auto portNum = attr.value.u32;

    std::vector<sai_object_id_t> oids(portNum);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portNum;
    attr.value.objlist.list = oids.data();
    EXPECT_EQ(sai.get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr), SAI_STATUS_SUCCESS);

    std::vector<sai_attribute_t> attrs(portNum);
    std::vector<uint32_t> attrCounts(portNum, 1);
    std::vector<sai_status_t> statuses(portNum);
    std::vector<sai_attribute_t*> pattrs(portNum);
    for (size_t i = 0; i < portNum; i++)
    {
        attrs[i].id = SAI_PORT_ATTR_ADMIN_STATE;
        pattrs[i] = &attrs[i];
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                portNum,
                oids.data(),
                attrCounts.data(),
                pattrs.data(),
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses.data()));
}
TEST(Sai, fdbAgingWakeEvent)
{
    // Smoke test: verify the FDB aging thread starts and shuts down cleanly
    // without deadlocking. The aging thread's normal wake path (MAC event
    // delivery via m_fdbAgingWakeEvent) is NOT exercised here — this only
    // confirms the start/stop life cycle completes without hanging.
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    sai_attribute_t attr;
    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sai.create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 1, &attr), SAI_STATUS_SUCCESS);
    EXPECT_NE(switch_id, SAI_NULL_OBJECT_ID);

    // Allow the aging thread to run at least one cycle.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // remove(SWITCH) triggers stopFdbAgingThread() which joins the thread.
    // If the wake/shutdown paths are broken this would hang; the test passing
    // confirms the aging thread terminates correctly.
    EXPECT_EQ(sai.remove(SAI_OBJECT_TYPE_SWITCH, switch_id), SAI_STATUS_SUCCESS);
}

TEST(Sai, platformEventAppliedUnderApiMutex)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    sai_attribute_t attr;
    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    ASSERT_EQ(sai.create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 1, &attr), SAI_STATUS_SUCCESS);

    auto& switchStateMap = sai.m_vsSai->m_switchStateMap;
    auto original = switchStateMap.at(switch_id);
    auto state = std::make_shared<PlatformEventCountingSwitchState>(
            switch_id,
            sai.m_vsSai->m_realObjectIdManager,
            original->m_switchConfig);

    {
        // While an API call holds the mutex, the event queue thread waits for it
        std::lock_guard<std::recursive_mutex> lock(sai.m_apimutex);

        switchStateMap.at(switch_id) = state;

        sai.m_eventQueue->enqueue(std::make_shared<Event>(EVENT_TYPE_PLATFORM_EVENT,
                    std::make_shared<EventPayloadPlatformEvent>(switch_id)));

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        EXPECT_EQ(state->m_events.load(), 0);
    }

    // and applies the event once the call returns
    for (int i = 0; i < 200 && state->m_events.load() == 0; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(state->m_events.load(), 1);

    {
        std::lock_guard<std::recursive_mutex> lock(sai.m_apimutex);

        switchStateMap.at(switch_id) = original;
    }

    EXPECT_EQ(sai.remove(SAI_OBJECT_TYPE_SWITCH, switch_id), SAI_STATUS_SUCCESS);
}
