#include "TestDashEnv.h"

#include "swss/logger.h"

#include <arpa/inet.h>
#include <vector>
#include <algorithm>

using std::vector;

#define ASSERT_SUCCESS(e) ASSERT_EQ((e), SAI_STATUS_SUCCESS)

template<typename L>
static bool compare_lists(L la, L lb)
{
    // SWSS_LOG_ENTER(); // disabled

    if (la.count != lb.count) {
        return false;
    }

    return std::equal(la.list, la.list + la.count, lb.list);
}

static bool operator==(sai_ip_address_t a, sai_ip_address_t b)
{
    if (a.addr_family != b.addr_family) {
        return false;
    }

    switch (a.addr_family) {
        case SAI_IP_ADDR_FAMILY_IPV4:
            return a.addr.ip4 == b.addr.ip4;
        case SAI_IP_ADDR_FAMILY_IPV6:
            return memcmp(a.addr.ip6, b.addr.ip6, sizeof(a.addr.ip6)) == 0;
        default:
            return false;
    }
}

static bool operator==(sai_ip_prefix_t a, sai_ip_prefix_t b)
{
    if (a.addr_family != b.addr_family) {
        return false;
    }

    switch (a.addr_family) {
        case SAI_IP_ADDR_FAMILY_IPV4:
            return a.addr.ip4 == b.addr.ip4 && a.mask.ip4 == b.mask.ip4;
        case SAI_IP_ADDR_FAMILY_IPV6:
            return memcmp(a.addr.ip6, b.addr.ip6, sizeof(a.addr.ip6)) == 0 &&
                memcmp(a.mask.ip6, b.mask.ip6, sizeof(a.mask.ip6)) == 0;
        default:
            return false;
    }
}

static bool operator==(sai_u16_range_t a, sai_u16_range_t b)
{
    return (a.min == b.min) && (a.max == b.max);
}

class API : public testing::TestWithParam<bool> {};
class APIBulk : public testing::TestWithParam<bool> {};

static std::string GenTestType(const testing::TestParamInfo<bool>& p)
{
    // SWSS_LOG_ENTER(); // disabled
    return p.param ? "CreateRemoveGetSet" : "CreateRemove";
}

INSTANTIATE_TEST_CASE_P(DASH, API, testing::Values(true, false), GenTestType);
INSTANTIATE_TEST_CASE_P(DASH, APIBulk, testing::Values(true, false), GenTestType);

TEST_P(API, direction_lookup)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_direction_lookup_api_t *dash_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_DIRECTION_LOOKUP, (void**)&dash_api));

    sai_direction_lookup_entry_t entry;

    entry.switch_id = switchid;
    entry.vni = 1;

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_DIRECTION_LOOKUP_ENTRY_ATTR_ACTION;
    attr.value.s32 = SAI_DIRECTION_LOOKUP_ENTRY_ACTION_SET_OUTBOUND_DIRECTION;
    attrs.push_back(attr);

    ASSERT_SUCCESS(dash_api->create_direction_lookup_entry(&entry, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        attr.id = SAI_DIRECTION_LOOKUP_ENTRY_ATTR_ACTION;
        attr.value.s32 = SAI_DIRECTION_LOOKUP_ENTRY_ACTION_DENY;
        ASSERT_SUCCESS(dash_api->set_direction_lookup_entry_attribute(&entry, &attr));

        ASSERT_SUCCESS(dash_api->get_direction_lookup_entry_attribute(&entry, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.s32 == SAI_DIRECTION_LOOKUP_ENTRY_ACTION_DENY);
    }

    ASSERT_SUCCESS(dash_api->remove_direction_lookup_entry(&entry));
}

TEST_P(APIBulk, direction_lookup)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 1;

    sai_dash_direction_lookup_api_t *dash_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_DIRECTION_LOOKUP, (void**)&dash_api));

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

    ASSERT_SUCCESS(dash_api->create_direction_lookup_entries(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < entries_count; i++) {
            sai_attribute_t attr = {};
            attr.id = SAI_DIRECTION_LOOKUP_ENTRY_ATTR_ACTION;
            ASSERT_SUCCESS(dash_api->get_direction_lookup_entry_attribute(&entries[i], 1, &attr));
            ASSERT_TRUE(attr.value.s32 == attr_list[i][0].value.s32);
        }
    }

    ASSERT_SUCCESS(dash_api->remove_direction_lookup_entries(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }
}

TEST_P(API, eni_ether_address_map_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_eni_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ENI, (void**)&dash_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_attribute_t attr;
    sai_object_id_t eni0, vnet0;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 100;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet0, switchid, 1, &attr));

    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet0;
    ASSERT_SUCCESS(dash_api->create_eni(&eni0, switchid, 1, &attr));

    sai_object_id_t eni1, vnet1;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 200;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet1, switchid, 1, &attr));

    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet1;
    ASSERT_SUCCESS(dash_api->create_eni(&eni1, switchid, 1, &attr));

    sai_eni_ether_address_map_entry_t entry = { .switch_id = switchid, .address = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 }};

    vector<sai_attribute_t> attrs;
    attr.id = SAI_ENI_ETHER_ADDRESS_MAP_ENTRY_ATTR_ENI_ID;
    attr.value.oid = eni0;
    attrs.push_back(attr);

    ASSERT_SUCCESS(dash_api->create_eni_ether_address_map_entry(&entry, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        ASSERT_SUCCESS(dash_api->get_eni_ether_address_map_entry_attribute(&entry, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.oid == eni0);

        attr.id = SAI_ENI_ETHER_ADDRESS_MAP_ENTRY_ATTR_ENI_ID;
        attr.value.oid = eni1;
        ASSERT_SUCCESS(dash_api->set_eni_ether_address_map_entry_attribute(&entry, &attr));

        ASSERT_SUCCESS(dash_api->get_eni_ether_address_map_entry_attribute(&entry, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.oid == eni1);
    }

    ASSERT_SUCCESS(dash_api->remove_eni_ether_address_map_entry(&entry));
    ASSERT_SUCCESS(dash_api->remove_eni(eni0));
    ASSERT_SUCCESS(dash_api->remove_eni(eni1));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet0));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet1));
}

