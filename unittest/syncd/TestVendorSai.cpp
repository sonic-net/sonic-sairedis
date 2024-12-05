#include <cstdint>

#include <memory>
#include <vector>
#include <array>

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

class VendorSaiTest : public ::testing::Test
{
public:
    VendorSaiTest() = default;
    virtual ~VendorSaiTest() = default;

public:
    virtual void SetUp() override
    {
        m_vsai = std::make_shared<VendorSai>();

        auto status = m_vsai->apiInitialize(0, &test_services);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        sai_attribute_t attr;
        attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        attr.value.booldata = true;

        status = m_vsai->create(SAI_OBJECT_TYPE_SWITCH, &m_swid, SAI_NULL_OBJECT_ID, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    virtual void TearDown() override
    {
        auto status = m_vsai->remove(SAI_OBJECT_TYPE_SWITCH, m_swid);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);

        status = m_vsai->apiUninitialize();
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

protected:
    std::shared_ptr<VendorSai> m_vsai;

    sai_object_id_t m_swid = SAI_NULL_OBJECT_ID;
};

TEST_F(VendorSaiTest, portBulkAddRemove)
{
    const std::uint32_t portCount = 1;
    const std::uint32_t laneCount = 4;

    // Generate port config
    std::array<std::uint32_t, laneCount> laneList = { 0, 1, 2, 3 };

    sai_attribute_t attr;
    std::vector<sai_attribute_t> attrList;

    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
    attr.value.u32list.count = static_cast<std::uint32_t>(laneList.size());
    attr.value.u32list.list = laneList.data();
    attrList.push_back(attr);

    attr.id = SAI_PORT_ATTR_SPEED;
    attr.value.u32 = 1000;
    attrList.push_back(attr);

    std::array<std::uint32_t, portCount> attrCountList = { static_cast<std::uint32_t>(attrList.size()) };
    std::array<const sai_attribute_t*, portCount> attrPtrList = { attrList.data() };

    std::array<sai_object_id_t, portCount> oidList = { SAI_NULL_OBJECT_ID };
    std::array<sai_status_t, portCount> statusList = { SAI_STATUS_SUCCESS };

    // Validate port bulk add
    auto status = m_vsai->bulkCreate(
        SAI_OBJECT_TYPE_PORT, m_swid, portCount, attrCountList.data(), attrPtrList.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        oidList.data(), statusList.data()
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (std::uint32_t i = 0; i < portCount; i++)
    {
        ASSERT_EQ(statusList.at(i), SAI_STATUS_SUCCESS);
    }

    // Validate port bulk remove
    status = m_vsai->bulkRemove(
        SAI_OBJECT_TYPE_PORT, portCount, oidList.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        statusList.data()
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (std::uint32_t i = 0; i < portCount; i++)
    {
        ASSERT_EQ(statusList.at(i), SAI_STATUS_SUCCESS);
    }
}

TEST(VendorSai, bulkGetStats)
{
    VendorSai sai;
    sai.apiInitialize(0, &test_services);
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
    sai.apiInitialize(0, &test_services);

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

static sai_object_id_t create_switch(VendorSai &sai)
{
    SWSS_LOG_ENTER();

    sai_object_id_t oid;
    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(SAI_OBJECT_TYPE_SWITCH, &oid, SAI_NULL_OBJECT_ID, 1, &attr));

    return oid;
}

static sai_object_id_t create_counter(VendorSai &sai, sai_object_id_t switchid)
{
    SWSS_LOG_ENTER();

    sai_object_id_t oid;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(SAI_OBJECT_TYPE_COUNTER, &oid, switchid, 0, nullptr));

    return oid;
}

static void remove_counter(VendorSai &sai, sai_object_id_t counter)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_COUNTER, counter));
}

static sai_object_id_t create_vnet(VendorSai &sai, sai_object_id_t switchid, uint32_t vni)
{
    SWSS_LOG_ENTER();

    sai_object_id_t oid;
    sai_attribute_t attr;

    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = vni;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_VNET, &oid, switchid, 1, &attr));

    return oid;
}

static void remove_vnet(VendorSai &sai, sai_object_id_t vnet)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_VNET, vnet));
}

static sai_object_id_t create_eni(VendorSai &sai, sai_object_id_t switchid, sai_object_id_t vnet)
{
    SWSS_LOG_ENTER();

    sai_object_id_t oid;
    sai_attribute_t attr;

    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_ENI, &oid, switchid, 1, &attr));

    return oid;
}

static void remove_eni(VendorSai &sai, sai_object_id_t eni)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_ENI, eni));
}

static sai_object_id_t create_outbound_routing_group(VendorSai &sai, sai_object_id_t switchid, bool disabled)
{
    SWSS_LOG_ENTER();

    sai_object_id_t oid;
    sai_attribute_t attr;

    attr.id = SAI_OUTBOUND_ROUTING_GROUP_ATTR_DISABLED;
    attr.value.booldata = disabled;

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_OUTBOUND_ROUTING_GROUP, &oid, switchid, 1, &attr));

    return oid;
}

static void remove_outbound_routing_group(VendorSai &sai, sai_object_id_t outbound_routing_group)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_OUTBOUND_ROUTING_GROUP, outbound_routing_group));
}

