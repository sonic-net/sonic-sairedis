#pragma once

extern "C" {
#include "sai.h"
#include "saimetadata.h"
}

#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <iomanip>
#include <map>
#include <tuple>
#include <cstring>

#include "swss/logger.h"

#include "sairedis.h"

// util

sai_status_t transfer_attributes(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *src_attr_list,
        _In_ sai_attribute_t *dst_attr_list,
        _In_ bool countOnly = false);

// serialize

std::string sai_serialize_fdb_event(
        _In_ sai_fdb_event_t event);

std::string sai_serialize_nat_event(
        _In_ sai_nat_event_t event);

std::string sai_serialize_ipv6(
        _In_ const sai_ip6_t& ip);

std::string sai_serialize_ip_address(
        _In_ const sai_ip_address_t &ip_address);

std::string sai_serialize_ip_prefix(
        _In_ const sai_ip_prefix_t &ip_prefix);

std::string sai_serialize_neighbor_entry(
        _In_ const sai_neighbor_entry_t &neighbor_entry);

std::string sai_serialize_route_entry(
        _In_ const sai_route_entry_t &route_entry);

std::string sai_serialize_nat_entry(
        _In_ const sai_nat_entry_t &nat_entry);

std::string sai_serialize_inseg_entry(
        _In_ const sai_inseg_entry_t &inseg_entry);

std::string sai_serialize_l2mc_entry(
        _In_ const sai_l2mc_entry_t &l2mc_entry);

std::string sai_serialize_ipmc_entry(
        _In_ const sai_ipmc_entry_t &ipmc_entry);

std::string sai_serialize_mcast_fdb_entry(
        _In_ const sai_mcast_fdb_entry_t &mcast_fdb_entry);

std::string sai_serialize_fdb_entry(
        _In_ const sai_fdb_entry_t &fdb_entry);

std::string sai_serialize_meter_bucket_entry(
        _In_ const sai_meter_bucket_entry_t &meter_bucket_entry);

std::string sai_serialize_prefix_compression_entry(
        _In_ const sai_prefix_compression_entry_t &prefix_compression_entry);

std::string sai_serialize_flow_entry(
        _In_ const sai_flow_entry_t &flow_entry);

std::string sai_serialize_vlan_id(
        _In_ const sai_vlan_id_t vlan_id);

std::string sai_serialize_object_type(
        _In_ const sai_object_type_t object_type);

std::string sai_serialize_object_id(
        _In_ const sai_object_id_t object_id);

std::string sai_serialize_log_level(
        _In_ const sai_log_level_t log_level);

std::string sai_serialize_api(
        _In_ const sai_api_t api);

std::string sai_serialize_attr_value_type(
        _In_ const sai_attr_value_type_t attr_value_type);

std::string sai_serialize_attr_value(
        _In_ const sai_attr_metadata_t& meta,
        _In_ const sai_attribute_t &attr,
        _In_ const bool countOnly = false);

std::string sai_serialize_status(
        _In_ const sai_status_t status);

std::string sai_serialize_common_api(
        _In_ const sai_common_api_t common_api);

std::string sai_serialize_port_stat(
        _In_ const sai_port_stat_t counter);

std::string sai_serialize_switch_stat(
        _In_ const sai_switch_stat_t counter);

std::string sai_serialize_port_pool_stat(
        _In_ const sai_port_pool_stat_t counter);

std::string sai_serialize_queue_stat(
        _In_ const sai_queue_stat_t counter);

std::string sai_serialize_router_interface_stat(
        _In_ const sai_router_interface_stat_t counter);

std::string sai_serialize_ingress_priority_group_stat(
        _In_ const sai_ingress_priority_group_stat_t counter);

std::string sai_serialize_ingress_priority_group_attr(
        _In_ const sai_ingress_priority_group_attr_t attr);

std::string sai_serialize_buffer_pool_stat(
        _In_ const sai_buffer_pool_stat_t counter);

std::string sai_serialize_eni_stat(
        _In_ const sai_eni_stat_t counter);

std::string sai_serialize_meter_bucket_entry_stat(
        _In_ const sai_meter_bucket_entry_stat_t counter);

std::string sai_serialize_tunnel_stat(
        _In_ const sai_tunnel_stat_t counter);

std::string sai_serialize_counter_stat(
        _In_ const sai_counter_stat_t counter);

std::string sai_serialize_policer_stat(
        _In_ const sai_policer_stat_t counter);

