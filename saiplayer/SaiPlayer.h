#pragma once

#include "CommandLineOptions.h"

#include "meta/SaiInterface.h"
#include "meta/SaiAttributeList.h"
#include "syncd/ServiceMethodTable.h"
#include "syncd/SwitchNotifications.h"

#include <fstream>
#include <memory>
#include <map>

namespace saiplayer
{
    class SaiPlayer
    {
        private:

            SaiPlayer(const SaiPlayer&) = delete;
            SaiPlayer& operator=(const SaiPlayer&) = delete;

        public:

            SaiPlayer(
                    _In_ std::shared_ptr<sairedis::SaiInterface> sai,
                    _In_ std::shared_ptr<CommandLineOptions> cmd);

            virtual ~SaiPlayer();

        public:

            int run();

        private:

            int replay();

            void processBulk(
                    _In_ sai_common_api_t api,
                    _In_ const std::string &line);

            sai_status_t handle_bulk_route(
                    _In_ const std::vector<std::string> &object_ids,
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>> &attributes,
                    _In_ const std::vector<sai_status_t> &recorded_statuses);

            sai_status_t handle_bulk_generic(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string> &object_ids,
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>> &attributes,
                    _In_ const std::vector<sai_status_t> &recorded_statuses);

            sai_status_t handle_bulk_object(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &object_ids,
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>> &attributes,
                    _Out_ std::vector<sai_status_t> &statuses);

            sai_status_t handle_bulk_entry(
                    _In_ const std::vector<std::string> &object_ids,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>> &attributes,
                    _Out_ std::vector<sai_status_t> &statuses);

            std::vector<std::string> tokenize(
                    _In_ std::string input,
                    _In_ const std::string &delim);

            void performFdbFlush(
                    _In_ const std::string& request,
                    _In_ const std::string response);

            void performNotifySyncd(
                    _In_ const std::string& request,
                    _In_ const std::string& response);

            void performSleep(
                    _In_ const std::string& line);

            void handle_get_response(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t get_attr_count,
                    _In_ sai_attribute_t* get_attr_list,
                    _In_ const std::string& response,
                    _In_ sai_status_t status);

            sai_status_t handle_generic(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            void update_notifications_pointers(
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

            sai_status_t handle_route(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_neighbor(
                    _In_ const std::string& str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_fdb(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_inseg(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_direction_lookup(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_eni_ether_address_map(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_vip(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_inbound_routing(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_pa_validation(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_outbound_routing(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_outbound_ca_to_pa(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_outbound_port_map_port_range(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_global_trusted_vni(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t handle_dash_eni_trusted_vni(
                    _In_ const std::string &str_object_id,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            void match_redis_with_rec(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t get_attr_count,
                    _In_ sai_attribute_t* get_attr_list,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t* attr_list);

            void match_redis_with_rec(
                    _In_ sai_object_list_t get_objlist,
                    _In_ sai_object_list_t objlist);

            void match_redis_with_rec(
                    _In_ sai_object_id_t get_oid,
                    _In_ sai_object_id_t oid);

            void match_list_lengths(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t get_attr_count,
                    _In_ sai_attribute_t* get_attr_list,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t* attr_list);

            const std::vector<swss::FieldValueTuple> get_values(
                    _In_ const std::vector<std::string>& items);

            sai_object_type_t deserialize_object_type(
                    _In_ const std::string& s);

            void translate_local_to_redis(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_object_id_t translate_local_to_redis(
                    _In_ sai_object_id_t rid);

            void translate_local_to_redis(
                    _Inout_ sai_object_list_t& element);

            const char* profileGetValue(
                    _In_ sai_switch_profile_id_t profile_id,
                    _In_ const char* variable);

            int profileGetNextValue(
                    _In_ sai_switch_profile_id_t profile_id,
                    _Out_ const char** variable,
                    _Out_ const char** value);

            void loadProfileMap();

        private: // notification handlers

            void onFdbEvent(
                    _In_ uint32_t count,
                    _In_ const sai_fdb_event_notification_data_t *data);

            void onPortStateChange(
                    _In_ uint32_t count,
                    _In_ const sai_port_oper_status_notification_t *data);

            void onQueuePfcDeadlock(
                    _In_ uint32_t count,
                    _In_ const sai_queue_deadlock_notification_data_t *data);

            void onSwitchAsicSdkHealthEvent(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_switch_asic_sdk_health_severity_t severity,
                    _In_ sai_timespec_t timestamp,
                    _In_ sai_switch_asic_sdk_health_category_t category,
                    _In_ sai_switch_health_data_t data,
                    _In_ const sai_u8_list_t description);

            void onSwitchShutdownRequest(
                    _In_ sai_object_id_t switch_id)  __attribute__ ((noreturn));

            void onSwitchStateChange(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_switch_oper_status_t switch_oper_status);

            void onBfdSessionStateChange(
                    _In_ uint32_t count,
                    _In_ const sai_bfd_session_state_notification_t *data);

            void onIcmpEchoSessionStateChange(
                    _In_ uint32_t count,
                    _In_ const sai_icmp_echo_session_state_notification_t *data);

            void onHaSetEvent(
                    _In_ uint32_t count,
                    _In_ const sai_ha_set_event_data_t *data);

            void onHaScopeEvent(
                    _In_ uint32_t count,
                    _In_ const sai_ha_scope_event_data_t *data);

            void onFlowBulkGetSessionEvent(
                    _In_ sai_object_id_t flow_bulk_session_id,
                    _In_ uint32_t count,
                    _In_ const sai_flow_bulk_get_session_event_data_t *data);

            void onPortHostTxReady(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_id_t port_id,
                    _In_ sai_port_host_tx_ready_status_t host_tx_ready_status);

        private:

            std::shared_ptr<sairedis::SaiInterface> m_sai;

            std::shared_ptr<CommandLineOptions> m_commandLineOptions;

            std::ifstream m_infile;

            std::map<sai_object_id_t,sai_object_id_t> m_local_to_redis;
            std::map<sai_object_id_t,sai_object_id_t> m_redis_to_local;

            syncd::ServiceMethodTable m_smt;

            syncd::SwitchNotifications m_sn;

            sai_switch_notifications_t m_switchNotifications;

            sai_service_method_table_t m_test_services;

            std::map<std::string, std::string> m_profileMap;

            std::map<std::string, std::string>::iterator m_profileIter;
    };
}
