#include "sai_serialize.h"

#include "sairedis.h"
#include "sairediscommon.h"

#include <gtest/gtest.h>

#include <memory>

TEST(SaiSerialize, transfer_attributes)
{
    SWSS_LOG_ENTER();

    sai_attribute_t src;
    sai_attribute_t dst;

    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    EXPECT_EQ(SAI_STATUS_SUCCESS, transfer_attributes(SAI_OBJECT_TYPE_SWITCH, 1, &src, &dst, true));

    EXPECT_THROW(transfer_attributes(SAI_OBJECT_TYPE_NULL, 1, &src, &dst, true), std::runtime_error);

    src.id = 0;
    dst.id = 1;

    EXPECT_THROW(transfer_attributes(SAI_OBJECT_TYPE_SWITCH, 1, &src, &dst, true), std::runtime_error);

    for (size_t idx = 0 ; idx < sai_metadata_attr_sorted_by_id_name_count; ++idx)
    {
        auto meta = sai_metadata_attr_sorted_by_id_name[idx];

        src.id = meta->attrid;
        dst.id = meta->attrid;

        EXPECT_EQ(SAI_STATUS_SUCCESS, transfer_attributes(meta->objecttype, 1, &src, &dst, true));
    }
}

TEST(SaiSerialize, sai_serialize_object_meta_key)
{
    sai_object_meta_key_t mk;

    mk.objecttype = SAI_OBJECT_TYPE_NULL;

    EXPECT_THROW(sai_serialize_object_meta_key(mk), std::runtime_error);

    memset(&mk, 0, sizeof(mk));

    for (int32_t i = SAI_OBJECT_TYPE_NULL+1; i < SAI_OBJECT_TYPE_EXTENSIONS_MAX; i++)
    {
        mk.objecttype = (sai_object_type_t)i;

        auto s = sai_serialize_object_meta_key(mk);

        sai_deserialize_object_meta_key(s, mk);
    }
}

TEST(SaiSerialize, sai_serialize_attr_value)
{
    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    for (size_t idx = 0 ; idx < sai_metadata_attr_sorted_by_id_name_count; ++idx)
    {
        auto meta = sai_metadata_attr_sorted_by_id_name[idx];

        switch (meta->attrvaluetype)
        {
            // values that currently don't have serialization methods
            case SAI_ATTR_VALUE_TYPE_TIMESPEC:
            case SAI_ATTR_VALUE_TYPE_PORT_ERR_STATUS_LIST:
            case SAI_ATTR_VALUE_TYPE_PORT_EYE_VALUES_LIST:
            case SAI_ATTR_VALUE_TYPE_FABRIC_PORT_REACHABILITY:
            case SAI_ATTR_VALUE_TYPE_PRBS_RX_STATE:
            case SAI_ATTR_VALUE_TYPE_SEGMENT_LIST:
            case SAI_ATTR_VALUE_TYPE_TLV_LIST:
            case SAI_ATTR_VALUE_TYPE_MAP_LIST:
                continue;

            default:
                break;
        }

        attr.id = meta->attrid;

        if (meta->isaclaction)
        {
            attr.value.aclaction.enable = true;
        }

        if (meta->isaclfield)
        {
            attr.value.aclfield.enable = true;
        }

        auto s = sai_serialize_attr_value(*meta, attr, false);

        sai_deserialize_attr_value(s, *meta, attr, false);

        sai_deserialize_free_attribute_value(meta->attrvaluetype, attr);
    }
}

TEST(SaiSerialize, sai_deserialize_redis_communication_mode)
{
    sai_redis_communication_mode_t value;

    sai_deserialize_redis_communication_mode(REDIS_COMMUNICATION_MODE_REDIS_ASYNC_STRING, value);

    EXPECT_EQ(value, SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC);

    sai_deserialize_redis_communication_mode(REDIS_COMMUNICATION_MODE_REDIS_SYNC_STRING, value);

    EXPECT_EQ(value, SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC);

    sai_deserialize_redis_communication_mode(REDIS_COMMUNICATION_MODE_ZMQ_SYNC_STRING, value);

    EXPECT_EQ(value, SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC);
}

TEST(SaiSerialize, sai_deserialize_ingress_priority_group_attr)
{
    auto s = sai_serialize_ingress_priority_group_attr(SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE);

    EXPECT_EQ(s, "SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE");

    sai_ingress_priority_group_attr_t attr;

    sai_deserialize_ingress_priority_group_attr(s, attr);
}

