#include "sai_serialize.h"
#include "MetaTestSaiInterface.h"
#include "Meta.h"

#include "sairedis.h"
#include "sairediscommon.h"

#include <nlohmann/json.hpp>

#include <inttypes.h>
#include <arpa/inet.h>

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

using json = nlohmann::json;

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

    for (size_t i = 1; i < sai_metadata_enum_sai_object_type_t.valuescount; ++i)
    {
        mk.objecttype = (sai_object_type_t)sai_metadata_enum_sai_object_type_t.values[i];

        auto s = sai_serialize_object_meta_key(mk);

        sai_deserialize_object_meta_key(s, mk);
    }
}

TEST(SaiSerialize, sai_serialize_port_lane_latch_status_list)
{
    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    for (size_t idx = 0 ; idx < sai_metadata_attr_sorted_by_id_name_count; ++idx)
    {
        auto meta = sai_metadata_attr_sorted_by_id_name[idx];
        if(meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_PORT_LANE_LATCH_STATUS_LIST)
        {
            attr.id = meta->attrid;

            if (meta->isaclaction)
            {
                attr.value.aclaction.enable = true;
            }

            if (meta->isaclfield)
            {
                attr.value.aclfield.enable = true;
            }

            sai_port_lane_latch_status_t list[1];
            list[0].lane = 1;
            list[0].value.changed=true;
            list[0].value.current_status=true;

            attr.value.portlanelatchstatuslist.count=1;
            attr.value.portlanelatchstatuslist.list = list;

            auto s = sai_serialize_attr_value(*meta, attr, false);

            sai_deserialize_attr_value(s, *meta, attr, false);
        }
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
            case SAI_ATTR_VALUE_TYPE_PORT_FREQUENCY_OFFSET_PPM_LIST:
            case SAI_ATTR_VALUE_TYPE_PORT_SNR_LIST:
            case SAI_ATTR_VALUE_TYPE_ACL_CHAIN_LIST:
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

TEST(SaiSerialize, sai_serialize_counter_stat)
{
    EXPECT_EQ(sai_serialize_counter_stat(SAI_COUNTER_STAT_PACKETS), "SAI_COUNTER_STAT_PACKETS");
}

TEST(SaiSerialize, sai_serialize_macsec_sa_stat)
{
    EXPECT_EQ(sai_serialize_macsec_sa_stat(SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED),
            "SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED");
}

TEST(SaiSerialize, sai_serialize_macsec_flow_stat)
{
    EXPECT_EQ(sai_serialize_macsec_flow_stat(SAI_MACSEC_FLOW_STAT_OTHER_ERR),
            "SAI_MACSEC_FLOW_STAT_OTHER_ERR");
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

TEST(SaiSerialize, sai_serialize_port_host_tx_ready)
{
    EXPECT_EQ(sai_serialize_port_host_tx_ready(SAI_PORT_HOST_TX_READY_STATUS_READY), "SAI_PORT_HOST_TX_READY_STATUS_READY");
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

TEST(SaiSerialize, sai_serialize_nat_event_ntf)
{
    EXPECT_THROW(sai_serialize_nat_event_ntf(1, nullptr), std::runtime_error);
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

TEST(SaiSerialize, sai_serialize_redis_port_attr_id)
{
    for (const auto& attr :
            {SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM, SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG})
    {
        sai_redis_port_attr_t deserialized_attr;
        sai_deserialize_redis_port_attr_id(
                sai_serialize_redis_port_attr_id(attr), deserialized_attr);

        EXPECT_EQ(deserialized_attr, attr);
    }

    // Undefined enum.
    int index = 1000;
    std::string serialized_attr = sai_serialize_redis_port_attr_id(
            static_cast<sai_redis_port_attr_t>(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG + index));

    EXPECT_EQ(serialized_attr,
            std::to_string(SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG + index));

    sai_redis_port_attr_t deserialized_attr;
    sai_deserialize_redis_port_attr_id(serialized_attr, deserialized_attr);
    EXPECT_EQ(deserialized_attr, SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG + index);
}

TEST(SaiSerialize, sai_serialize_redis_link_event_damping_algorithm)
{
    for (const auto& algo : {SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_DISABLED,
                                SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED})
    {
        sai_redis_link_event_damping_algorithm_t deserialized_algo;
        sai_deserialize_redis_link_event_damping_algorithm(
                sai_serialize_redis_link_event_damping_algorithm(algo), deserialized_algo);

        EXPECT_EQ(deserialized_algo, algo);
    }

    // Undefined enum.
    int index = 1000;
    std::string serialized_attr = sai_serialize_redis_link_event_damping_algorithm(
            static_cast<sai_redis_link_event_damping_algorithm_t>(SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED + index));

    EXPECT_EQ(serialized_attr,
            std::to_string(SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED + index));

    sai_redis_link_event_damping_algorithm_t deserialized_attr;
    sai_deserialize_redis_link_event_damping_algorithm(serialized_attr, deserialized_attr);
    EXPECT_EQ(deserialized_attr, SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED + index);
}

TEST(SaiSerialize, sai_serialize_redis_link_event_damping_aied_config)
{
    SWSS_LOG_ENTER();

    sai_redis_link_event_damping_algo_aied_config_t config = {
      .max_suppress_time = 500,
      .suppress_threshold = 2500,
      .reuse_threshold = 1000,
      .decay_half_life = 100,
      .flap_penalty = 100};

    std::string expected = "{\"max_suppress_time\":\"500\",\"suppress_threshold\":\"2500\",\"reuse_threshold\":\"1000\",\"decay_half_life\":\"100\",\"flap_penalty\":\"100\"}";
    std::string serialized_config = sai_serialize_redis_link_event_damping_aied_config(config);

    EXPECT_EQ(json::parse(serialized_config), json::parse(expected));

    sai_redis_link_event_damping_algo_aied_config_t deserialized_config;
    sai_deserialize_redis_link_event_damping_aied_config(serialized_config, deserialized_config);

    EXPECT_EQ(deserialized_config.max_suppress_time, config.max_suppress_time);
    EXPECT_EQ(deserialized_config.suppress_threshold, config.suppress_threshold);
    EXPECT_EQ(deserialized_config.reuse_threshold, config.reuse_threshold);
    EXPECT_EQ(deserialized_config.decay_half_life, config.decay_half_life);
    EXPECT_EQ(deserialized_config.flap_penalty, config.flap_penalty);
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

// LEGACY TESTS

TEST(SaiSerialize, serialize_bool)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    // test bool

    attr.id = SAI_SWITCH_ATTR_ON_LINK_ROUTE_SUPPORTED;
    attr.value.booldata = true;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    EXPECT_EQ(sai_serialize_attr_value(*meta, attr), "true");

    attr.id = SAI_SWITCH_ATTR_ON_LINK_ROUTE_SUPPORTED;
    attr.value.booldata = false;

    EXPECT_EQ(sai_serialize_attr_value(*meta, attr), "false");

    // deserialize

    attr.id = SAI_SWITCH_ATTR_ON_LINK_ROUTE_SUPPORTED;

    sai_deserialize_attr_value("true", *meta, attr);

    EXPECT_EQ(true, attr.value.booldata);

    sai_deserialize_attr_value("false", *meta, attr);

    EXPECT_EQ(false, attr.value.booldata);

    EXPECT_THROW(sai_deserialize_attr_value("xx", *meta, attr), std::runtime_error);
}

TEST(SaiSerialize, serialize_chardata)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    memset(attr.value.chardata, 0, 32);

    attr.id = SAI_HOSTIF_ATTR_NAME;
    memcpy(attr.value.chardata, "foo", sizeof("foo"));

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HOSTIF, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "foo");

    attr.id = SAI_HOSTIF_ATTR_NAME;
    memcpy(attr.value.chardata, "f\\oo\x12", sizeof("f\\oo\x12"));

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HOSTIF, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "f\\\\oo\\x12");

    attr.id = SAI_HOSTIF_ATTR_NAME;
    memcpy(attr.value.chardata, "\x80\xff", sizeof("\x80\xff"));

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HOSTIF, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "\\x80\\xFF");

    // deserialize

    sai_deserialize_attr_value("f\\\\oo\\x12", *meta, attr);

    SWSS_LOG_NOTICE("des: %s", attr.value.chardata);

    EXPECT_EQ(0, strcmp(attr.value.chardata, "f\\oo\x12"));

    sai_deserialize_attr_value("foo", *meta, attr);

    EXPECT_EQ(0, strcmp(attr.value.chardata, "foo"));

    EXPECT_THROW(sai_deserialize_attr_value("\\x2g", *meta, attr), std::runtime_error);

    EXPECT_THROW(sai_deserialize_attr_value("\\x2", *meta, attr), std::runtime_error);

    EXPECT_THROW(sai_deserialize_attr_value("\\s45", *meta, attr), std::runtime_error);

    EXPECT_THROW(sai_deserialize_attr_value("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", *meta, attr), std::runtime_error);
}

TEST(SaiSerialize, serialize_uint64)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_NV_STORAGE_SIZE;
    attr.value.u64 = 42;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "42");

    attr.value.u64 = 0x87654321aabbccdd;

    attr.id = SAI_SWITCH_ATTR_NV_STORAGE_SIZE;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    char buf[32];
    sprintf(buf, "%" PRIu64, attr.value.u64);

    EXPECT_EQ(s, std::string(buf));

    // deserialize

    sai_deserialize_attr_value("12345", *meta, attr);

    EXPECT_EQ(12345, attr.value.u64);

    EXPECT_THROW(sai_deserialize_attr_value("22345235345345345435", *meta, attr), std::runtime_error);

    EXPECT_THROW(sai_deserialize_attr_value("2a", *meta, attr), std::runtime_error);
}