TEST_P(APIBulk, eni_ether_address_map_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 1;

    sai_dash_eni_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ENI, (void**)&dash_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_attribute_t attr;
    sai_object_id_t eni0, vnet0;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 100;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet0, switchid, 1, &attr));

    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet0;
    ASSERT_SUCCESS(dash_api->create_eni(&eni0, switchid, 1, &attr));

    sai_object_id_t eni1, vnet1;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 200;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet1, switchid, 1, &attr));

    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet1;
    ASSERT_SUCCESS(dash_api->create_eni(&eni1, switchid, 1, &attr));

    sai_attribute_t attrs0[entry_attrs_count] = {
        {.id = SAI_ENI_ETHER_ADDRESS_MAP_ENTRY_ATTR_ENI_ID, .value = (sai_attribute_value_t){.oid = eni0}},
    };

     sai_attribute_t attrs1[entry_attrs_count] = {
        {.id = SAI_ENI_ETHER_ADDRESS_MAP_ENTRY_ATTR_ENI_ID, .value = (sai_attribute_value_t){.oid = eni1}},
    };

    const sai_attribute_t *attr_list[entries_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[entries_count] = {entry_attrs_count, entry_attrs_count};
    sai_status_t statuses[entries_count] = {};

    sai_eni_ether_address_map_entry_t entries[entries_count] = {
        { .switch_id = switchid, .address = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 }},
        { .switch_id = switchid, .address = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x06 }},
    };

    ASSERT_SUCCESS(dash_api->create_eni_ether_address_map_entries(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < entries_count; i++) {
            attr.id = SAI_DIRECTION_LOOKUP_ENTRY_ATTR_ACTION;
            ASSERT_SUCCESS(dash_api->get_eni_ether_address_map_entry_attribute(&entries[i], 1, &attr));
            ASSERT_TRUE(attr.value.oid == attr_list[i][0].value.oid);
        }
    }

    ASSERT_SUCCESS(dash_api->remove_eni_ether_address_map_entries(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    ASSERT_SUCCESS(dash_api->remove_eni(eni0));
    ASSERT_SUCCESS(dash_api->remove_eni(eni1));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet0));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet1));
}

TEST_P(API, eni)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_eni_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ENI, (void**)&dash_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_attribute_t attr;
    sai_object_id_t vnet;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 101;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet, switchid, 1, &attr));

    vector<sai_attribute_t> attrs;

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

    attr.id = SAI_ENI_ATTR_FLOWS;
    attr.value.u32 = 30;
    attrs.push_back(attr);

    attr.id = SAI_ENI_ATTR_VM_UNDERLAY_DIP;
    attr.value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &attr.value.ipaddr.addr.ip4);
    attrs.push_back(attr);

    sai_object_id_t eni;
    ASSERT_SUCCESS(dash_api->create_eni(&eni, switchid, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        ASSERT_SUCCESS(dash_api->get_eni_attribute(eni, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.oid == vnet);
        ASSERT_TRUE(attrs[1].value.booldata);
        ASSERT_TRUE(attrs[2].value.u32 == 123);
        ASSERT_TRUE(attrs[3].value.u32 == 10);
        ASSERT_TRUE(attrs[4].value.u32 == 30);

        attr.id = SAI_ENI_ATTR_CPS;
        attr.value.u32 = 40;
        ASSERT_SUCCESS(dash_api->set_eni_attribute(eni, &attr));

        ASSERT_SUCCESS(dash_api->get_eni_attribute(eni, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.oid == vnet);
        ASSERT_TRUE(attrs[1].value.booldata);
        ASSERT_TRUE(attrs[2].value.u32 == 123);
        ASSERT_TRUE(attrs[3].value.u32 == 40);
        ASSERT_TRUE(attrs[4].value.u32 == 30);
    }

    ASSERT_SUCCESS(dash_api->remove_eni(eni));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet));
}

TEST_P(APIBulk, eni)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t enis_count = 2;
    const uint32_t eni_attrs_count = 6;

    sai_dash_eni_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ENI, (void**)&dash_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_attribute_t attr;
    sai_object_id_t vnet0, vnet1;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 101;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet0, switchid, 1, &attr));

    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 102;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet1, switchid, 1, &attr));

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
    sai_object_id_t enis[enis_count];
    sai_status_t statuses[enis_count] = {};

    ASSERT_SUCCESS(dash_api->create_enis(switchid, enis_count, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, enis, statuses));
    for (uint32_t i = 0; i < enis_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < enis_count; i++) {
            attr.id = SAI_ENI_ATTR_VNET_ID;
            ASSERT_SUCCESS(dash_api->get_eni_attribute(enis[i], 1, &attr));
            ASSERT_TRUE(attr.value.oid == attr_list[i][0].value.oid);
        }
    }

    ASSERT_SUCCESS(dash_api->remove_enis(enis_count, enis, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < enis_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet0));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet1));
}