//TEST(SaiSerialize, char_to_int)
//{
//    EXPECT_THROW(char_to_int('g'), std::runtime_error);
//
//    EXPECT_EQ(char_to_int('a'), 10);
//}

TEST(SaiSerialize, transfer_list)
{
    sai_attribute_t src;
    sai_attribute_t dst;

    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    src.id = SAI_PORT_ATTR_HW_LANE_LIST;
    dst.id = SAI_PORT_ATTR_HW_LANE_LIST;

    uint32_t list[2] = { 2, 1 };

    src.value.u32list.count = 2;
    src.value.u32list.list = list;

    dst.value.u32list.count = 2;
    dst.value.u32list.list = nullptr;

    EXPECT_EQ(SAI_STATUS_FAILURE, transfer_attributes(SAI_OBJECT_TYPE_PORT, 1, &src, &dst, false));

    src.value.u32list.count = 1;
    src.value.u32list.list = nullptr;

    dst.value.u32list.count = 1;
    dst.value.u32list.list = list;

    EXPECT_THROW(transfer_attributes(SAI_OBJECT_TYPE_PORT, 1, &src, &dst, false), std::runtime_error);

    src.value.u32list.count = 2;
    src.value.u32list.list = list;

    dst.value.u32list.count = 1;
    dst.value.u32list.list = list;

    EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW, transfer_attributes(SAI_OBJECT_TYPE_PORT, 1, &src, &dst, false));
}

TEST(SaiSerialize, sai_deserialize_ip_prefix)
{
    sai_ip_prefix_t p;

    memset(&p, 0, sizeof(p));

    p.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

    p.addr.ip6[0] = 0x11;

    p.mask.ip6[0] = 0xFF;
    p.mask.ip6[1] = 0xF0;

    auto s = sai_serialize_ip_prefix(p);

    EXPECT_EQ(s, "1100::/12");

    sai_deserialize_ip_prefix(s, p);

    EXPECT_THROW(sai_deserialize_ip_prefix("a/0/c", p), std::runtime_error);

    EXPECT_THROW(sai_deserialize_ip_prefix("12x/0", p), std::runtime_error);

    p.addr_family = SAI_IP_ADDR_FAMILY_IPV4;

    sai_deserialize_ip_prefix("127.0.0.1/8", p);
}

TEST(SaiSerialize, sai_serialize_ip_prefix)
{
    sai_ip_prefix_t p;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    p.addr_family = (sai_ip_addr_family_t)7;
#pragma GCC diagnostic pop

    EXPECT_THROW(sai_serialize_ip_prefix(p), std::runtime_error);
}

TEST(SaiSerialize, sai_deserialize_ip_address)
{
    sai_ip_address_t a;

    EXPECT_THROW(sai_deserialize_ip_address("123", a), std::runtime_error);
}

TEST(SaiSerialize, sai_deserialize_ipv4)
{
    sai_ip4_t a;

    EXPECT_THROW(sai_deserialize_ipv4("123", a), std::runtime_error);
}

TEST(SaiSerialize, sai_deserialize_ipv6)
{
    sai_ip6_t a;

    EXPECT_THROW(sai_deserialize_ipv6("123", a), std::runtime_error);
}

TEST(SaiSerialize, sai_deserialize_chardata)
{
    sai_attribute_t a;

    EXPECT_THROW(sai_deserialize_chardata(std::string("123456789012345678901234567890123"), a.value.chardata), std::runtime_error);

    EXPECT_THROW(sai_deserialize_chardata(std::string("abc\\"), a.value.chardata), std::runtime_error);

    EXPECT_THROW(sai_deserialize_chardata(std::string("abc\\x"), a.value.chardata), std::runtime_error);

    EXPECT_THROW(sai_deserialize_chardata(std::string("a\\\\bc\\x1"), a.value.chardata), std::runtime_error);

    EXPECT_THROW(sai_deserialize_chardata(std::string("a\\\\bc\\xzg"), a.value.chardata), std::runtime_error);

    sai_deserialize_chardata(std::string("a\\\\\\x22"), a.value.chardata);
}