TEST(VendorSai, quad_dash_direction_lookup)
{
    VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    sai_direction_lookup_entry_t entry;

    entry.switch_id = switchid;
    entry.vni = 1;

    std::vector<sai_attribute_t> attrs;

    attr.id = SAI_DIRECTION_LOOKUP_ENTRY_ATTR_ACTION;
    attr.value.s32 = SAI_DIRECTION_LOOKUP_ENTRY_ACTION_SET_OUTBOUND_DIRECTION;
    attrs.push_back(attr);

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(&entry, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove(&entry));
}

TEST(VendorSai, bulk_dash_direction_lookup)
{
    VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 1;

    sai_attribute_t attrs0[entry_attrs_count] = {
        {.id = SAI_DIRECTION_LOOKUP_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_DIRECTION_LOOKUP_ENTRY_ACTION_SET_OUTBOUND_DIRECTION}},
    };

     sai_attribute_t attrs1[entry_attrs_count] = {
        {.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_DIRECTION_LOOKUP_ENTRY_ACTION_SET_OUTBOUND_DIRECTION}},
    };

    const sai_attribute_t *attr_list[entries_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[entries_count] = {entry_attrs_count, entry_attrs_count};
    sai_status_t statuses[entries_count] = {};

    sai_direction_lookup_entry_t entries[entries_count] = {
        { .switch_id = switchid, .vni = 10},
        { .switch_id = switchid, .vni = 20},
    };

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }
}

TEST(VendorSai, quad_dash_eni)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    sai_ip_address_t uip4, uip6;
    uip4.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &uip4.addr.ip4);
    uip6.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "100::ffff:ffff:ffff:ffff", &uip6.addr.ip6);

    sai_object_id_t vnet = create_vnet(sai, switchid, 101);

    std::vector<sai_attribute_t> attrs;

    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet;
    attrs.push_back(attr);

    attr.id = SAI_ENI_ATTR_ADMIN_STATE;
    attr.value.booldata = true;
    attrs.push_back(attr);

    attr.id = SAI_ENI_ATTR_VM_VNI;
    attr.value.u32 = 123;
    attrs.push_back(attr);

    attr.id = SAI_ENI_ATTR_CPS;
    attr.value.u32 = 10;
    attrs.push_back(attr);

    attr.id = SAI_ENI_ATTR_PPS;
    attr.value.u32 = 20;
    attrs.push_back(attr);

    attr.id = SAI_ENI_ATTR_FLOWS;
    attr.value.u32 = 30;
    attrs.push_back(attr);

    attr.id = SAI_ENI_ATTR_VM_UNDERLAY_DIP;
    attr.value.ipaddr = uip4;
    attrs.push_back(attr);

    sai_object_id_t eni;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_ENI, &eni, switchid, (uint32_t)attrs.size(), attrs.data()));

    remove_eni(sai, eni);
    remove_vnet(sai, vnet);
}

TEST(VendorSai, bulk_dash_eni)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t enis_count = 2;
    const uint32_t eni_attrs_count = 6;

    sai_object_id_t vnet0 = create_vnet(sai, switchid, 101);
    sai_object_id_t vnet1 = create_vnet(sai, switchid, 102);

    sai_ip_address_t ipaddr0, ipaddr1;
    ipaddr0.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &ipaddr0.addr.ip4);

    ipaddr1.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.1.1", &ipaddr1.addr.ip4);

    sai_attribute_t attrs0[] = {
        {.id = SAI_ENI_ATTR_VNET_ID, .value = (sai_attribute_value_t){.oid = vnet0}},
        {.id = SAI_ENI_ATTR_ADMIN_STATE, .value = (sai_attribute_value_t){.booldata = true}},
        {.id = SAI_ENI_ATTR_VM_VNI, .value = (sai_attribute_value_t){.u32 = 123}},
        {.id = SAI_ENI_ATTR_CPS, .value = (sai_attribute_value_t){.u32 = 10}},
        {.id = SAI_ENI_ATTR_FLOWS, .value = (sai_attribute_value_t){.u32 = 20}},
        {.id = SAI_ENI_ATTR_VM_UNDERLAY_DIP, .value = (sai_attribute_value_t){.ipaddr = ipaddr0}},
    };

    sai_attribute_t attrs1[] = {
        {.id = SAI_ENI_ATTR_VNET_ID, .value = (sai_attribute_value_t){.oid = vnet1}},
        {.id = SAI_ENI_ATTR_ADMIN_STATE, .value = (sai_attribute_value_t){.booldata = true}},
        {.id = SAI_ENI_ATTR_VM_VNI, .value = (sai_attribute_value_t){.u32 = 124}},
        {.id = SAI_ENI_ATTR_CPS, .value = (sai_attribute_value_t){.u32 = 11}},
        {.id = SAI_ENI_ATTR_FLOWS, .value = (sai_attribute_value_t){.u32 = 21}},
        {.id = SAI_ENI_ATTR_VM_UNDERLAY_DIP, .value = (sai_attribute_value_t){.ipaddr = ipaddr1}},
    };

    const sai_attribute_t *attr_list[enis_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[enis_count] = {eni_attrs_count, eni_attrs_count};
    sai_object_id_t enis[enis_count] = {};
    sai_status_t statuses[enis_count] = {};

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate((sai_object_type_t)SAI_OBJECT_TYPE_ENI, switchid, enis_count, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, enis, statuses));
    for (uint32_t i = 0; i < enis_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove((sai_object_type_t)SAI_OBJECT_TYPE_ENI, enis_count, enis, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < enis_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    remove_vnet(sai, vnet0);
    remove_vnet(sai, vnet1);
}

TEST(VendorSai, quad_dash_eni_acl)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    std::vector<sai_eni_attr_t> acl_attrs = {
        SAI_ENI_ATTR_INBOUND_V4_STAGE1_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_INBOUND_V4_STAGE2_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_INBOUND_V4_STAGE3_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_INBOUND_V4_STAGE4_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_INBOUND_V4_STAGE5_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_INBOUND_V6_STAGE1_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_INBOUND_V6_STAGE2_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_INBOUND_V6_STAGE3_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_INBOUND_V6_STAGE4_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_INBOUND_V6_STAGE5_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V4_STAGE1_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V4_STAGE2_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V4_STAGE3_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V4_STAGE4_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V4_STAGE5_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V6_STAGE1_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V6_STAGE2_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V6_STAGE3_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V6_STAGE4_DASH_ACL_GROUP_ID,
        SAI_ENI_ATTR_OUTBOUND_V6_STAGE5_DASH_ACL_GROUP_ID,
    };

    std::vector<sai_attribute_t> attrs;
    std::vector<sai_object_id_t> acl_groups, acl_groups_new;
    for (auto at : acl_attrs) {
        sai_object_id_t acl_group;
        EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, &acl_group, switchid, 0, nullptr));
        acl_groups.push_back(acl_group);

        attr.id = at;
        attr.value.oid = acl_group;
        attrs.push_back(attr);

        EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, &acl_group, switchid, 0, nullptr));
        acl_groups_new.push_back(acl_group);
    }

    sai_object_id_t eni;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_ENI, &eni, switchid, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_ENI, eni));
}

