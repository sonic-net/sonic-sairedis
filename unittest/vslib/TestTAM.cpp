#include <SwitchStateBase.h>
#include <EventPayloadNotification.h>
#include <meta/sai_serialize.h>

#include <gtest/gtest.h>

#include <string>

using namespace saivs;
using namespace std;

static sai_object_id_t notified_tam_tel_type_id = SAI_NULL_OBJECT_ID;

static void notify_tam_tel_type_config_change(
        _In_ sai_object_id_t tam_tel_type_id)
{
    SWSS_LOG_ENTER();

    notified_tam_tel_type_id = tam_tel_type_id;
}

TEST(TAM, TAMTelTypeConfigNotification)
{
    vector<sai_attribute_t> attrs;
    sai_attribute_t attr;
    sai_object_id_t switch_id = 0x2100000000;
    auto signal = std::make_shared<Signal>();
    auto eventQueue = std::make_shared<EventQueue>(signal);
    auto sc = make_shared<SwitchConfig>(0, "");
    sc->m_eventQueue = eventQueue;
    auto scc = make_shared<SwitchConfigContainer>();
    SwitchStateBase ss(
        switch_id,
        make_shared<RealObjectIdManager>(0, scc),
        sc);

    sai_object_id_t tam_tel_type_id = 0x4b000000000001;
    string tam_tel_type_id_str = sai_serialize_object_id(tam_tel_type_id);

    attrs.clear();
    attr.id = SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(notify_tam_tel_type_config_change);
    attrs.push_back(attr);

    EXPECT_EQ(
        SAI_STATUS_SUCCESS,
        ss.create(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(switch_id), 0, static_cast<uint32_t>(attrs.size()), attrs.data()));

    EXPECT_EQ(
        SAI_STATUS_SUCCESS,
        ss.create(SAI_OBJECT_TYPE_TAM_TEL_TYPE, tam_tel_type_id_str, switch_id, 0, nullptr));

    attr.id = SAI_TAM_TEL_TYPE_ATTR_STATE;
    attr.value.s32 = SAI_TAM_TEL_TYPE_STATE_CREATE_CONFIG;

    EXPECT_EQ(
        SAI_STATUS_SUCCESS,
        ss.set(SAI_OBJECT_TYPE_TAM_TEL_TYPE, tam_tel_type_id_str, &attr));

    auto event = eventQueue->dequeue();
    EXPECT_EQ(event->getType(), EventType::EVENT_TYPE_NOTIFICATION);
    auto payload = dynamic_pointer_cast<EventPayloadNotification>(event->getPayload());

    auto switchNotifications = payload->getSwitchNotifications();
    auto ntf = payload->getNotification();
    ntf->executeCallback(switchNotifications);
    EXPECT_EQ(notified_tam_tel_type_id, tam_tel_type_id);
}