TEST(SaiSerialize, sai_serialize_chardata)
{
    sai_attribute_t a;

    a.value.chardata[0] = 'a';
    a.value.chardata[1] = '\\';
    a.value.chardata[2] = 'b';
    a.value.chardata[3] = 7;
    a.value.chardata[4] = 0;

    auto s = sai_serialize_chardata(a.value.chardata);

    EXPECT_EQ(s, "a\\\\b\\x07");
}

TEST(SaiSerialize, sai_serialize_api)
{
    EXPECT_EQ(sai_serialize_api(SAI_API_VLAN), "SAI_API_VLAN");
}

TEST(SaiSerialize, sai_serialize_vlan_id)
{
    EXPECT_EQ(sai_serialize_vlan_id(123), "123");
}

TEST(SaiSerialize, sai_deserialize_vlan_id)
{
    sai_vlan_id_t vlan;

    sai_deserialize_vlan_id("123", vlan);
}

TEST(SaiSerialize, sai_serialize_port_stat)
{
    EXPECT_EQ(sai_serialize_port_stat(SAI_PORT_STAT_IF_IN_OCTETS),"SAI_PORT_STAT_IF_IN_OCTETS");
}

TEST(SaiSerialize, sai_serialize_switch_stat)
{
    EXPECT_EQ(sai_serialize_switch_stat(SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS),
            "SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS");
}

TEST(SaiSerialize, sai_serialize_port_pool_stat)
{
    EXPECT_EQ(sai_serialize_port_pool_stat(SAI_PORT_POOL_STAT_IF_OCTETS), "SAI_PORT_POOL_STAT_IF_OCTETS");
}

TEST(SaiSerialize, sai_serialize_queue_stat)
{
    EXPECT_EQ(sai_serialize_queue_stat(SAI_QUEUE_STAT_PACKETS), "SAI_QUEUE_STAT_PACKETS");
}

TEST(SaiSerialize, sai_serialize_router_interface_stat)
{
    EXPECT_EQ(sai_serialize_router_interface_stat(SAI_ROUTER_INTERFACE_STAT_IN_OCTETS),
            "SAI_ROUTER_INTERFACE_STAT_IN_OCTETS");
}

TEST(SaiSerialize, sai_serialize_ingress_priority_group_stat)
{
    EXPECT_EQ(sai_serialize_ingress_priority_group_stat(SAI_INGRESS_PRIORITY_GROUP_STAT_PACKETS),
            "SAI_INGRESS_PRIORITY_GROUP_STAT_PACKETS");
}

TEST(SaiSerialize, sai_serialize_buffer_pool_stat)
{
    EXPECT_EQ(sai_serialize_buffer_pool_stat(SAI_BUFFER_POOL_STAT_CURR_OCCUPANCY_BYTES),
            "SAI_BUFFER_POOL_STAT_CURR_OCCUPANCY_BYTES");
}

TEST(SaiSerialize, sai_serialize_tunnel_stat)
{
    EXPECT_EQ(sai_serialize_tunnel_stat(SAI_TUNNEL_STAT_IN_OCTETS), "SAI_TUNNEL_STAT_IN_OCTETS");
}

TEST(SaiSerialize, sai_serialize_queue_attr)
{
    EXPECT_EQ(sai_serialize_queue_attr(SAI_QUEUE_ATTR_TYPE), "SAI_QUEUE_ATTR_TYPE");
}

TEST(SaiSerialize, sai_serialize_macsec_sa_attr)
{
    EXPECT_EQ(sai_serialize_macsec_sa_attr(SAI_MACSEC_SA_ATTR_MACSEC_DIRECTION),
            "SAI_MACSEC_SA_ATTR_MACSEC_DIRECTION");
}

TEST(SaiSerialize, sai_serialize_ingress_drop_reason)
{
    EXPECT_EQ(sai_serialize_ingress_drop_reason(SAI_IN_DROP_REASON_L2_ANY), "SAI_IN_DROP_REASON_L2_ANY");
}

TEST(SaiSerialize, sai_serialize_egress_drop_reason)
{
    EXPECT_EQ(sai_serialize_egress_drop_reason(SAI_OUT_DROP_REASON_L2_ANY), "SAI_OUT_DROP_REASON_L2_ANY");
}

TEST(SaiSerialize, sai_serialize_switch_shutdown_request)
{
    EXPECT_EQ(sai_serialize_switch_shutdown_request(0x1), "{\"switch_id\":\"oid:0x1\"}");
}