TEST(VendorSai, quad_dash_vip)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    SWSS_LOG_ENTER();

    std::vector<sai_attribute_t> attrs;

    attr.id = SAI_VIP_ENTRY_ATTR_ACTION;
    attr.value.s32 = SAI_VIP_ENTRY_ACTION_ACCEPT;
    attrs.push_back(attr);

    sai_ip_address_t vip_addr;
    vip_addr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &vip_addr.addr.ip4);

    sai_vip_entry_t vip = { .switch_id = switchid, .vip = vip_addr };
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(&vip, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove(&vip));
}

TEST(VendorSai, bulk_dash_vip)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t vips_count = 2;
    const uint32_t vip_attrs_count = 1;

    sai_attribute_t attrs0[] = {
        {.id = SAI_VIP_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_VIP_ENTRY_ACTION_ACCEPT}},
    };

     sai_attribute_t attrs1[] = {
        {.id = SAI_VIP_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_VIP_ENTRY_ACTION_ACCEPT}},
    };

    const sai_attribute_t *attr_list[vips_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[vips_count] = {vip_attrs_count, vip_attrs_count};
    sai_status_t statuses[vips_count] = {};

    sai_ip_address_t vip_addr0, vip_addr1;
    vip_addr0.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &vip_addr0.addr.ip4);
    vip_addr1.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "100::ffff:ffff:ffff:ffff", &vip_addr1.addr.ip6);

    sai_vip_entry_t vips[vips_count] = {
        {.switch_id = switchid, .vip = vip_addr0},
        {.switch_id = switchid, .vip = vip_addr1},
    };

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate(vips_count, vips, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < vips_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove(vips_count, vips, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < vips_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }
}

TEST(VendorSai, quad_dash_acl_group)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    std::vector<sai_attribute_t> attrs;

    attr.id = SAI_DASH_ACL_GROUP_ATTR_IP_ADDR_FAMILY;
    attr.value.s32 = SAI_IP_ADDR_FAMILY_IPV4;
    attrs.push_back(attr);

    sai_object_id_t acl_group;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, &acl_group, switchid, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, acl_group));
}

TEST(VendorSai, bulk_dash_acl_group)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t acls_count = 2;
    const uint32_t acl_attrs_count = 1;

    sai_attribute_t attrs0[acl_attrs_count] = {
        {.id = SAI_DASH_ACL_GROUP_ATTR_IP_ADDR_FAMILY, .value = (sai_attribute_value_t){.s32 = SAI_IP_ADDR_FAMILY_IPV4}},
    };

    sai_attribute_t attrs1[acl_attrs_count] = {
        {.id = SAI_DASH_ACL_GROUP_ATTR_IP_ADDR_FAMILY, .value = (sai_attribute_value_t){.s32 = SAI_IP_ADDR_FAMILY_IPV6}},
    };

    const sai_attribute_t *attr_list[acls_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[acls_count] = {acl_attrs_count, acl_attrs_count};
    sai_object_id_t acls[acls_count];
    sai_status_t statuses[acls_count] = {};

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, switchid, acls_count, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, acls, statuses));
    for (uint32_t i = 0; i < acls_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, acls_count, acls, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < acls_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }
}

