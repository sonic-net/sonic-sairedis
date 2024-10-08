#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>
#include "Workaround.h"
#include "swss/logger.h"

#include <arpa/inet.h>

using namespace syncd;

TEST(Workaround, isSetAttributeWorkaround)
{
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_HOSTIF, SAI_HOSTIF_ATTR_QUEUE, SAI_STATUS_FAILURE), true);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_SRC_MAC_ADDRESS, SAI_STATUS_FAILURE), true);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_TYPE, SAI_STATUS_FAILURE), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_TYPE, SAI_STATUS_SUCCESS), false);
}

TEST(Workaround,convertPortOperStatusNotification)
{
    sai_port_oper_status_notification_t data[2];

    ASSERT_EQ(Workaround::convertPortOperStatusNotification(0, nullptr, SAI_API_VERSION).size(), 0);
    ASSERT_EQ(Workaround::convertPortOperStatusNotification(5000, data, SAI_API_VERSION).size(), 0);

    ASSERT_EQ(Workaround::convertPortOperStatusNotification(2, data, SAI_VERSION(1,15,0)).size(), 2);
    ASSERT_EQ(Workaround::convertPortOperStatusNotification(2, data, SAI_VERSION(1,14,1)).size(), 2);

    // check new structure notifications

    data[0].port_id = 12;
    data[0].port_state = SAI_PORT_OPER_STATUS_DOWN;
    data[0].port_error_status = SAI_PORT_ERROR_STATUS_HIGH_BER;
    data[1].port_id = 22;
    data[1].port_state = SAI_PORT_OPER_STATUS_UP;
    data[1].port_error_status = SAI_PORT_ERROR_STATUS_DATA_UNIT_MISALIGNMENT_ERROR;

    auto ntf = Workaround::convertPortOperStatusNotification(2, data, SAI_VERSION(1,14,1));

    ASSERT_EQ(ntf[0].port_id, 12);
    ASSERT_EQ(ntf[0].port_state, SAI_PORT_OPER_STATUS_DOWN);
    ASSERT_EQ(ntf[0].port_error_status, SAI_PORT_ERROR_STATUS_HIGH_BER);
    ASSERT_EQ(ntf[1].port_id, 22);
    ASSERT_EQ(ntf[1].port_state, SAI_PORT_OPER_STATUS_UP);
    ASSERT_EQ(ntf[1].port_error_status, SAI_PORT_ERROR_STATUS_DATA_UNIT_MISALIGNMENT_ERROR);

    // check old structure notification
    Workaround::sai_port_oper_status_notification_v1_14_0_t old[2];

    old[0].port_id = 42;
    old[0].port_state = SAI_PORT_OPER_STATUS_UP;
    old[1].port_id = 43;
    old[1].port_state = SAI_PORT_OPER_STATUS_DOWN;

    auto ntf2 = Workaround::convertPortOperStatusNotification(2, reinterpret_cast<sai_port_oper_status_notification_t*>(old), SAI_VERSION(1,14,0));

    ASSERT_EQ(ntf.size(), 2);

    ASSERT_EQ(ntf2[0].port_id, 42);
    ASSERT_EQ(ntf2[0].port_state, SAI_PORT_OPER_STATUS_UP);
    ASSERT_EQ(ntf2[0].port_error_status, 0);
    ASSERT_EQ(ntf2[1].port_id, 43);
    ASSERT_EQ(ntf2[1].port_state, SAI_PORT_OPER_STATUS_DOWN);
    ASSERT_EQ(ntf2[1].port_error_status, 0);
}