std::string sai_serialize_queue_attr(
        _In_ const sai_queue_attr_t attr);

std::string sai_serialize_my_sid_entry(
        _In_ const sai_my_sid_entry_t &my_sid_entry);

std::string sai_serialize_hex_binary(
        _In_ const void *buffer,
        _In_ size_t length);

std::string sai_serialize_direction_lookup_entry(
        _In_ const sai_direction_lookup_entry_t &direction_lookup_entry);

std::string sai_serialize_eni_ether_address_map_entry(
        _In_ const sai_eni_ether_address_map_entry_t &eni_ether_address_map_entry);

std::string sai_serialize_vip_entry(
        _In_ const sai_vip_entry_t &vip_entry);

std::string sai_serialize_inbound_routing_entry(
        _In_ const sai_inbound_routing_entry_t &inbound_routing_entry);

std::string sai_serialize_pa_validation_entry(
        _In_ const sai_pa_validation_entry_t &pa_validation_entry);

std::string sai_serialize_outbound_routing_entry(
        _In_ const sai_outbound_routing_entry_t &outbound_routing_entry);

std::string sai_serialize_outbound_ca_to_pa_entry(
        _In_ const sai_outbound_ca_to_pa_entry_t &outbound_ca_to_pa_entry);

void sai_deserialize_system_port_config_list(
        _In_ const std::string& s,
        _Out_ sai_system_port_config_list_t& sysportconfiglist,
        _In_ bool countOnly);

std::string sai_serialize_chardata(
        _In_ const char data[32]);

std::string sai_serialize_oid_list(
        _In_ const sai_object_list_t &list,
        _In_ bool countOnly);

std::string sai_serialize_system_port_config_list(
        _In_ const sai_attr_metadata_t &meta,
        _In_ const sai_system_port_config_list_t& sysportconfiglist,
        _In_ bool countOnly);

std::string sai_serialize_queue_deadlock_event(
        _In_ sai_queue_pfc_deadlock_event_type_t event);

template <typename T>
std::string sai_serialize_hex_binary(
        _In_ const T &value)
{
    SWSS_LOG_ENTER();

    return sai_serialize_hex_binary(&value, sizeof(T));
}

std::string sai_serialize_macsec_flow_stat(
        _In_ const sai_macsec_flow_stat_t counter);

std::string sai_serialize_macsec_sa_stat(
        _In_ const sai_macsec_sa_stat_t counter);

std::string sai_serialize_macsec_sa_attr(
        _In_ const  sai_macsec_sa_attr_t &attr);

std::string sai_serialize_acl_counter_attr(
        _In_ const  sai_acl_counter_attr_t &attr);

std::string sai_serialize_switch_oper_status(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t status);

std::string sai_serialize_timespec(
        _In_ const sai_timespec_t &timespec);

std::string sai_serialize_switch_asic_sdk_health_event(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ const sai_timespec_t &timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ const sai_switch_health_data_t &data,
        _In_ const sai_u8_list_t &description);

std::string sai_serialize_switch_shutdown_request(
        _In_ sai_object_id_t switch_id);

std::string sai_serialize_enum(
        _In_ const int32_t value,
        _In_ const sai_enum_metadata_t* meta);

std::string sai_serialize_enum_list(
        _In_ const sai_s32_list_t& list,
        _In_ const sai_enum_metadata_t* meta,
        _In_ bool countOnly);

std::string sai_serialize_number(
        _In_ uint32_t number,
        _In_ bool hex = false);

template <typename T>
std::string sai_serialize_number_list(
        _In_ const T& list,
        _In_ bool countOnly,
        _In_ bool hex = false);

std::string sai_serialize_attr_id(
        _In_ const sai_attr_metadata_t& meta);

std::string sai_serialize_object_meta_key(
        _In_ const sai_object_meta_key_t& meta_key);

std::string sai_serialize_mac(
        _In_ const sai_mac_t mac);

std::string sai_serialize_port_oper_status(
        _In_ sai_port_oper_status_t status);

std::string sai_serialize_port_error_status(
        _In_ sai_port_error_status_t status);

std::string sai_serialize_port_host_tx_ready(
        _In_ sai_port_host_tx_ready_status_t host_tx_ready_status);

std::string sai_serialize_ingress_drop_reason(
        _In_ const sai_in_drop_reason_t reason);

std::string sai_serialize_egress_drop_reason(
        _In_ const sai_out_drop_reason_t reason);