TEST(VendorSai, quad_dash_acl_rule)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    sai_object_id_t group;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, &group, switchid, 0, nullptr));

    sai_object_id_t counter = create_counter(sai, switchid);

    sai_ip_prefix_t ip_addr_list[2] = {};

    ip_addr_list[0].addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &ip_addr_list[0].addr.ip4);
    inet_pton(AF_INET, "255.255.0.0", &ip_addr_list[0].mask.ip4);
    ip_addr_list[1].addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "100::ffff:ffff:ffff:ffff", &ip_addr_list[1].addr.ip6);
    inet_pton(AF_INET6, "ffff:fff0::", &ip_addr_list[1].mask.ip6);
    sai_ip_prefix_list_t ip_prefix_list0 = {.count = 2, .list = ip_addr_list};
    sai_ip_prefix_list_t ip_prefix_list1 = {.count = 1, .list = ip_addr_list};

    uint8_t protos[2] = {0xAA, 0xBB};
    sai_u8_list_t protos_list = {.count=2, .list = protos};

    sai_u16_range_t port_ranges[2] = {{.min = 10, .max = 20}, {.min = 30, .max = 40}};
    sai_u16_range_list_t port_ranges_list0 = {.count=2, .list = port_ranges};
    sai_u16_range_list_t port_ranges_list1 = {.count=1, .list = port_ranges};

    std::vector<sai_attribute_t> attrs;

    attr.id = SAI_DASH_ACL_RULE_ATTR_ACTION;
    attr.value.s32 = SAI_DASH_ACL_RULE_ACTION_PERMIT;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_DASH_ACL_GROUP_ID;
    attr.value.oid = group;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_DIP;
    attr.value.ipprefixlist = ip_prefix_list0;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_SIP;
    attr.value.ipprefixlist = ip_prefix_list1;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_PROTOCOL;
    attr.value.u8list = protos_list;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_SRC_PORT;
    attr.value.u16rangelist = port_ranges_list0;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_DST_PORT;
    attr.value.u16rangelist = port_ranges_list1;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_COUNTER_ID;
    attr.value.oid = counter;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_PRIORITY;
    attr.value.u32 = 1;
    attrs.push_back(attr);

    sai_object_id_t acl;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_RULE, &acl, switchid, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_RULE, acl));
    remove_counter(sai, counter);
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, group));
}

TEST(VendorSai, bulk_dash_acl_rule)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t acls_count = 2;
    const uint32_t acl_attrs_count = 9;

    sai_object_id_t counter0 = create_counter(sai, switchid);
    sai_object_id_t counter1 = create_counter(sai, switchid);

    sai_object_id_t group0;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, &group0, switchid, 0, nullptr));

    sai_object_id_t group1;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, &group1, switchid, 0, nullptr));

    sai_ip_prefix_t ip_prefix_arr0[2] = {};
    ip_prefix_arr0[0].addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &ip_prefix_arr0[0].addr.ip4);
    inet_pton(AF_INET, "255.255.255.0", &ip_prefix_arr0[0].mask.ip4);
    ip_prefix_arr0[1].addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "100::ffff:ffff:ffff:ffff", &ip_prefix_arr0[1].addr.ip6);
    inet_pton(AF_INET6, "ffff::", &ip_prefix_arr0[1].mask.ip6);

    sai_ip_prefix_t ip_prefix_arr1[2] = {};
    ip_prefix_arr1[0].addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.2", &ip_prefix_arr1[0].addr.ip4);
    inet_pton(AF_INET, "255.255.0.0", &ip_prefix_arr1[0].mask.ip4);
    ip_prefix_arr1[1].addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "100::ffff:ffff:ffff:fffe", &ip_prefix_arr1[1].addr.ip6);
    inet_pton(AF_INET6, "ffff::", &ip_prefix_arr1[1].mask.ip6);

    sai_ip_prefix_list_t ip_prefix_list0 = {.count = 1, .list = ip_prefix_arr0};
    sai_ip_prefix_list_t ip_prefix_list1 = {.count = 1, .list = ip_prefix_arr1};

    uint8_t protos0[2] = {0xAA, 0xBB};
    uint8_t protos1[2] = {0xCC, 0xDD};
    sai_u8_list_t protos_list0 = {.count=2, .list = protos0};
    sai_u8_list_t protos_list1 = {.count=2, .list = protos1};

    sai_u16_range_t port_ranges0[2] = {{.min = 10, .max = 20}, {.min = 30, .max = 40}};
    sai_u16_range_t port_ranges1[2] = {{.min = 50, .max = 60}, {.min = 70, .max = 80}};
    sai_u16_range_list_t u16_range_list0 = {.count=2, .list = port_ranges0};
    sai_u16_range_list_t u16_range_list1 = {.count=2, .list = port_ranges1};

    sai_attribute_t attrs0[acl_attrs_count] = {
        {.id = SAI_DASH_ACL_RULE_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_DASH_ACL_RULE_ACTION_PERMIT_AND_CONTINUE}},
        {.id = SAI_DASH_ACL_RULE_ATTR_DASH_ACL_GROUP_ID, .value = (sai_attribute_value_t){.oid = group0}},
        {.id = SAI_DASH_ACL_RULE_ATTR_DIP, .value = (sai_attribute_value_t){.ipprefixlist = ip_prefix_list0}},
        {.id = SAI_DASH_ACL_RULE_ATTR_SIP, .value = (sai_attribute_value_t){.ipprefixlist = ip_prefix_list1}},
        {.id = SAI_DASH_ACL_RULE_ATTR_PROTOCOL, .value = (sai_attribute_value_t){.u8list = protos_list0}},
        {.id = SAI_DASH_ACL_RULE_ATTR_SRC_PORT, .value = (sai_attribute_value_t){.u16rangelist = u16_range_list0}},
        {.id = SAI_DASH_ACL_RULE_ATTR_DST_PORT, .value = (sai_attribute_value_t){.u16rangelist = u16_range_list1}},
        {.id = SAI_DASH_ACL_RULE_ATTR_COUNTER_ID, .value = (sai_attribute_value_t){.oid = counter0}},
        {.id = SAI_DASH_ACL_RULE_ATTR_PRIORITY, .value = (sai_attribute_value_t){.u32 = 1}},
    };

    sai_attribute_t attrs1[acl_attrs_count] = {
        {.id = SAI_DASH_ACL_RULE_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_DASH_ACL_RULE_ACTION_DENY}},
        {.id = SAI_DASH_ACL_RULE_ATTR_DASH_ACL_GROUP_ID, .value = (sai_attribute_value_t){.oid = group1}},
        {.id = SAI_DASH_ACL_RULE_ATTR_DIP, .value = (sai_attribute_value_t){.ipprefixlist = ip_prefix_list0}},
        {.id = SAI_DASH_ACL_RULE_ATTR_SIP, .value = (sai_attribute_value_t){.ipprefixlist = ip_prefix_list1}},
        {.id = SAI_DASH_ACL_RULE_ATTR_PROTOCOL, .value = (sai_attribute_value_t){.u8list = protos_list1}},
        {.id = SAI_DASH_ACL_RULE_ATTR_SRC_PORT, .value = (sai_attribute_value_t){.u16rangelist = u16_range_list1}},
        {.id = SAI_DASH_ACL_RULE_ATTR_DST_PORT, .value = (sai_attribute_value_t){.u16rangelist = u16_range_list0}},
        {.id = SAI_DASH_ACL_RULE_ATTR_COUNTER_ID, .value = (sai_attribute_value_t){.oid = counter1}},
        {.id = SAI_DASH_ACL_RULE_ATTR_PRIORITY, .value = (sai_attribute_value_t){.u32 = 2}},
    };

    const sai_attribute_t *attr_list[acls_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[acls_count] = {acl_attrs_count, acl_attrs_count};
    sai_object_id_t acls[acls_count];
    sai_status_t statuses[acls_count] = {};

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_RULE, switchid, acls_count, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, acls, statuses));
    for (uint32_t i = 0; i < acls_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_RULE, acls_count, acls, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < acls_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, group0));
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_DASH_ACL_GROUP, group1));
    remove_counter(sai, counter0);
    remove_counter(sai, counter1);
}