TEST(SaiSerialize, serialize_enum)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;
    attr.value.s32 = SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD");

    attr.value.s32 = -1;

    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "-1");

    attr.value.s32 = 100;

    attr.id = SAI_SWITCH_ATTR_SWITCHING_MODE;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "100");

    // deserialize

    sai_deserialize_attr_value("12345", *meta, attr);

    EXPECT_EQ(12345, attr.value.s32);

    sai_deserialize_attr_value("-1", *meta, attr);

    EXPECT_EQ(-1, attr.value.s32);

    sai_deserialize_attr_value("SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD", *meta, attr);

    EXPECT_EQ(SAI_SWITCH_SWITCHING_MODE_STORE_AND_FORWARD, attr.value.s32);

    EXPECT_THROW(sai_deserialize_attr_value("foo", *meta, attr), std::runtime_error);
}

TEST(SaiSerialize, serialize_mac)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
    memcpy(attr.value.mac, "\x01\x22\x33\xaa\xbb\xcc", 6);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "01:22:33:AA:BB:CC");

    // deserialize

    sai_deserialize_attr_value("ff:ee:dd:33:44:55", *meta, attr);

    EXPECT_EQ(0, memcmp("\xff\xee\xdd\x33\x44\x55", attr.value.mac, 6));

    EXPECT_THROW(sai_deserialize_attr_value("foo", *meta, attr), std::runtime_error);
}