std::string sai_serialize_l2mc_entry_type(
        _In_ const sai_l2mc_entry_type_t type);

std::string sai_serialize_ipmc_entry_type(
        _In_ const sai_ipmc_entry_type_t type);

std::string sai_serialize_nat_entry_type(
        _In_ const sai_nat_type_t type);

std::string sai_serialize_qos_map_item(
        _In_ const sai_qos_map_t& qosmap);

std::string sai_serialize_twamp_session_stat(
        _In_ const sai_twamp_session_stat_t counter);

std::string sai_serialize_poe_port_power_consumption(
        _In_ const sai_poe_port_power_consumption_t& pppc);

std::string sai_serialize_stats_capability_list(
        _In_ const sai_stat_capability_list_t& stat_capability_list,
        _In_ const sai_enum_metadata_t* meta,
        _In_ bool countOnly);

std::string sai_serialize_stats_st_capability_list(
        _In_ const sai_stat_st_capability_list_t &stat_capability_list,
        _In_ const sai_enum_metadata_t *meta,
        _In_ bool countOnly);

// serialize notifications

std::string sai_serialize_fdb_event_ntf(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t* fdb_event);

std::string sai_serialize_nat_event_ntf(
        _In_ uint32_t count,
        _In_ const sai_nat_event_notification_data_t* nat_event);

std::string sai_serialize_port_oper_status_ntf(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t* port_oper_status);

std::string sai_serialize_queue_deadlock_ntf(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t* deadlock_data);

std::string sai_serialize_bfd_session_state_ntf(
        _In_ uint32_t count,
        _In_ const sai_bfd_session_state_notification_t* bfd_session_state);

std::string sai_serialize_port_host_tx_ready_ntf(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_port_host_tx_ready_status_t host_tx_ready_status);

std::string sai_serialize_twamp_session_event_ntf(
        _In_ uint32_t count,
        _In_ const sai_twamp_session_event_notification_data_t* twamp_session_event);

// sairedis

std::string sai_serialize(
        _In_ const sai_redis_notify_syncd_t& value);

std::string sai_serialize_redis_communication_mode(
        _In_ sai_redis_communication_mode_t value);

std::string sai_serialize_redis_port_attr_id(
        _In_ const sai_redis_port_attr_t value);

// Link event damping.

std::string sai_serialize_redis_link_event_damping_algorithm(
        _In_ const sai_redis_link_event_damping_algorithm_t value);

std::string sai_serialize_redis_link_event_damping_aied_config(
         _In_ const sai_redis_link_event_damping_algo_aied_config_t& value);

// deserialize

void sai_deserialize_enum(
        _In_ const std::string& s,
        _In_ const sai_enum_metadata_t * meta,
        _Out_ int32_t& value);

void sai_deserialize_number(
        _In_ const std::string& s,
        _Out_ uint32_t& number,
        _In_ bool hex = false);

void sai_deserialize_status(
        _In_ const std::string& s,
        _Out_ sai_status_t& status);

void sai_deserialize_switch_oper_status(
        _In_ const std::string& s,
        _Out_ sai_object_id_t &switch_id,
        _Out_ sai_switch_oper_status_t& status);

void sai_deserialize_timespec(
        _In_ const std::string& s,
        _Out_ sai_timespec_t &timestamp);

void sai_deserialize_switch_asic_sdk_health_event(
        _In_ const std::string& s,
        _Out_ sai_object_id_t &switch_id,
        _Out_ sai_switch_asic_sdk_health_severity_t &severity,
        _Out_ sai_timespec_t &timestamp,
        _Out_ sai_switch_asic_sdk_health_category_t &category,
        _Out_ sai_switch_health_data_t &data,
        _Out_ sai_u8_list_t &description);

void sai_deserialize_switch_shutdown_request(
        _In_ const std::string& s,
        _Out_ sai_object_id_t &switch_id);

void sai_deserialize_object_type(
        _In_ const std::string& s,
        _Out_ sai_object_type_t& object_type);

void sai_deserialize_object_id(
        _In_ const std::string& s,
        _Out_ sai_object_id_t& oid);

void sai_deserialize_log_level(
        _In_ const std::string& s,
        _Out_ sai_log_level_t& log_level);

void sai_deserialize_api(
        _In_ const std::string& s,
        _Out_ sai_api_t& api);