TEST_P(API, vip_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_vip_api_t *dash_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VIP, (void**)&dash_api));

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_VIP_ENTRY_ATTR_ACTION;
    attr.value.s32 = SAI_VIP_ENTRY_ACTION_DENY;
    attrs.push_back(attr);

    sai_ip_address_t vip_addr;
    vip_addr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &vip_addr.addr.ip4);

    sai_vip_entry_t vip = { .switch_id = switchid, .vip = vip_addr };
    ASSERT_SUCCESS(dash_api->create_vip_entry(&vip, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        ASSERT_SUCCESS(dash_api->get_vip_entry_attribute(&vip, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.s32 == SAI_VIP_ENTRY_ACTION_DENY);

        attr.id = SAI_ENI_ETHER_ADDRESS_MAP_ENTRY_ATTR_ENI_ID;
        attr.value.s32 = SAI_VIP_ENTRY_ACTION_ACCEPT;
        ASSERT_SUCCESS(dash_api->set_vip_entry_attribute(&vip, &attr));

        ASSERT_SUCCESS(dash_api->get_vip_entry_attribute(&vip, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.s32 == SAI_VIP_ENTRY_ACTION_ACCEPT);
    }

    ASSERT_SUCCESS(dash_api->remove_vip_entry(&vip));
}

TEST_P(APIBulk, vip_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_vip_api_t *dash_api = nullptr;
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VIP, (void**)&dash_api));

    const uint32_t vips_count = 2;
    const uint32_t vip_attrs_count = 1;

    sai_attribute_t attrs0[] = {
        {.id = SAI_VIP_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_VIP_ENTRY_ACTION_DENY}},
    };

     sai_attribute_t attrs1[] = {
        {.id = SAI_VIP_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_VIP_ENTRY_ACTION_DENY}},
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
    vip_addr1.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.1.1", &vip_addr1.addr.ip4);

    sai_vip_entry_t vips[vips_count] = {
        {.switch_id = switchid, .vip = vip_addr0},
        {.switch_id = switchid, .vip = vip_addr1},
    };

    ASSERT_SUCCESS(dash_api->create_vip_entries(vips_count, vips, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < vips_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < vips_count; i++) {
            sai_attribute_t attr = {};
            attr.id = SAI_VIP_ENTRY_ATTR_ACTION;
            ASSERT_SUCCESS(dash_api->get_vip_entry_attribute(&vips[i], 1, &attr));
            ASSERT_TRUE(attr.value.s32 == attr_list[i][0].value.s32);
        }
    }

    ASSERT_SUCCESS(dash_api->remove_vip_entries(vips_count, vips, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < vips_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }
}

TEST_P(API, acl_group)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_acl_api_t *dash_acl_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ACL, (void**)&dash_acl_api));

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_DASH_ACL_GROUP_ATTR_IP_ADDR_FAMILY;
    attr.value.s32 = SAI_IP_ADDR_FAMILY_IPV4;
    attrs.push_back(attr);

    sai_object_id_t acl_group;
    ASSERT_SUCCESS(dash_acl_api->create_dash_acl_group(&acl_group, switchid, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        ASSERT_SUCCESS(dash_acl_api->get_dash_acl_group_attribute(acl_group, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.s32 == SAI_IP_ADDR_FAMILY_IPV4);

        attr.id = SAI_DASH_ACL_GROUP_ATTR_IP_ADDR_FAMILY;
        attr.value.s32 = SAI_IP_ADDR_FAMILY_IPV6;
        ASSERT_SUCCESS(dash_acl_api->set_dash_acl_group_attribute(acl_group, &attr));

        ASSERT_SUCCESS(dash_acl_api->get_dash_acl_group_attribute(acl_group, 1, &attr));
        ASSERT_TRUE(attr.value.s32 == SAI_IP_ADDR_FAMILY_IPV6);
    }

    ASSERT_SUCCESS(dash_acl_api->remove_dash_acl_group(acl_group));
}

TEST_P(APIBulk, acl_group)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t acls_count = 2;
    const uint32_t acl_attrs_count = 1;

    sai_dash_acl_api_t *dash_acl_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ACL, (void**)&dash_acl_api));

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

    ASSERT_SUCCESS(dash_acl_api->create_dash_acl_groups(switchid, acls_count, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, acls, statuses));
    for (uint32_t i = 0; i < acls_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < acls_count; i++) {
            sai_attribute_t attrs[acl_attrs_count] = {
                {.id = SAI_DASH_ACL_GROUP_ATTR_IP_ADDR_FAMILY, .value = {}},
            };
            ASSERT_SUCCESS(dash_acl_api->get_dash_acl_group_attribute(acls[i], acl_attrs_count, attrs));
            ASSERT_TRUE(attrs[0].value.s32 == attr_list[i][0].value.s32);
        }
    }

    ASSERT_SUCCESS(dash_acl_api->remove_dash_acl_groups(acls_count, acls, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < acls_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }
}