TEST(SaiSerialize, serialize_ip_address)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    attr.value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    attr.value.ipaddr.addr.ip4 = htonl(0x0a000015);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_TUNNEL, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "10.0.0.21");

    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    attr.value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

    uint16_t ip6[] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0xaaaa, 0xbbbb };

    memcpy(attr.value.ipaddr.addr.ip6, ip6, 16);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_TUNNEL, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "1111:2222:3333:4444:5555:6666:aaaa:bbbb");

    uint16_t ip6a[] = { 0x0100, 0 ,0 ,0 ,0 ,0 ,0 ,0xff00 };

    memcpy(attr.value.ipaddr.addr.ip6, ip6a, 16);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_TUNNEL, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "1::ff");

    uint16_t ip6b[] = { 0, 0 ,0 ,0 ,0 ,0 ,0 ,0x100 };

    memcpy(attr.value.ipaddr.addr.ip6, ip6b, 16);

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_TUNNEL, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "::1");

    int k = 100;
    attr.value.ipaddr.addr_family = (sai_ip_addr_family_t)k;

    EXPECT_THROW(sai_serialize_attr_value(*meta, attr), std::runtime_error);

    // deserialize

    sai_deserialize_attr_value("10.0.0.23", *meta, attr);

    EXPECT_EQ(attr.value.ipaddr.addr.ip4, htonl(0x0a000017));
    EXPECT_EQ(attr.value.ipaddr.addr_family, SAI_IP_ADDR_FAMILY_IPV4);

    sai_deserialize_attr_value("1::ff", *meta, attr);

    EXPECT_EQ(0, memcmp(attr.value.ipaddr.addr.ip6, ip6a, 16));
    EXPECT_EQ(attr.value.ipaddr.addr_family, SAI_IP_ADDR_FAMILY_IPV6);

    EXPECT_THROW(sai_deserialize_attr_value("foo", *meta, attr), std::runtime_error);
}