TEST(VendorSai, quad_dash_vnet)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    sai_object_id_t vnet;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 10;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_VNET, &vnet, switchid, 1, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_VNET, vnet));
}

TEST(VendorSai, bulk_dash_vnet)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t vnets_count = 2;
    const uint32_t vnet_attrs_count = 1;

    sai_attribute_t attrs0[] = {
        {.id = SAI_VNET_ATTR_VNI, .value = (sai_attribute_value_t){.u32 = 10}},
    };

     sai_attribute_t attrs1[] = {
        {.id = SAI_VNET_ATTR_VNI, .value = (sai_attribute_value_t){.u32 = 20}},
    };

    const sai_attribute_t *attr_list[vnets_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[vnets_count] = {vnet_attrs_count, vnet_attrs_count};
    sai_object_id_t vnets[vnets_count];
    sai_status_t statuses[vnets_count] = {};

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate((sai_object_type_t)SAI_OBJECT_TYPE_VNET, switchid, vnets_count, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, vnets, statuses));
    for (uint32_t i = 0; i < vnets_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove((sai_object_type_t)SAI_OBJECT_TYPE_VNET, vnets_count, vnets, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < vnets_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }
}

TEST(VendorSai, quad_dash_inbound_routing_entry)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    sai_object_id_t vnet = create_vnet(sai, switchid, 10);
    sai_object_id_t eni = create_eni(sai, switchid, vnet);

    sai_ip_address_t sip, sip_mask;
    sip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &sip.addr.ip4);
    sip_mask.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "255.255.255.0", &sip_mask.addr.ip4);

    sai_inbound_routing_entry_t entry = { .switch_id = switchid, .eni_id = eni, .vni = 10, .sip = sip, .sip_mask = sip_mask, .priority = 1};

    std::vector<sai_attribute_t> attrs;
    attr.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_ACTION;
    attr.value.s32 = SAI_INBOUND_ROUTING_ENTRY_ACTION_VXLAN_DECAP_PA_VALIDATE;
    attrs.push_back(attr);

    attr.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_SRC_VNET_ID;
    attr.value.oid = vnet;
    attrs.push_back(attr);

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(&entry, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove(&entry));
    remove_eni(sai, eni);
    remove_vnet(sai, vnet);
}