TEST_P(API, acl_rule)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_acl_api_t *dash_acl_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ACL, (void**)&dash_acl_api));

    sai_object_id_t group;
    ASSERT_SUCCESS(dash_acl_api->create_dash_acl_group(&group, switchid, 0, nullptr));

    sai_ip_prefix_t ip_addr_list[2] = {};

    ip_addr_list[0].addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &ip_addr_list[0].addr.ip4);
    inet_pton(AF_INET, "255.255.0.0", &ip_addr_list[0].mask.ip4);
    ip_addr_list[1].addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "100::ffff:ffff:ffff:ffff", &ip_addr_list[1].addr.ip6);
    inet_pton(AF_INET6, "ffff:fff0::", &ip_addr_list[1].mask.ip6);

    uint8_t protos[2] = {0xAA, 0xBB};
    sai_u16_range_t port_ranges[2] = {{.min = 10, .max = 20}, {.min = 30, .max = 40}};

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_DASH_ACL_RULE_ATTR_ACTION;
    attr.value.s32 = SAI_DASH_ACL_RULE_ACTION_PERMIT;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_DASH_ACL_GROUP_ID;
    attr.value.oid = group;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_DIP;
    attr.value.ipprefixlist.count = 1;
    attr.value.ipprefixlist.list = ip_addr_list;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_SIP;
    attr.value.ipprefixlist.count = 1;
    attr.value.ipprefixlist.list = ip_addr_list;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_PROTOCOL;
    attr.value.u8list.count = 2;
    attr.value.u8list.list = protos;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_SRC_PORT;
    attr.value.u16rangelist.count = 2;
    attr.value.u16rangelist.list = port_ranges;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_DST_PORT;
    attr.value.u16rangelist.count = 2;
    attr.value.u16rangelist.list = port_ranges;
    attrs.push_back(attr);

    attr.id = SAI_DASH_ACL_RULE_ATTR_PRIORITY;
    attr.value.u32 = 1;
    attrs.push_back(attr);

    sai_object_id_t acl;
    ASSERT_SUCCESS(dash_acl_api->create_dash_acl_rule(&acl, switchid, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        ASSERT_SUCCESS(dash_acl_api->get_dash_acl_rule_attribute(acl, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[5].value.u16rangelist.count == 2);
        ASSERT_TRUE(attrs[5].value.u16rangelist.list[0].min == port_ranges[0].min);
        ASSERT_TRUE(attrs[5].value.u16rangelist.list[0].max == port_ranges[0].max);
        ASSERT_TRUE(attrs[5].value.u16rangelist.list[1].min == port_ranges[1].min);
        ASSERT_TRUE(attrs[5].value.u16rangelist.list[1].max == port_ranges[1].max);

        attr.id = SAI_DASH_ACL_RULE_ATTR_ACTION;
        attr.value.s32 = SAI_DASH_ACL_RULE_ACTION_DENY_AND_CONTINUE;
        ASSERT_SUCCESS(dash_acl_api->set_dash_acl_rule_attribute(acl, &attr));

        ASSERT_SUCCESS(dash_acl_api->get_dash_acl_rule_attribute(acl, 1, &attr));
        ASSERT_TRUE(attr.value.s32 == SAI_DASH_ACL_RULE_ACTION_DENY_AND_CONTINUE);
    }

    ASSERT_SUCCESS(dash_acl_api->remove_dash_acl_rule(acl));

    ASSERT_SUCCESS(dash_acl_api->remove_dash_acl_group(group));
}

TEST_P(APIBulk, acl_rule)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t acls_count = 2;
    const uint32_t acl_attrs_count = 8;

    sai_dash_acl_api_t *dash_acl_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ACL, (void**)&dash_acl_api));

    sai_object_id_t group0;
    ASSERT_SUCCESS(dash_acl_api->create_dash_acl_group(&group0, switchid, 0, nullptr));

    sai_object_id_t group1;
    ASSERT_SUCCESS(dash_acl_api->create_dash_acl_group(&group1, switchid, 0, nullptr));

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
        {.id = SAI_DASH_ACL_RULE_ATTR_PRIORITY, .value = (sai_attribute_value_t){.u32 = 2}},
    };

    const sai_attribute_t *attr_list[acls_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[acls_count] = {acl_attrs_count, acl_attrs_count};
    sai_object_id_t acls[acls_count];
    sai_status_t statuses[acls_count] = {};

    ASSERT_SUCCESS(dash_acl_api->create_dash_acl_rules(switchid, acls_count, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, acls, statuses));
    for (uint32_t i = 0; i < acls_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < acls_count; i++) {
            sai_ip_prefix_t dip_ip_prefixes[2] = {};
            sai_ip_prefix_t sip_ip_prefixes[2] = {};
            sai_u16_range_t src_port_ranges[2] = {};
            sai_u16_range_t dst_port_ranges[2] = {};
            uint8_t protos[2] = {};
            sai_ip_prefix_list_t dip_ip_prefix_list = {.count = 2, .list = dip_ip_prefixes};
            sai_ip_prefix_list_t sip_ip_prefix_list = {.count = 2, .list = sip_ip_prefixes};
            sai_u8_list_t protos_list = {.count=2, .list = protos};
            sai_u16_range_list_t src_port_ranges_list = {.count=2, .list = src_port_ranges};
            sai_u16_range_list_t dst_port_ranges_list = {.count=2, .list = dst_port_ranges};

            sai_attribute_t attrs[acl_attrs_count] = {
                {.id = SAI_DASH_ACL_RULE_ATTR_ACTION, .value = {}},
                {.id = SAI_DASH_ACL_RULE_ATTR_DASH_ACL_GROUP_ID, .value = {}},
                {.id = SAI_DASH_ACL_RULE_ATTR_DIP, .value = (sai_attribute_value_t){.ipprefixlist = dip_ip_prefix_list}},
                {.id = SAI_DASH_ACL_RULE_ATTR_SIP, .value = (sai_attribute_value_t){.ipprefixlist = sip_ip_prefix_list}},
                {.id = SAI_DASH_ACL_RULE_ATTR_PROTOCOL, .value = (sai_attribute_value_t){.u8list = protos_list}},
                {.id = SAI_DASH_ACL_RULE_ATTR_SRC_PORT, .value = (sai_attribute_value_t){.u16rangelist = src_port_ranges_list}},
                {.id = SAI_DASH_ACL_RULE_ATTR_DST_PORT, .value = (sai_attribute_value_t){.u16rangelist = dst_port_ranges_list}},
                {.id = SAI_DASH_ACL_RULE_ATTR_PRIORITY, .value = {}},
            };

            ASSERT_SUCCESS(dash_acl_api->get_dash_acl_rule_attribute(acls[i], acl_attrs_count, attrs));

            ASSERT_TRUE(attrs[0].value.s32 == attr_list[i][0].value.s32);
            ASSERT_TRUE(attrs[1].value.oid == attr_list[i][1].value.oid);
            ASSERT_TRUE(compare_lists(attrs[2].value.ipprefixlist, attr_list[i][2].value.ipprefixlist));
            ASSERT_TRUE(compare_lists(attrs[3].value.ipprefixlist, attr_list[i][3].value.ipprefixlist));
            ASSERT_TRUE(compare_lists(attrs[4].value.u8list, attr_list[i][4].value.u8list));
            ASSERT_TRUE(compare_lists(attrs[5].value.u16rangelist, attr_list[i][5].value.u16rangelist));
            ASSERT_TRUE(compare_lists(attrs[6].value.u16rangelist, attr_list[i][6].value.u16rangelist));
            ASSERT_TRUE(attrs[7].value.u32 == attr_list[i][7].value.u32);
        }
    }

    ASSERT_SUCCESS(dash_acl_api->remove_dash_acl_rules(acls_count, acls, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < acls_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    ASSERT_SUCCESS(dash_acl_api->remove_dash_acl_group(group0));
    ASSERT_SUCCESS(dash_acl_api->remove_dash_acl_group(group1));
}

TEST_P(API, vnet)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_object_id_t vnet;
    sai_attribute_t attr;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 10;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet, switchid, 1, &attr));

    if (GetParam()) {
        ASSERT_SUCCESS(dash_vnet_api->get_vnet_attribute(vnet, 1, &attr));
        ASSERT_TRUE(attr.value.u32 == 10);

        attr.id = SAI_VNET_ATTR_VNI;
        attr.value.u32 = 20;
        ASSERT_SUCCESS(dash_vnet_api->set_vnet_attribute(vnet, &attr));

        ASSERT_SUCCESS(dash_vnet_api->get_vnet_attribute(vnet, 1, &attr));
        ASSERT_TRUE(attr.value.u32 == 20);
    }

    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet));
}