TEST(SaiSerialize, serialize_uint32_list)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_PORT_ATTR_SUPPORTED_SPEED; //SAI_PORT_ATTR_SUPPORTED_HALF_DUPLEX_SPEED;

    uint32_t list[] = {1,2,3,4,5,6,7};

    attr.value.u32list.count = 7;
    attr.value.u32list.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "7:null");

    attr.value.u32list.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "7:1,2,3,4,5,6,7");

    attr.value.u32list.count = 0;
    attr.value.u32list.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "0:null");

    attr.value.u32list.count = 0;
    attr.value.u32list.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "0:null");

    memset(&attr, 0, sizeof(attr));

    sai_deserialize_attr_value("7:1,2,3,4,5,6,7", *meta, attr, false);

    EXPECT_EQ(attr.value.u32list.count, 7);
    EXPECT_EQ(attr.value.u32list.list[0], 1);
    EXPECT_EQ(attr.value.u32list.list[1], 2);
    EXPECT_EQ(attr.value.u32list.list[2], 3);
    EXPECT_EQ(attr.value.u32list.list[3], 4);
}

TEST(SaiSerialize, serialize_enum_list)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;

    int32_t list[] = {
             SAI_NATIVE_HASH_FIELD_SRC_IP,
             SAI_NATIVE_HASH_FIELD_DST_IP,
             SAI_NATIVE_HASH_FIELD_VLAN_ID,
             77
    };

    attr.value.s32list.count = 4;
    attr.value.s32list.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HASH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "4:null");

    attr.value.s32list.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HASH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    // or for enum: 4:SAI_NATIVE_HASH_FIELD[SRC_IP,DST_IP,VLAN_ID,77]

    EXPECT_EQ(s, "4:SAI_NATIVE_HASH_FIELD_SRC_IP,SAI_NATIVE_HASH_FIELD_DST_IP,SAI_NATIVE_HASH_FIELD_VLAN_ID,77");

    attr.value.s32list.count = 0;
    attr.value.s32list.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HASH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "0:null");

    attr.value.s32list.count = 0;
    attr.value.s32list.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_HASH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "0:null");
}

TEST(SaiSerialize, serialize_oid)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

    attr.value.oid = 0x1234567890abcdef;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "oid:0x1234567890abcdef");

    // deserialize

    sai_deserialize_attr_value("oid:0x1234567890abcdef", *meta, attr);

    EXPECT_EQ(0x1234567890abcdef, attr.value.oid);

    EXPECT_THROW(sai_deserialize_attr_value("foo", *meta, attr), std::runtime_error);
}