TEST(VendorSai, bulk_dash_inbound_routing_entry)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 2;

    sai_object_id_t vnet0 = create_vnet(sai, switchid, 101);
    sai_object_id_t eni0 = create_eni(sai, switchid, vnet0);

    sai_object_id_t vnet1 = create_vnet(sai, switchid, 102);
    sai_object_id_t eni1 = create_eni(sai, switchid, vnet0);

    sai_ip_address_t sip0, sip_mask0;
    sai_ip_address_t sip1, sip_mask1;
    sip0.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &sip0.addr.ip4);
    sip_mask0.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "255.255.255.0", &sip_mask0.addr.ip4);
    sip1.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.2", &sip1.addr.ip4);
    sip_mask1.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "255.255.0.0", &sip_mask1.addr.ip4);

    sai_attribute_t attrs0[entry_attrs_count] = {
        {.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_INBOUND_ROUTING_ENTRY_ACTION_VXLAN_DECAP}},
        {.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_SRC_VNET_ID, .value = (sai_attribute_value_t){.oid = vnet0}},
    };

     sai_attribute_t attrs1[entry_attrs_count] = {
        {.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_INBOUND_ROUTING_ENTRY_ACTION_VXLAN_DECAP}},
        {.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_SRC_VNET_ID, .value = (sai_attribute_value_t){.oid = vnet1}},
    };

    const sai_attribute_t *attr_list[entries_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[entries_count] = {entry_attrs_count, entry_attrs_count};
    sai_status_t statuses[entries_count] = {};

    sai_inbound_routing_entry_t entries[entries_count] = {
        { .switch_id = switchid, .eni_id = eni0, .vni = 10, .sip = sip0, .sip_mask = sip_mask0, .priority = 1},
        { .switch_id = switchid, .eni_id = eni1, .vni = 100, .sip = sip1, .sip_mask = sip_mask1, .priority = 2}
    };

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    remove_eni(sai, eni0);
    remove_eni(sai, eni1);
    remove_vnet(sai, vnet0);
    remove_vnet(sai, vnet1);
}

TEST(VendorSai, quad_dash_pa_validation)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    sai_object_id_t vnet = create_vnet(sai, switchid, 10);

    sai_pa_validation_entry_t entry;
    entry.switch_id = switchid;
    entry.vnet_id = vnet;
    entry.sip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.3.3.3", &entry.sip.addr.ip4);

    std::vector<sai_attribute_t> attrs;
    attr.id = SAI_PA_VALIDATION_ENTRY_ATTR_ACTION;
    attr.value.s32 = SAI_PA_VALIDATION_ENTRY_ACTION_PERMIT;
    attrs.push_back(attr);

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(&entry, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove(&entry));
    remove_vnet(sai, vnet);
}

TEST(VendorSai, bulk_dash_pa_validation)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 1;

    sai_object_id_t vnet0 = create_vnet(sai, switchid, 10);
    sai_object_id_t vnet1 = create_vnet(sai, switchid, 20);

    sai_ip_address_t sip0 = {};
    sai_ip_address_t sip1 = {};
    sip0.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    sip1.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET, "192.1.1.1", &sip0.addr.ip4);
    inet_pton(AF_INET6, "ffff::", &sip1.addr.ip6);

    sai_attribute_t attrs0[entry_attrs_count] = {
        {.id = SAI_PA_VALIDATION_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_PA_VALIDATION_ENTRY_ACTION_PERMIT}},
    };

     sai_attribute_t attrs1[entry_attrs_count] = {
        {.id = SAI_PA_VALIDATION_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_PA_VALIDATION_ENTRY_ACTION_PERMIT}},
    };

    const sai_attribute_t *attr_list[entries_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[entries_count] = {entry_attrs_count, entry_attrs_count};
    sai_status_t statuses[entries_count] = {};

    sai_pa_validation_entry_t entries[entries_count] = {
        { .switch_id = switchid, .vnet_id = vnet0, .sip = sip0},
        { .switch_id = switchid, .vnet_id = vnet1, .sip = sip1},
    };

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    remove_vnet(sai, vnet0);
    remove_vnet(sai, vnet1);
}

TEST(VendorSai, quad_dash_outbound_routing_entry)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;

    sai_object_id_t counter = create_counter(sai, switchid);
    sai_object_id_t vnet = create_vnet(sai, switchid, 101);
    sai_object_id_t outbound_routing_group = create_outbound_routing_group(sai, switchid, false);

    sai_ip_address_t oip6;
    oip6.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "ffff::", &oip6.addr);

    sai_outbound_routing_entry_t entry0;
    entry0.switch_id = switchid;
    entry0.outbound_routing_group_id = outbound_routing_group;
    entry0.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.1.0", &entry0.destination.addr.ip4);
    inet_pton(AF_INET, "255.255.255.0", &entry0.destination.mask.ip4);

    std::vector<sai_attribute_t> attrs;
    attr.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_ACTION;
    attr.value.u32 = SAI_OUTBOUND_ROUTING_ENTRY_ACTION_ROUTE_VNET;
    attrs.push_back(attr);
    attr.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_DST_VNET_ID;
    attr.value.oid = vnet;
    attrs.push_back(attr);
    attr.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_OVERLAY_IP;
    attr.value.ipaddr = oip6;
    attrs.push_back(attr);
    attr.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_COUNTER_ID;
    attr.value.oid = counter;
    attrs.push_back(attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(&entry0, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove(&entry0));

    remove_outbound_routing_group(sai, outbound_routing_group);
    remove_vnet(sai, vnet);
    remove_counter(sai, counter);
}