TEST_P(APIBulk, vnet)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t vnets_count = 2;
    const uint32_t vnet_attrs_count = 1;

    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

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

    ASSERT_SUCCESS(dash_vnet_api->create_vnets(switchid, vnets_count, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, vnets, statuses));
    for (uint32_t i = 0; i < vnets_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < vnets_count; i++) {
            sai_attribute_t attr = {};
            attr.id = SAI_VNET_ATTR_VNI;
            ASSERT_SUCCESS(dash_vnet_api->get_vnet_attribute(vnets[i], 1, &attr));
            ASSERT_TRUE(attr.value.u32 == attr_list[i][0].value.u32);
        }
    }

    ASSERT_SUCCESS(dash_vnet_api->remove_vnets(vnets_count, vnets, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < vnets_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }
}

TEST_P(API, inbound_routing_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_eni_api_t *dash_api_eni = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;
    sai_dash_inbound_routing_api_t *dash_in_routing_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ENI, (void**)&dash_api_eni));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_INBOUND_ROUTING, (void**)&dash_in_routing_api));

    sai_object_id_t vnet;
    sai_attribute_t attr;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 10;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet, switchid, 1, &attr));

    sai_object_id_t eni;
    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet;
    ASSERT_SUCCESS(dash_api_eni->create_eni(&eni, switchid, 1, &attr));

    sai_ip_address_t sip, sip_mask;
    sip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &sip.addr.ip4);
    sip_mask.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "255.255.255.0", &sip_mask.addr.ip4);

    sai_inbound_routing_entry_t entry = { .switch_id = switchid, .eni_id = eni, .vni = 10, .sip = sip, .sip_mask = sip_mask, .priority = 1};

    vector<sai_attribute_t> attrs;

    attr.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_ACTION;
    attr.value.s32 = SAI_INBOUND_ROUTING_ENTRY_ACTION_VXLAN_DECAP;
    attrs.push_back(attr);

    ASSERT_SUCCESS(dash_in_routing_api->create_inbound_routing_entry(&entry, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        attr.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_ACTION;
        attr.value.s32 = SAI_INBOUND_ROUTING_ENTRY_ACTION_VXLAN_DECAP_PA_VALIDATE;
        ASSERT_SUCCESS(dash_in_routing_api->set_inbound_routing_entry_attribute(&entry, &attr));

        ASSERT_SUCCESS(dash_in_routing_api->get_inbound_routing_entry_attribute(&entry, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.s32 == SAI_INBOUND_ROUTING_ENTRY_ACTION_VXLAN_DECAP_PA_VALIDATE);
    }

    ASSERT_SUCCESS(dash_in_routing_api->remove_inbound_routing_entry(&entry));

    ASSERT_SUCCESS(dash_api_eni->remove_eni(eni));
}

TEST_P(APIBulk, inbound_routing_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 1;

    sai_dash_eni_api_t *dash_api_eni = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;
    sai_dash_inbound_routing_api_t *dash_in_routing_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ENI, (void**)&dash_api_eni));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_INBOUND_ROUTING, (void**)&dash_in_routing_api));

    sai_object_id_t vnet0, vnet1;
    sai_attribute_t attr;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 10;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet0, switchid, 1, &attr));
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 100;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet1, switchid, 1, &attr));


    sai_object_id_t eni0, eni1;
    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet0;
    ASSERT_SUCCESS(dash_api_eni->create_eni(&eni0, switchid, 1, &attr));
    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet0;
    ASSERT_SUCCESS(dash_api_eni->create_eni(&eni1, switchid, 1, &attr));

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
    };

     sai_attribute_t attrs1[entry_attrs_count] = {
        {.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_INBOUND_ROUTING_ENTRY_ACTION_VXLAN_DECAP}},
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

    ASSERT_SUCCESS(dash_in_routing_api->create_inbound_routing_entries(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < entries_count; i++) {
            attr = {};
            attr.id = SAI_INBOUND_ROUTING_ENTRY_ATTR_ACTION;
            ASSERT_SUCCESS(dash_in_routing_api->get_inbound_routing_entry_attribute(&entries[i], 1, &attr));
            ASSERT_TRUE(attr.value.s32 == attr_list[i][0].value.s32);
        }
    }

    ASSERT_SUCCESS(dash_in_routing_api->remove_inbound_routing_entries(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    ASSERT_SUCCESS(dash_api_eni->remove_eni(eni0));
    ASSERT_SUCCESS(dash_api_eni->remove_eni(eni1));
}

TEST_P(API, pa_validation_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_pa_validation_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_PA_VALIDATION, (void**)&dash_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_attribute_t attr;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 10;

    sai_object_id_t vnet;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet, switchid, 1, &attr));

    sai_pa_validation_entry_t entry;
    entry.switch_id = switchid;
    entry.vnet_id = vnet;
    entry.sip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.3.3.3", &entry.sip.addr.ip4);

    vector<sai_attribute_t> attrs;

    attr.id = SAI_PA_VALIDATION_ENTRY_ATTR_ACTION;
    attr.value.s32 = SAI_PA_VALIDATION_ENTRY_ACTION_PERMIT;
    attrs.push_back(attr);

    ASSERT_SUCCESS(dash_api->create_pa_validation_entry(&entry, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        attr.id = SAI_PA_VALIDATION_ENTRY_ATTR_ACTION;
        attr.value.s32 = SAI_PA_VALIDATION_ENTRY_ACTION_DENY;
        ASSERT_SUCCESS(dash_api->set_pa_validation_entry_attribute(&entry, &attr));

        ASSERT_SUCCESS(dash_api->get_pa_validation_entry_attribute(&entry, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.s32 == SAI_PA_VALIDATION_ENTRY_ACTION_DENY);
    }

    ASSERT_SUCCESS(dash_api->remove_pa_validation_entry(&entry));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet));
}