void sai_deserialize_ipmc_entry_type(
        _In_ const std::string& s,
        _Out_ sai_ipmc_entry_type_t& type);

void sai_deserialize_l2mc_entry_type(
        _In_ const std::string& s,
        _Out_ sai_l2mc_entry_type_t& type);

void sai_deserialize_fdb_entry(
        _In_ const std::string& s,
        _In_ sai_fdb_entry_t &fdb_entry);

void sai_deserialize_neighbor_entry(
        _In_ const std::string& s,
        _In_ sai_neighbor_entry_t &neighbor_entry);

void sai_deserialize_route_entry(
        _In_ const std::string& s,
        _In_ sai_route_entry_t &route_entry);

void sai_deserialize_nat_entry_type(
        _In_ const std::string& s,
        _Out_ sai_nat_type_t& type);

void sai_deserialize_nat_entry(
        _In_ const std::string& s,
        _In_ sai_nat_entry_t &nat_entry);

void sai_deserialize_inseg_entry(
        _In_ const std::string& s,
        _In_ sai_inseg_entry_t &inseg_entry);

void sai_deserialize_l2mc_entry(
        _In_ const std::string& s,
        _In_ sai_l2mc_entry_t &l2mc_entry);

void sai_deserialize_ipmc_entry(
        _In_ const std::string& s,
        _In_ sai_ipmc_entry_t &ipmc_entry);

void sai_deserialize_mcast_fdb_entry(
        _In_ const std::string& s,
        _In_ sai_mcast_fdb_entry_t &mcast_fdb_entry);

void sai_deserialize_meter_bucket_entry(
        _In_ const std::string& s,
        _Out_ sai_meter_bucket_entry_t& meter_bucket_entry);

void sai_deserialize_prefix_compression_entry(
        _In_ const std::string& s,
        _Out_ sai_prefix_compression_entry_t& prefix_compression_entry);

void sai_deserialize_flow_entry(
        _In_ const std::string& s,
        _Out_ sai_flow_entry_t &flow_entry);

void sai_deserialize_vlan_id(
        _In_ const std::string& s,
        _In_ sai_vlan_id_t& vlan_id);

void sai_deserialize_direction_lookup_entry(
        _In_ const std::string &s,
        _Out_ sai_direction_lookup_entry_t& direction_lookup_entry);

void sai_deserialize_eni_ether_address_map_entry(
        _In_ const std::string &s,
        _Out_ sai_eni_ether_address_map_entry_t& eni_ether_address_map_entry);

void sai_deserialize_vip_entry(
        _In_ const std::string &s,
        _Out_ sai_vip_entry_t& vip_entry);

void sai_deserialize_inbound_routing_entry(
        _In_ const std::string &s,
        _Out_ sai_inbound_routing_entry_t& inbound_routing_entry);

void sai_deserialize_pa_validation_entry(
        _In_ const std::string &s,
        _Out_ sai_pa_validation_entry_t& pa_validation_entry);

void sai_deserialize_outbound_routing_entry(
        _In_ const std::string &s,
        _Out_ sai_outbound_routing_entry_t& outbound_routing_entry);

void sai_deserialize_outbound_ca_to_pa_entry(
        _In_ const std::string &s,
        _Out_ sai_outbound_ca_to_pa_entry_t& outbound_ca_to_pa_entry);

void sai_deserialize_attr_value(
        _In_ const std::string& s,
        _In_ const sai_attr_metadata_t& meta,
        _Out_ sai_attribute_t &attr,
        _In_ const bool countOnly = false);

void sai_deserialize_attr_id(
        _In_ const std::string& s,
        _Out_ sai_attr_id_t &attrid);

void sai_deserialize_attr_id(
        _In_ const std::string& s,
        _In_ const sai_attr_metadata_t** meta);

void sai_deserialize_object_meta_key(
        _In_ const std::string &s,
        _Out_ sai_object_meta_key_t& meta_key);

void sai_deserialize_ip_address(
        _In_ const std::string& s,
        _Out_ sai_ip_address_t& ipaddr);

void sai_deserialize_ip_prefix(
        _In_ const std::string &s,
        _Out_ sai_ip_prefix_t &ip_prefix);

void sai_deserialize_mac(
        _In_ const std::string& s,
        _Out_ sai_mac_t& mac);

void sai_deserialize_my_sid_entry(
        _In_ const std::string& s,
        _Out_ sai_my_sid_entry_t& my_sid_entry);

