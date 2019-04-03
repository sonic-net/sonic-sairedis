#ifndef __SAI_SERIALIZE__
#define __SAI_SERIALIZE__

extern "C" {
#include "sai.h"
}

#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <iomanip>
#include <map>
#include <tuple>
#include <string.h>
#include "swss/logger.h"
#include "sai_meta.h"

// util

sai_status_t transfer_attributes(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *src_attr_list,
        _In_ sai_attribute_t *dst_attr_list,
        _In_ bool countOnly = false);

// serialize

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

std::string sai_serialize_vlan_id(
        _In_ const sai_vlan_id_t vlan_id);

std::string sai_serialize_object_type(
        _In_ const sai_object_type_t object_type);

std::string sai_serialize_object_id(
        _In_ const sai_object_id_t object_id);

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

std::string sai_serialize_tunnel_stat(
        _In_ const sai_tunnel_stat_t counter);

std::string sai_serialize_queue_attr(
        _In_ const sai_queue_attr_t attr);

std::string sai_serialize_switch_oper_status(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t status);

std::string sai_serialize_switch_shutdown_request(
        _In_ sai_object_id_t switch_id);

std::string sai_serialize_enum(
        _In_ const int32_t value,
        _In_ const sai_enum_metadata_t* meta);

std::string sai_serialize_number(
        _In_ uint32_t number,
        _In_ bool hex = false);

std::string sai_serialize_attr_id(
        _In_ const sai_attr_metadata_t& meta);

std::string sai_serialize_object_meta_key(
        _In_ const sai_object_meta_key_t& meta_key);

std::string sai_serialize_mac(
        _In_ const sai_mac_t mac);

std::string sai_serialize_port_oper_status(
        _In_ sai_port_oper_status_t status);

std::string sai_serialize_l2mc_entry_type(
        _In_ const sai_l2mc_entry_type_t type);

std::string sai_serialize_ipmc_entry_type(
        _In_ const sai_ipmc_entry_type_t type);

std::string sai_serialize_qos_map_item(
        _In_ const sai_qos_map_t& qosmap);

// serialize notifications

std::string sai_serialize_fdb_event_ntf(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t* fdb_event);

std::string sai_serialize_port_oper_status_ntf(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t* port_oper_status);

std::string sai_serialize_queue_deadlock_ntf(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t* deadlock_data);

// deserialize

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

void sai_deserialize_switch_shutdown_request(
        _In_ const std::string& s,
        _Out_ sai_object_id_t &switch_id);

void sai_deserialize_object_type(
        _In_ const std::string& s,
        _Out_ sai_object_type_t& object_type);

void sai_deserialize_object_id(
        _In_ const std::string& s,
        _Out_ sai_object_id_t& oid);

void sai_deserialize_fdb_entry(
        _In_ const std::string& s,
        _In_ sai_fdb_entry_t &fdb_entry);

void sai_deserialize_neighbor_entry(
        _In_ const std::string& s,
        _In_ sai_neighbor_entry_t &neighbor_entry);

void sai_deserialize_route_entry(
        _In_ const std::string& s,
        _In_ sai_route_entry_t &route_entry);

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

void sai_deserialize_vlan_id(
        _In_ const std::string& s,
        _In_ sai_vlan_id_t& vlan_id);

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

// deserialize notifications

void sai_deserialize_fdb_event_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_fdb_event_notification_data_t** fdbdata);

void sai_deserialize_port_oper_status_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_port_oper_status_notification_t** portoperstatus);

void sai_deserialize_queue_deadlock_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_queue_deadlock_notification_data_t** deadlock_data);

// free methods

void sai_deserialize_free_attribute_value(
        _In_ const sai_attr_value_type_t type,
        _In_ sai_attribute_t &attr);

// deserialize free notifications

void sai_deserialize_free_fdb_event_ntf(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t* fdbdata);

void sai_deserialize_free_port_oper_status_ntf(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t* portoperstatus);

void sai_deserialize_free_queue_deadlock_ntf(
        _In_ uint32_t count,
        _In_ sai_queue_deadlock_notification_data_t* deadlock_data);

void sai_deserialize_ingress_priority_group_attr(
        _In_ const std::string& s,
        _Out_ sai_ingress_priority_group_attr_t& attr);

void sai_deserialize_queue_attr(
        _In_ const std::string& s,
        _Out_ sai_queue_attr_t& attr);

#endif // __SAI_SERIALIZE__