TEST_P(APIBulk, pa_validation_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 1;

    sai_dash_pa_validation_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_PA_VALIDATION, (void**)&dash_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_attribute_t vnetattr;
    vnetattr.id = SAI_VNET_ATTR_VNI;

    sai_object_id_t vnet0, vnet1;
    vnetattr.value.u32 = 10;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet0, switchid, 1, &vnetattr));
    vnetattr.value.u32 = 20;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet1, switchid, 1, &vnetattr));

    sai_ip_address_t sip0 = {};
    sai_ip_address_t sip1 = {};
    sip0.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    sip1.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.1.1.1", &sip0.addr.ip4);
    inet_pton(AF_INET, "192.2.2.2", &sip1.addr.ip4);

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

    ASSERT_SUCCESS(dash_api->create_pa_validation_entries(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < entries_count; i++) {
            sai_attribute_t attr = {};
            attr.id = SAI_PA_VALIDATION_ENTRY_ATTR_ACTION;
            ASSERT_SUCCESS(dash_api->get_pa_validation_entry_attribute(&entries[i], 1, &attr));
            ASSERT_TRUE(attr.value.s32 == attr_list[i][0].value.s32);
        }
    }

    ASSERT_SUCCESS(dash_api->remove_pa_validation_entries(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet0));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet1));
}