TEST(VendorSai, bulk_dash_outbound_routing_entry)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 4;

    sai_object_id_t counter0 = create_counter(sai, switchid);
    sai_object_id_t counter1 = create_counter(sai, switchid);

    sai_ip_address_t oip4, oip6;
    oip4.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "1.2.3.4", &oip4.addr);
    oip6.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "ffff::", &oip6.addr);

    sai_object_id_t vnet0 = create_vnet(sai, switchid, 101);
    sai_object_id_t vnet1 = create_vnet(sai, switchid, 102);
    sai_object_id_t outbound_routing_group0 = create_outbound_routing_group(sai, switchid, false);
    sai_object_id_t outbound_routing_group1 = create_outbound_routing_group(sai, switchid, false);

    sai_ip_prefix_t dst0 = {};
    sai_ip_prefix_t dst1 = {};
    dst0.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    dst1.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET, "192.1.1.1", &dst0.addr.ip4);
    inet_pton(AF_INET, "255.255.255.0", &dst0.mask.ip4);
    inet_pton(AF_INET6, "fe80::886a:feff:fe31:bfe0", &dst1.addr.ip6);
    inet_pton(AF_INET6, "ffff:ffff::", &dst1.mask.ip6);

    sai_attribute_t attrs0[entry_attrs_count] = {
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_OUTBOUND_ROUTING_ENTRY_ACTION_ROUTE_VNET}},
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_DST_VNET_ID, .value = (sai_attribute_value_t){.oid = vnet0}},
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_OVERLAY_IP, .value = (sai_attribute_value_t){.ipaddr = oip4}},
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_COUNTER_ID, .value = (sai_attribute_value_t){.oid = counter0}},
    };

     sai_attribute_t attrs1[entry_attrs_count] = {
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_OUTBOUND_ROUTING_ENTRY_ACTION_ROUTE_VNET}},
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_DST_VNET_ID, .value = (sai_attribute_value_t){.oid = vnet1}},
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_OVERLAY_IP, .value = (sai_attribute_value_t){.ipaddr = oip6}},
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_COUNTER_ID, .value = (sai_attribute_value_t){.oid = counter1}},
    };

    const sai_attribute_t *attr_list[entries_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[entries_count] = {entry_attrs_count, entry_attrs_count};
    sai_status_t statuses[entries_count] = {};

    sai_outbound_routing_entry_t entries[entries_count] = {
        { .switch_id = switchid, .destination = dst0, .outbound_routing_group_id = outbound_routing_group0},
        { .switch_id = switchid, .destination = dst1, .outbound_routing_group_id = outbound_routing_group1},
    };

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    remove_outbound_routing_group(sai, outbound_routing_group0);
    remove_outbound_routing_group(sai, outbound_routing_group1);
    remove_vnet(sai, vnet0);
    remove_vnet(sai, vnet1);
    remove_counter(sai, counter0);
    remove_counter(sai, counter1);
}

TEST(VendorSai, quad_dash_outbound_ca_to_pa_entry)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_object_id_t counter = create_counter(sai, switchid);

    sai_ip_address_t uip4;
    uip4.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &uip4.addr.ip4);

    sai_object_id_t vnet = create_vnet(sai, switchid, 10);

    sai_outbound_ca_to_pa_entry_t entry;
    entry.switch_id = switchid;
    entry.dst_vnet_id = vnet;
    entry.dip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &entry.dip.addr.ip4);

    sai_attribute_t attr;
    std::vector<sai_attribute_t> attrs;

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_UNDERLAY_DIP;
    attr.value.ipaddr = uip4;
    attrs.push_back(attr);

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_OVERLAY_DMAC;
    sai_mac_t dmac = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
    memcpy(attr.value.mac, dmac, 6);
    attrs.push_back(attr);

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI;
    attr.value.booldata = true;
    attrs.push_back(attr);

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_COUNTER_ID;
    attr.value.oid = counter;
    attrs.push_back(attr);

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create(&entry, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove(&entry));

    remove_vnet(sai, vnet);
    remove_counter(sai, counter);
}

TEST(VendorSai, bulk_dash_outbound_ca_to_pa_entry)
{
        VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 4;

    sai_object_id_t counter0 = create_counter(sai, switchid);
    sai_object_id_t counter1 = create_counter(sai, switchid);

    sai_object_id_t vnet0 = create_vnet(sai, switchid, 10);
    sai_object_id_t vnet1 = create_vnet(sai, switchid, 20);

    sai_ip_address_t dip0 = {};
    sai_ip_address_t dip1 = {};
    sai_ip_address_t udip0 = {};
    sai_ip_address_t udip1 = {};
    dip0.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    dip1.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    udip0.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    udip1.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.1.1.1", &dip0.addr.ip4);
    inet_pton(AF_INET6, "fe80::886a:feff:fe31:bfe0", &dip1.addr.ip6);
    inet_pton(AF_INET6, "fe80::886a:feff:fe31:bfe1", &udip0.addr.ip6);
    inet_pton(AF_INET, "192.1.1.2", &udip1.addr.ip4);

    sai_attribute_t attrs0[entry_attrs_count] = {
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_UNDERLAY_DIP, .value = (sai_attribute_value_t){.ipaddr = udip0}},
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_OVERLAY_DMAC, .value = (sai_attribute_value_t){.mac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}}},
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI, .value = (sai_attribute_value_t){.booldata = true}},
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_COUNTER_ID, .value = (sai_attribute_value_t){.oid = counter0}}
    };

     sai_attribute_t attrs1[entry_attrs_count] = {
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_UNDERLAY_DIP, .value = (sai_attribute_value_t){.ipaddr = udip1}},
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_OVERLAY_DMAC, .value = (sai_attribute_value_t){.mac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x56}}},
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI, .value = (sai_attribute_value_t){.booldata = false}},
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_COUNTER_ID, .value = (sai_attribute_value_t){.oid = counter1}}
    };

    const sai_attribute_t *attr_list[entries_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[entries_count] = {entry_attrs_count, entry_attrs_count};
    sai_status_t statuses[entries_count] = {};

    sai_outbound_ca_to_pa_entry_t entries[entries_count] = {
        { .switch_id = switchid, .dst_vnet_id = vnet0, .dip = dip0},
        { .switch_id = switchid, .dst_vnet_id = vnet1, .dip = dip1},
    };

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    remove_vnet(sai, vnet0);
    remove_vnet(sai, vnet1);
    remove_counter(sai, counter0);
    remove_counter(sai, counter1);
}

