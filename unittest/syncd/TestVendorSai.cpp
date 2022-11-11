#include <gtest/gtest.h>
#include "VendorSai.h"
#include "swss/logger.h"

#include <arpa/inet.h>

#ifdef HAVE_SAI_BULK_OBJECT_GET_STATS
#undef HAVE_SAI_BULK_OBJECT_GET_STATS
#endif

using namespace syncd;

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

TEST(VendorSai, bulkGetStats)
{
    VendorSai sai;
    sai.initialize(0, &test_services);
    ASSERT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai.bulkGetStats(SAI_NULL_OBJECT_ID,
                                                           SAI_OBJECT_TYPE_PORT,
                                                           0,
                                                           nullptr,
                                                           0,
                                                           nullptr,
                                                           SAI_STATS_MODE_BULK_READ_AND_CLEAR,
                                                           nullptr,
                                                           nullptr));
    ASSERT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai.bulkClearStats(SAI_NULL_OBJECT_ID,
                                                             SAI_OBJECT_TYPE_PORT,
                                                             0,
                                                             nullptr,
                                                             0,
                                                             nullptr,
                                                             SAI_STATS_MODE_BULK_READ_AND_CLEAR,
                                                             nullptr));
}

sai_object_id_t create_port(
        _In_ VendorSai& sai,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();
    sai_object_id_t port;

    static uint32_t id = 1;
    id++;
    sai_attribute_t attrs[9] = { };

    uint32_t list[1] = { id };

    attrs[0].id = SAI_PORT_ATTR_HW_LANE_LIST;
    attrs[0].value.u32list.count = 1;
    attrs[0].value.u32list.list = list;

    attrs[1].id = SAI_PORT_ATTR_SPEED;
    attrs[1].value.u32 = 10000;

    auto status = sai.create(SAI_OBJECT_TYPE_PORT, &port, switch_id, 2, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return port;
}

sai_object_id_t create_rif(
        _In_ VendorSai& sai,
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_object_id_t vr_id)
{
    SWSS_LOG_ENTER();
    sai_object_id_t rif;

    sai_attribute_t attrs[9] = { };

    attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    attrs[0].value.oid = vr_id;

    attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    attrs[1].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;

    attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    attrs[2].value.oid = port_id;

    auto status = sai.create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, &rif, switch_id, 3, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return rif;
}

TEST(VendorSai, quad_bulk_neighbor_entry)
{
    VendorSai sai;
    sai.initialize(0, &test_services);

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    sai_object_id_t portId = create_port(sai, switchId);
    sai_object_id_t rifId = create_rif(sai, switchId, portId, vrId);

    // create

    sai_neighbor_entry_t e[2];

    memset(e, 0, sizeof(e));

    e[0].switch_id = switchId;
    e[1].switch_id = switchId;

    e[0].ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    e[0].ip_address.addr.ip4 = htonl(0x0a00000e);
    e[1].ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    e[1].ip_address.addr.ip4 = htonl(0x0a00000f);

    e[0].rif_id = rifId;
    e[1].rif_id = rifId;

    uint32_t attr_count[2];

    attr_count[0] = 2;
    attr_count[1] = 2;

    sai_attribute_t list1[2];
    sai_attribute_t list2[2];
    sai_mac_t mac1 = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    sai_mac_t mac2 = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    list1[0].id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(list1[0].value.mac, mac1, 6);
    list1[1].id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    list1[1].value.s32 = SAI_PACKET_ACTION_FORWARD;

    list2[0].id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(list2[0].value.mac, mac2, 6);
    list2[1].id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    list2[1].value.s32 = SAI_PACKET_ACTION_FORWARD;

    std::vector<const sai_attribute_t*> alist;

    alist.push_back(list1);
    alist.push_back(list2);

    const sai_attribute_t **attr_list = alist.data();

    sai_status_t statuses[2];

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate(2, e, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // set
    sai_attribute_t setlist[2];

    setlist[0].id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    setlist[0].value.s32 = SAI_PACKET_ACTION_FORWARD;

    setlist[1].id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    setlist[1].value.s32 = SAI_PACKET_ACTION_FORWARD;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkSet(2, e, setlist, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // remove
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove(2, e, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

}