TEST_P(API, outbound_routing_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_eni_api_t *dash_eni_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;
    sai_dash_outbound_routing_api_t *dash_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ENI, (void**)&dash_eni_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_OUTBOUND_ROUTING, (void**)&dash_api));

    sai_attribute_t attr;
    sai_object_id_t eni, vnet;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 101;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet, switchid, 1, &attr));

    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet;
    ASSERT_SUCCESS(dash_eni_api->create_eni(&eni, switchid, 1, &attr));

    sai_outbound_routing_entry_t entry0;
    entry0.switch_id = switchid;
    entry0.eni_id = eni;
    entry0.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.1.0", &entry0.destination.addr.ip4);
    inet_pton(AF_INET, "255.255.255.0", &entry0.destination.mask.ip4);

    vector<sai_attribute_t> attrs;
    attr.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_ACTION;
    attr.value.u32 = SAI_OUTBOUND_ROUTING_ENTRY_ACTION_ROUTE_VNET;
    attrs.push_back(attr);
    attr.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_DST_VNET_ID;
    attr.value.oid = vnet;
    attrs.push_back(attr);
    ASSERT_SUCCESS(dash_api->create_outbound_routing_entry(&entry0, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        ASSERT_SUCCESS(dash_api->get_outbound_routing_entry_attribute(&entry0, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.u32 == SAI_OUTBOUND_ROUTING_ENTRY_ACTION_ROUTE_VNET);

        attr.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_ACTION;
        attr.value.u32 = SAI_OUTBOUND_ROUTING_ENTRY_ACTION_ROUTE_DIRECT;
        ASSERT_SUCCESS(dash_api->set_outbound_routing_entry_attribute(&entry0, &attr));

        ASSERT_SUCCESS(dash_api->get_outbound_routing_entry_attribute(&entry0, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.u32 == SAI_OUTBOUND_ROUTING_ENTRY_ACTION_ROUTE_DIRECT);
    }

    ASSERT_SUCCESS(dash_api->remove_outbound_routing_entry(&entry0));

    ASSERT_SUCCESS(dash_eni_api->remove_eni(eni));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet));
}

TEST_P(APIBulk, outbound_routing_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 2;

    sai_dash_eni_api_t *dash_eni_api = nullptr;
    sai_dash_outbound_routing_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_ENI, (void**)&dash_eni_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_OUTBOUND_ROUTING, (void**)&dash_api));

    sai_attribute_t attr;
    sai_object_id_t vnet0, vnet1;
    sai_object_id_t eni0, eni1;
    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 101;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet0, switchid, 1, &attr));

    attr.id = SAI_VNET_ATTR_VNI;
    attr.value.u32 = 102;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet1, switchid, 1, &attr));

    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet0;
    ASSERT_SUCCESS(dash_eni_api->create_eni(&eni0, switchid, 1, &attr));

    attr.id = SAI_ENI_ATTR_VNET_ID;
    attr.value.oid = vnet1;
    ASSERT_SUCCESS(dash_eni_api->create_eni(&eni1, switchid, 1, &attr));

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
    };

     sai_attribute_t attrs1[entry_attrs_count] = {
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_ACTION, .value = (sai_attribute_value_t){.s32 = SAI_OUTBOUND_ROUTING_ENTRY_ACTION_ROUTE_VNET}},
        {.id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_DST_VNET_ID, .value = (sai_attribute_value_t){.oid = vnet1}},
    };

    const sai_attribute_t *attr_list[entries_count] = {
        attrs0,
        attrs1,
    };

    uint32_t attr_count[entries_count] = {entry_attrs_count, entry_attrs_count};
    sai_status_t statuses[entries_count] = {};

    sai_outbound_routing_entry_t entries[entries_count] = {
        { .switch_id = switchid, .eni_id = eni0, .destination = dst0},
        { .switch_id = switchid, .eni_id = eni1, .destination = dst1},
    };

    ASSERT_SUCCESS(dash_api->create_outbound_routing_entries(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < entries_count; i++) {
            sai_attribute_t gattrs[entry_attrs_count];
            gattrs[0].id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_ACTION;
            gattrs[1].id = SAI_OUTBOUND_ROUTING_ENTRY_ATTR_DST_VNET_ID;
            ASSERT_SUCCESS(dash_api->get_outbound_routing_entry_attribute(&entries[i], entry_attrs_count, gattrs));
            ASSERT_TRUE(gattrs[0].value.s32 == attr_list[i][0].value.s32);
            ASSERT_TRUE(gattrs[1].value.oid == attr_list[i][1].value.oid);
        }
    }

    ASSERT_SUCCESS(dash_api->remove_outbound_routing_entries(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    ASSERT_SUCCESS(dash_eni_api->remove_eni(eni0));
    ASSERT_SUCCESS(dash_eni_api->remove_eni(eni1));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet0));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet1));
}

TEST_P(API, outbound_ca_to_pa_entry_ipv4)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_outbound_ca_to_pa_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_OUTBOUND_CA_TO_PA, (void**)&dash_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_attribute_t vnetattr;
    vnetattr.id = SAI_VNET_ATTR_VNI;
    vnetattr.value.u32 = 10;

    sai_object_id_t vnet;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet, switchid, 1, &vnetattr));

    sai_outbound_ca_to_pa_entry_t entry;
    entry.switch_id = switchid;
    entry.dst_vnet_id = vnet;
    entry.dip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &entry.dip.addr.ip4);

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_UNDERLAY_DIP;
    attr.value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    inet_pton(AF_INET, "192.168.0.1", &attr.value.ipaddr.addr.ip4);
    attrs.push_back(attr);

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_OVERLAY_DMAC;
    sai_mac_t addr1 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
    memcpy(attr.value.mac, addr1, 6);
    attrs.push_back(attr);

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI;
    attr.value.booldata = false;
    attrs.push_back(attr);

    ASSERT_SUCCESS(dash_api->create_outbound_ca_to_pa_entry(&entry, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI;
        attr.value.booldata = true;
        attrs.clear();
        attrs.push_back(attr);
        ASSERT_SUCCESS(dash_api->set_outbound_ca_to_pa_entry_attribute(&entry, &attr));

        ASSERT_SUCCESS(dash_api->get_outbound_ca_to_pa_entry_attribute(&entry, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.booldata == true);
    }

    ASSERT_SUCCESS(dash_api->remove_outbound_ca_to_pa_entry(&entry));

    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet));
}