void sai_deserialize_ipv4(
        _In_ const std::string& s,
        _Out_ sai_ip4_t& ipaddr);

void sai_deserialize_ipv6(
        _In_ const std::string& s,
        _Out_ sai_ip6_t& ipaddr);

void sai_deserialize_chardata(
        _In_ const std::string& s,
        _Out_ char chardata[32]);

void sai_deserialize_poe_port_power_consumption(
        _In_ const std::string& s,
        _Out_ sai_poe_port_power_consumption_t& pppc);

// deserialize notifications

void sai_deserialize_fdb_event_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_fdb_event_notification_data_t** fdbdata);

void sai_deserialize_nat_event_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_nat_event_notification_data_t** natdata);

void sai_deserialize_port_oper_status_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_port_oper_status_notification_t** portoperstatus);

void sai_deserialize_queue_deadlock_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_queue_deadlock_notification_data_t** deadlock_data);

void sai_deserialize_bfd_session_state_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_bfd_session_state_notification_t** bfdsession);

void sai_deserialize_port_host_tx_ready_ntf(
        _In_ const std::string& s,
        _Out_ sai_object_id_t& switch_id,
        _Out_ sai_object_id_t& port_id,
        _Out_ sai_port_host_tx_ready_status_t& host_tx_ready_status);


void sai_deserialize_twamp_session_event_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_twamp_session_event_notification_data_t** twamp_session_data);

// free methods

void sai_deserialize_free_attribute_value(
        _In_ const sai_attr_value_type_t type,
        _In_ sai_attribute_t &attr);

// deserialize free notifications

void sai_deserialize_free_fdb_event_ntf(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t* fdbdata);

void sai_deserialize_free_nat_event_ntf(
        _In_ uint32_t count,
        _In_ sai_nat_event_notification_data_t* natdata);

void sai_deserialize_free_port_oper_status_ntf(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t* portoperstatus);

void sai_deserialize_free_queue_deadlock_ntf(
        _In_ uint32_t count,
        _In_ sai_queue_deadlock_notification_data_t* deadlock_data);

void sai_deserialize_free_bfd_session_state_ntf(
        _In_ uint32_t count,
        _In_ sai_bfd_session_state_notification_t* bfdsessionstate);

void sai_deserialize_free_switch_asic_sdk_health_event(
        _In_ sai_u8_list_t &description);

void sai_deserialize_ingress_priority_group_attr(
        _In_ const std::string& s,
        _Out_ sai_ingress_priority_group_attr_t& attr);

void sai_deserialize_free_twamp_session_event_ntf(
        _In_ uint32_t count,
        _In_ sai_twamp_session_event_notification_data_t* twamp_session_event);

void sai_deserialize_queue_attr(
        _In_ const std::string& s,
        _Out_ sai_queue_attr_t& attr);

void sai_deserialize_macsec_sa_attr(
        _In_ const std::string& s,
        _Out_ sai_macsec_sa_attr_t& attr);

void sai_deserialize_acl_counter_attr(
        _In_ const std::string& s,
        _Out_ sai_acl_counter_attr_t& attr);

// sairedis

void sai_deserialize(
        _In_ const std::string& s,
        _Out_ sai_redis_notify_syncd_t& value);

sai_redis_notify_syncd_t sai_deserialize_redis_notify_syncd(
        _In_ const std::string& s);

void sai_deserialize_redis_communication_mode(
        _In_ const std::string& s,
        _Out_ sai_redis_communication_mode_t& value);

void sai_deserialize_redis_port_attr_id(
        _In_ const std::string& s,
        _Out_ sai_redis_port_attr_t& value);

// Link event damping.

void sai_deserialize_redis_link_event_damping_algorithm(
        _In_ const std::string& s,
        _Out_ sai_redis_link_event_damping_algorithm_t& value);

void sai_deserialize_redis_link_event_damping_aied_config(
        _In_ const std::string& s,
         _Out_ sai_redis_link_event_damping_algo_aied_config_t& value);

void sai_deserialize_stats_capability_list(
        _Inout_ sai_stat_capability_list_t *stats_capability,
        _In_    const std::string& stat_enum_str,
        _In_    const std::string& stat_modes_str);

void sai_deserialize_stats_st_capability_list(
        _Inout_ sai_stat_st_capability_list_t *stats_capability,
        _In_ const std::string &stat_enum_str,
        _In_ const std::string &stat_modes_str,
        _In_ const std::string &minimal_polling_interval_str);