TEST(SaiSerialize, serialize_oid_list)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;

    sai_object_id_t list[] = {
        1,0x42, 0x77
    };

    attr.value.objlist.count = 3;
    attr.value.objlist.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "3:null");

    attr.value.objlist.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    // or: 4:[ROUTE:0x1,PORT:0x3,oid:0x77] if we have query function

    EXPECT_EQ(s, "3:oid:0x1,oid:0x42,oid:0x77");

    attr.value.objlist.count = 0;
    attr.value.objlist.list = list;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "0:null");

    attr.value.objlist.count = 0;
    attr.value.objlist.list = NULL;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "0:null");

    memset(&attr, 0, sizeof(attr));

    // deserialize

    sai_deserialize_attr_value("3:oid:0x1,oid:0x42,oid:0x77", *meta, attr, false);

    EXPECT_EQ(attr.value.objlist.count, 3);
    EXPECT_EQ(attr.value.objlist.list[0], 0x1);
    EXPECT_EQ(attr.value.objlist.list[1], 0x42);
    EXPECT_EQ(attr.value.objlist.list[2], 0x77);
}

TEST(SaiSerialize, serialize_acl_action)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT;

    attr.value.aclaction.enable = true;
    attr.value.aclaction.parameter.oid = (sai_object_id_t)2;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "oid:0x2");

    attr.value.aclaction.enable = false;
    attr.value.aclaction.parameter.oid = (sai_object_id_t)2;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "disabled");

    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION;

    attr.value.aclaction.enable = true;
    attr.value.aclaction.parameter.s32 = SAI_PACKET_ACTION_TRAP;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "SAI_PACKET_ACTION_TRAP");

    attr.value.aclaction.enable = true;
    attr.value.aclaction.parameter.s32 = 77;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    EXPECT_EQ(s, "77");
}

TEST(SaiSerialize, serialize_qos_map)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;

    sai_qos_map_t qm = {
        .key   = { .tc = 1, .dscp = 2, .dot1p = 3, .prio = 4, .pg = 5, .queue_index = 6, .color = SAI_PACKET_COLOR_RED, .mpls_exp = 0, .fc = 2 },
        .value = { .tc = 11, .dscp = 22, .dot1p = 33, .prio = 44, .pg = 55, .queue_index = 66, .color = SAI_PACKET_COLOR_GREEN, .mpls_exp = 0, .fc = 2 } };

    attr.value.qosmap.count = 1;
    attr.value.qosmap.list = &qm;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_QOS_MAP, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    std::string ret = "{\"count\":1,\"list\":[{\"key\":{\"color\":\"SAI_PACKET_COLOR_RED\",\"dot1p\":3,\"dscp\":2,\"fc\":2,\"mpls_exp\":0,\"pg\":5,\"prio\":4,\"qidx\":6,\"tc\":1},\"value\":{\"color\":\"SAI_PACKET_COLOR_GREEN\",\"dot1p\":33,\"dscp\":22,\"fc\":2,\"mpls_exp\":0,\"pg\":55,\"prio\":44,\"qidx\":66,\"tc\":11}}]}";

    EXPECT_EQ(s, ret);

    s = sai_serialize_attr_value(*meta, attr, true);

    std::string ret2 = "{\"count\":1,\"list\":null}";
    EXPECT_EQ(s, ret2);

    // deserialize

    memset(&attr, 0, sizeof(attr));

    sai_deserialize_attr_value(ret, *meta, attr);

    EXPECT_EQ(attr.value.qosmap.count, 1);

    auto &l = attr.value.qosmap.list[0];

    EXPECT_EQ(l.key.tc, 1);
    EXPECT_EQ(l.key.dscp, 2);
    EXPECT_EQ(l.key.dot1p, 3);
    EXPECT_EQ(l.key.prio, 4);
    EXPECT_EQ(l.key.pg, 5);
    EXPECT_EQ(l.key.queue_index, 6);
    EXPECT_EQ(l.key.color, SAI_PACKET_COLOR_RED);
    EXPECT_EQ(l.key.mpls_exp, 0);
    EXPECT_EQ(l.key.fc, 2);

    EXPECT_EQ(l.value.tc, 11);
    EXPECT_EQ(l.value.dscp, 22);
    EXPECT_EQ(l.value.dot1p, 33);
    EXPECT_EQ(l.value.prio, 44);
    EXPECT_EQ(l.value.pg, 55);
    EXPECT_EQ(l.value.queue_index, 66);
    EXPECT_EQ(l.value.color, SAI_PACKET_COLOR_GREEN);
    EXPECT_EQ(l.value.mpls_exp, 0);
    EXPECT_EQ(l.value.fc, 2);
}