TEST_P(API, outbound_ca_to_pa_entry_ipv6)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    sai_dash_outbound_ca_to_pa_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_OUTBOUND_CA_TO_PA, (void**)&dash_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_attribute_t vnetattr;
    vnetattr.id = SAI_VNET_ATTR_VNI;
    vnetattr.value.u32 = 10;

    sai_object_id_t vnet;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet, switchid, 1, &vnetattr));

    sai_outbound_ca_to_pa_entry_t entry;
    entry.switch_id = switchid;
    entry.dst_vnet_id = vnet;
    entry.dip.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "fe80::886a:feff:fe31:bfe0", &entry.dip.addr.ip6);

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_UNDERLAY_DIP;
    attr.value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    inet_pton(AF_INET6, "fe80::886a:feff:fe31:bfe0", &attr.value.ipaddr.addr.ip6);
    attrs.push_back(attr);

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_OVERLAY_DMAC;
    sai_mac_t addr2 = { 0x00, 0x02, 0x03, 0x04, 0x05, 0x06 };
    memcpy(attr.value.mac, addr2, 6);
    attrs.push_back(attr);

    attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI;
    attr.value.booldata = true;
    attrs.push_back(attr);

    ASSERT_SUCCESS(dash_api->create_outbound_ca_to_pa_entry(&entry, (uint32_t)attrs.size(), attrs.data()));

    if (GetParam()) {
        attr.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI;
        attr.value.booldata = true;
        attrs.clear();
        attrs.push_back(attr);
        ASSERT_SUCCESS(dash_api->set_outbound_ca_to_pa_entry_attribute(&entry, &attr));

        ASSERT_SUCCESS(dash_api->get_outbound_ca_to_pa_entry_attribute(&entry, (uint32_t)attrs.size(), attrs.data()));
        ASSERT_TRUE(attrs[0].value.booldata == true);
    }

    ASSERT_SUCCESS(dash_api->remove_outbound_ca_to_pa_entry(&entry));

    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet));
}

TEST_P(APIBulk, outbound_ca_to_pa_entry)
{
    SWSS_LOG_ENTER();

    auto switchid = TestDashEnv::instance()->getSwitchOid();

    const uint32_t entries_count = 2;
    const uint32_t entry_attrs_count = 3;

    sai_dash_outbound_ca_to_pa_api_t *dash_api = nullptr;
    sai_dash_vnet_api_t *dash_vnet_api = nullptr;

    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_OUTBOUND_CA_TO_PA, (void**)&dash_api));
    ASSERT_SUCCESS(sai_api_query((sai_api_t)SAI_API_DASH_VNET, (void**)&dash_vnet_api));

    sai_attribute_t vnetattr;
    vnetattr.id = SAI_VNET_ATTR_VNI;

    sai_object_id_t vnet0, vnet1;
    vnetattr.value.u32 = 10;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet0, switchid, 1, &vnetattr));
    vnetattr.value.u32 = 20;
    ASSERT_SUCCESS(dash_vnet_api->create_vnet(&vnet1, switchid, 1, &vnetattr));

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
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI, .value = (sai_attribute_value_t){.booldata = true}}
    };

     sai_attribute_t attrs1[entry_attrs_count] = {
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_UNDERLAY_DIP, .value = (sai_attribute_value_t){.ipaddr = udip1}},
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_OVERLAY_DMAC, .value = (sai_attribute_value_t){.mac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x56}}},
        {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI, .value = (sai_attribute_value_t){.booldata = false}}
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

    ASSERT_SUCCESS(dash_api->create_outbound_ca_to_pa_entries(entries_count, entries, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    if (GetParam()) {
        for (uint32_t i = 0; i < entries_count; i++) {
            sai_attribute_t attrs[entry_attrs_count] = {
                {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_UNDERLAY_DIP, .value = {}},
                {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_OVERLAY_DMAC, .value = {}},
                {.id = SAI_OUTBOUND_CA_TO_PA_ENTRY_ATTR_USE_DST_VNET_VNI, .value = {}},
            };

            ASSERT_SUCCESS(dash_api->get_outbound_ca_to_pa_entry_attribute(&entries[i], entry_attrs_count, attrs));

            ASSERT_TRUE(attrs[0].value.ipaddr == attr_list[i][0].value.ipaddr);
            ASSERT_TRUE(memcmp(attrs[1].value.mac, attr_list[i][1].value.mac, sizeof(attrs[1].value.mac)) == 0);
            ASSERT_TRUE(attrs[2].value.booldata == attr_list[i][2].value.booldata);
        }
    }

    ASSERT_SUCCESS(dash_api->remove_outbound_ca_to_pa_entries(entries_count, entries, SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR, statuses));
    for (uint32_t i = 0; i < entries_count; i++) {
        ASSERT_SUCCESS(statuses[i]);
    }

    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet0));
    ASSERT_SUCCESS(dash_vnet_api->remove_vnet(vnet1));
}