TEST(SaiSerialize, sai_serialize_oid_list)
{
    sai_object_list_t list;

    list.count = 2;
    list.list = nullptr;

    EXPECT_EQ(sai_serialize_oid_list(list, true), "2");


    EXPECT_EQ(sai_serialize_oid_list(list, false), "2:null");
}

TEST(SaiSerialize, sai_serialize_hex_binary)
{
    EXPECT_EQ(sai_serialize_hex_binary(nullptr, 0), "");

    uint8_t buf[1];

    EXPECT_EQ(sai_serialize_hex_binary(buf, 0), "");
}

TEST(SaiSerialize, sai_serialize_system_port_config_list)
{
    sai_system_port_config_t pc;

    memset(&pc, 0, sizeof(pc));

    sai_system_port_config_list_t list;

    list.count = 1;
    list.list = &pc;

    sai_attr_metadata_t *meta = nullptr;

    sai_serialize_system_port_config_list(*meta, list, false);
}

TEST(SaiSerialize, sai_deserialize_system_port_config_list)
{
    sai_system_port_config_t pc;

    memset(&pc, 0, sizeof(pc));

    sai_system_port_config_list_t list;

    list.count = 1;
    list.list = &pc;

    sai_attr_metadata_t *meta = nullptr;

    auto s = sai_serialize_system_port_config_list(*meta, list, false);

    sai_deserialize_system_port_config_list(s, list, false);
}

TEST(SaiSerialize, sai_serialize_port_oper_status)
{
    EXPECT_EQ(sai_serialize_port_oper_status(SAI_PORT_OPER_STATUS_UP), "SAI_PORT_OPER_STATUS_UP");
}

TEST(SaiSerialize, sai_serialize_queue_deadlock_event)
{
    EXPECT_EQ(sai_serialize_queue_deadlock_event(SAI_QUEUE_PFC_DEADLOCK_EVENT_TYPE_DETECTED),
            "SAI_QUEUE_PFC_DEADLOCK_EVENT_TYPE_DETECTED");
}

TEST(SaiSerialize, sai_serialize_fdb_event_ntf)
{
    EXPECT_THROW(sai_serialize_fdb_event_ntf(1, nullptr), std::runtime_error);
}

TEST(SaiSerialize, sai_serialize_port_oper_status_ntf)
{
    sai_port_oper_status_notification_t ntf;

    memset(&ntf, 0, sizeof(ntf));

    sai_serialize_port_oper_status_ntf(1, &ntf);

    EXPECT_THROW(sai_serialize_port_oper_status_ntf(1, nullptr), std::runtime_error);
}

TEST(SaiSerialize, sai_serialize_queue_deadlock_ntf)
{
    sai_queue_deadlock_notification_data_t ntf;

    memset(&ntf, 0, sizeof(ntf));

    sai_serialize_queue_deadlock_ntf(1, &ntf);

    EXPECT_THROW(sai_serialize_queue_deadlock_ntf(1, nullptr), std::runtime_error);
}

TEST(SaiSerialize, sai_serialize)
{
    sai_redis_notify_syncd_t value = SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC;

    EXPECT_EQ(sai_serialize(value), SYNCD_INSPECT_ASIC);
}

TEST(SaiSerialize, sai_serialize_redis_communication_mode)
{
    EXPECT_EQ(sai_serialize_redis_communication_mode(SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC),
            REDIS_COMMUNICATION_MODE_REDIS_SYNC_STRING);
}

TEST(SaiSerialize, sai_deserialize_queue_attr)
{
    sai_queue_attr_t attr = SAI_QUEUE_ATTR_PORT;
    sai_deserialize_queue_attr("SAI_QUEUE_ATTR_TYPE", attr);

    EXPECT_EQ(attr, SAI_QUEUE_ATTR_TYPE);
}

TEST(SaiSerialize, sai_deserialize_macsec_sa_attr)
{
    sai_macsec_sa_attr_t attr = SAI_MACSEC_SA_ATTR_SC_ID;
    sai_deserialize_macsec_sa_attr("SAI_MACSEC_SA_ATTR_MACSEC_DIRECTION", attr);

    EXPECT_EQ(attr, SAI_MACSEC_SA_ATTR_MACSEC_DIRECTION);
}

TEST(SaiSerialize, sai_deserialize)
{
    sai_redis_notify_syncd_t value = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;

    sai_deserialize("SYNCD_INSPECT_ASIC", value);

    EXPECT_EQ(value, SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC);
}