TEST(SaiSerialize, serialize_map)
{
    sai_attribute_t attr;
    const sai_attr_metadata_t* meta;
    std::string s;

    attr.id = SAI_NEXT_HOP_GROUP_MAP_ATTR_MAP_TO_VALUE_LIST;

    sai_map_t map = { .key = 1, .value = 11 };

    attr.value.maplist.count = 1;
    attr.value.maplist.list = &map;

    meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MAP, attr.id);

    s = sai_serialize_attr_value(*meta, attr);

    std::string ret = "{\"count\":1,\"list\":[{\"key\":1,\"value\":11}]}";

    EXPECT_EQ(s, ret);

    s = sai_serialize_attr_value(*meta, attr, true);

    std::string ret2 = "{\"count\":1,\"list\":null}";
    EXPECT_EQ(s, ret2);

    // deserialize

    memset(&attr, 0, sizeof(attr));

    sai_deserialize_attr_value(ret, *meta, attr);

    EXPECT_EQ(attr.value.maplist.count, 1);

    auto &l = attr.value.maplist.list[0];
    EXPECT_EQ(l.key, 1);

    EXPECT_EQ(l.value, 11);
}

template<typename T>
static void deserialize_number(
        _In_ const std::string& s,
        _Out_ T& number,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    errno = 0;

    char *endptr = NULL;

    number = (T)strtoull(s.c_str(), &endptr, hex ? 16 : 10);

    if (errno != 0 || endptr != s.c_str() + s.length())
    {
        SWSS_LOG_THROW("invalid number %s", s.c_str());
    }
}

template <typename T>
static std::string serialize_number(
        _In_ const T& number,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    if (hex)
    {
        char buf[32];

        snprintf(buf, sizeof(buf), "0x%" PRIx64, (uint64_t)number);

        return buf;
    }

    return std::to_string(number);
}

TEST(SaiSerialize, serialize_number)
{
    SWSS_LOG_ENTER();

    int64_t sp =  0x12345678;
    int64_t sn = -0x12345678;
    int64_t u  =  0x12345678;

    auto ssp = serialize_number(sp);
    auto ssn = serialize_number(sn);
    auto su  = serialize_number(u);

    EXPECT_EQ(ssp, std::to_string(sp));
    EXPECT_EQ(ssn, std::to_string(sn));
    EXPECT_EQ(su,  std::to_string(u));

    auto shsp = serialize_number(sp, true);
    auto shsn = serialize_number(sn, true);
    auto shu  = serialize_number(u,  true);

    EXPECT_EQ(shsp, "0x12345678");
    EXPECT_EQ(shsn, "0xffffffffedcba988");
    EXPECT_EQ(shu,  "0x12345678");

    sp = 0;
    sn = 0;
    u  = 0;

    deserialize_number(ssp, sp);
    deserialize_number(ssn, sn);
    deserialize_number(su,  u);

    EXPECT_EQ(sp,  0x12345678);
    EXPECT_EQ(sn, -0x12345678);
    EXPECT_EQ(u,   0x12345678);

    deserialize_number(shsp, sp, true);
    deserialize_number(shsn, sn, true);
    deserialize_number(shu,  u,  true);

    EXPECT_EQ(sp,  0x12345678);
    EXPECT_EQ(sn, -0x12345678);
    EXPECT_EQ(u,   0x12345678);
}