TEST(VendorSai, bulkGet)
{
    VendorSai sai;

    sai_object_id_t oids[1] = {0};
    uint32_t attrcount[1] = {0};
    sai_attribute_t* attrs[1] = {0};
    sai_status_t statuses[1] = {0};

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                1,
                oids,
                attrcount,
                attrs,
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses));
}

TEST_F(VendorSaiTest, bulk_flow_entry)
{
    sai_flow_entry_t *e = nullptr;

    // metadata will fail
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
            m_vsai->bulkCreate(0, e, nullptr, nullptr, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, nullptr));

    // metadata will fail
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
            m_vsai->bulkRemove(0, e, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, nullptr));

    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED,
            m_vsai->bulkSet(0, e, nullptr, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, nullptr));
}

TEST_F(VendorSaiTest, bulk_meter_bucket_entry)
{
    sai_meter_bucket_entry_t *e = nullptr;

    // metadata will fail
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
            m_vsai->bulkCreate(0, e, nullptr, nullptr, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, nullptr));

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
            m_vsai->bulkRemove(0, e, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, nullptr));

    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED,
            m_vsai->bulkSet(0, e, nullptr, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, nullptr));
}

TEST(VendorSai, bulk_meter_rules)
{
    VendorSai sai;
    sai.apiInitialize(0, &test_services);

    sai_object_id_t switchid = create_switch(sai);

    sai_attribute_t attr;
    sai_object_id_t meter_policy0, meter_policy1;
    attr.id = SAI_METER_POLICY_ATTR_IP_ADDR_FAMILY;
    attr.value.s32 = SAI_IP_ADDR_FAMILY_IPV4;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_METER_POLICY, &meter_policy0, switchid, 1, &attr));
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.create((sai_object_type_t)SAI_OBJECT_TYPE_METER_POLICY, &meter_policy1, switchid, 1, &attr));

    sai_ip_address_t dst0 = {};
    sai_ip_address_t mask0 = {};
    sai_ip_address_t dst1 = {};
    sai_ip_address_t mask1 = {};
    dst0.addr_family = dst1.addr_family = mask0.addr_family = mask1.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.1.1.0", &dst0.addr.ip4);
    inet_pton(AF_INET, "255.255.255.0", &mask0.addr.ip4);
    inet_pton(AF_INET, "192.15.0.0", &dst1.addr.ip4);
    inet_pton(AF_INET, "255.255.0.0", &mask1.addr.ip4);

    sai_attribute_t attrs0[] = {
        {.id = SAI_METER_RULE_ATTR_METER_POLICY_ID, .value = (sai_attribute_value_t){.oid = meter_policy0}},
        {.id = SAI_METER_RULE_ATTR_DIP, .value = (sai_attribute_value_t){.ipaddr = dst0}},
        {.id = SAI_METER_RULE_ATTR_DIP_MASK, .value = (sai_attribute_value_t){.ipaddr = mask0}},
        {.id = SAI_METER_RULE_ATTR_METER_CLASS, .value = (sai_attribute_value_t){.u32 = 100}},
        {.id = SAI_METER_RULE_ATTR_PRIORITY, .value = (sai_attribute_value_t){.u32 = 1}},
    };

     sai_attribute_t attrs1[] = {
        {.id = SAI_METER_RULE_ATTR_METER_POLICY_ID, .value = (sai_attribute_value_t){.oid = meter_policy1}},
        {.id = SAI_METER_RULE_ATTR_DIP, .value = (sai_attribute_value_t){.ipaddr = dst1}},
        {.id = SAI_METER_RULE_ATTR_DIP_MASK, .value = (sai_attribute_value_t){.ipaddr = mask1}},
        {.id = SAI_METER_RULE_ATTR_METER_CLASS, .value = (sai_attribute_value_t){.u32 = 200}},
        {.id = SAI_METER_RULE_ATTR_PRIORITY, .value = (sai_attribute_value_t){.u32 = 2}},
    };

    const sai_attribute_t *attr_list[] = {
        attrs0,
        attrs1,
    };
    constexpr uint32_t meter_rules_count = sizeof(attr_list) / sizeof(sai_attribute_t*);
    constexpr uint32_t meter_rule_attrs_count = sizeof(attrs0) / sizeof(sai_attribute_t);

    uint32_t attr_count[meter_rules_count] = {meter_rule_attrs_count, meter_rule_attrs_count};
    sai_object_id_t meter_rules[meter_rules_count];
    sai_status_t statuses[meter_rules_count] = {};

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkCreate((sai_object_type_t)SAI_OBJECT_TYPE_METER_RULE, switchid, meter_rules_count, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, meter_rules, statuses));
    for (uint32_t i = 0; i < meter_rules_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.bulkRemove((sai_object_type_t)SAI_OBJECT_TYPE_METER_RULE, meter_rules_count, meter_rules, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < meter_rules_count; i++) {
        EXPECT_EQ(SAI_STATUS_SUCCESS, statuses[i]);
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_METER_POLICY, meter_policy0));
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai.remove((sai_object_type_t)SAI_OBJECT_TYPE_METER_POLICY, meter_policy1));
}