TEST(SaiSerialize, serialize_stat_capability_list)
{
    SWSS_LOG_ENTER();

    extern const sai_enum_metadata_t sai_metadata_enum_sai_stats_mode_t;
    sai_stat_capability_list_t queue_stats_capability;
    sai_stat_capability_t stat_initializer;
    stat_initializer.stat_enum = 0;
    stat_initializer.stat_modes = 0;
    std::vector<sai_stat_capability_t> qstat_cap_list(2, stat_initializer);
    queue_stats_capability.count = 2;
    queue_stats_capability.list = qstat_cap_list.data();
    queue_stats_capability.list[0].stat_enum = SAI_QUEUE_STAT_WRED_ECN_MARKED_PACKETS;
    queue_stats_capability.list[0].stat_modes = SAI_STATS_MODE_READ;
    queue_stats_capability.list[1].stat_enum = SAI_QUEUE_STAT_PACKETS;
    queue_stats_capability.list[1].stat_modes = SAI_STATS_MODE_READ;

    std::string capab_count = sai_serialize_stats_capability_list(queue_stats_capability, &sai_metadata_enum_sai_stats_mode_t, true);
    std::string capab_str = sai_serialize_stats_capability_list(queue_stats_capability, &sai_metadata_enum_sai_stats_mode_t, false);

    std::string exp_count_str = "{\"count\":2,\"list\":null}";
    EXPECT_EQ(capab_count, exp_count_str);

    std::string exp_capab_str = "{\"count\":2,\"list\":[{\"stat_enum\":\"34\",\"stat_modes\":[\"SAI_STATS_MODE_READ\"]},{\"stat_enum\":\"0\",\"stat_modes\":[\"SAI_STATS_MODE_READ\"]}]}";
    EXPECT_EQ(capab_str, exp_capab_str);

    std::vector<std::string> vec_stat_enum;
    std::vector<std::string> vec_stat_modes;

    for (uint32_t it = 0; it < queue_stats_capability.count; it++)
    {
        vec_stat_enum.push_back(std::to_string(queue_stats_capability.list[it].stat_enum));
        vec_stat_modes.push_back(std::to_string(queue_stats_capability.list[it].stat_modes));
    }

    std::ostringstream join_stat_enum;
    std::copy(vec_stat_enum.begin(), vec_stat_enum.end(), std::ostream_iterator<std::string>(join_stat_enum, ","));
    auto strCapEnum = join_stat_enum.str();

    std::ostringstream join_stat_modes;
    std::copy(vec_stat_modes.begin(), vec_stat_modes.end(), std::ostream_iterator<std::string>(join_stat_modes, ","));
    auto strCapModes = join_stat_modes.str();

    sai_stat_capability_list_t stats_capability;
    std::vector<sai_stat_capability_t> stat_cap_list(queue_stats_capability.count, stat_initializer);
    stats_capability.count = queue_stats_capability.count;
    stats_capability.list = stat_cap_list.data();

    // deserialize
    EXPECT_THROW(sai_deserialize_stats_capability_list(NULL, strCapEnum, strCapModes), std::runtime_error);

    sai_deserialize_stats_capability_list(&stats_capability, strCapEnum, strCapModes);

    EXPECT_EQ(stats_capability.count, queue_stats_capability.count);
    EXPECT_EQ(stats_capability.list[0].stat_modes, SAI_STATS_MODE_READ);
    EXPECT_EQ(stats_capability.list[1].stat_modes, SAI_STATS_MODE_READ);
    int is_expected_enum = false;

    if ((stats_capability.list[0].stat_enum == SAI_QUEUE_STAT_WRED_ECN_MARKED_PACKETS)||(stats_capability.list[1].stat_enum == SAI_QUEUE_STAT_PACKETS))
    {
        is_expected_enum = true;
    }
    if ((stats_capability.list[1].stat_enum == SAI_QUEUE_STAT_WRED_ECN_MARKED_PACKETS)||(stats_capability.list[0].stat_enum == SAI_QUEUE_STAT_PACKETS))
    {
        is_expected_enum = true;
    }
    EXPECT_EQ(is_expected_enum, true);
}
