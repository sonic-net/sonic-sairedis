#pragma once

#include "SwitchState.h"
#include "FdbInfo.h"
#include "HostInterfaceInfo.h"
#include "WarmBootState.h"
#include "SwitchConfig.h"
#include "RealObjectIdManager.h"
#include "EventPayloadNetLinkMsg.h"
#include "MACsecManager.h"

#include <set>
#include <unordered_set>
#include <vector>

#define SAI_VS_FDB_INFO "SAI_VS_FDB_INFO"

#define DEFAULT_VLAN_NUMBER 1

#define MAX_OBJLIST_LEN 128

#define CHECK_STATUS(status) {                                  \
    sai_status_t _status = (status);                            \
    if (_status != SAI_STATUS_SUCCESS) { return _status; } }

namespace saivs
{
    class SwitchStateBase:
        public SwitchState
    {
        public:

            typedef std::map<sai_object_id_t, std::shared_ptr<SwitchStateBase>> SwitchStateMap;

        public:

            SwitchStateBase(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config);

            SwitchStateBase(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config,
                    std::shared_ptr<WarmBootState> warmBootState);

            virtual ~SwitchStateBase();

        protected:

            virtual sai_status_t set_switch_mac_address();

            virtual sai_status_t set_vxlan_default_router_mac();

            virtual sai_status_t set_switch_supported_object_types();

            virtual sai_status_t set_switch_default_attributes();

            virtual sai_status_t create_default_hash();

            virtual sai_status_t create_default_vlan();

            virtual sai_status_t create_cpu_port();

            virtual sai_status_t create_default_1q_bridge();

            virtual sai_status_t create_ports();

            virtual sai_status_t set_port_list();

            virtual sai_status_t set_port_capabilities();

            virtual sai_status_t create_fabric_ports();

            virtual sai_status_t set_fabric_port_list();

            virtual sai_status_t create_default_virtual_router();

            virtual sai_status_t create_default_stp_instance();

            virtual sai_status_t create_default_trap_group();

            virtual sai_status_t create_ingress_priority_groups_per_port(
                    _In_ sai_object_id_t port_id);

            virtual sai_status_t create_ingress_priority_groups();

            virtual sai_status_t create_vlan_members();

            virtual sai_status_t create_bridge_ports();

            virtual sai_status_t set_acl_entry_min_prio();

            virtual sai_status_t set_acl_capabilities();

            virtual sai_status_t set_maximum_number_of_childs_per_scheduler_group();

            virtual sai_status_t set_number_of_ecmp_groups();

            virtual sai_status_t set_static_crm_values();

            virtual sai_status_t set_static_acl_resource_list(
                    _In_ sai_switch_attr_t acl_resource,
                    _In_ int max_count);

            sai_status_t filter_available_lanes(
                    _Inout_ std::vector<std::vector<uint32_t>> &lanes_vector);

            sai_status_t create_system_ports(
                    _In_ int32_t voq_switch_id,
                    _In_ uint32_t sys_port_count,
                    _In_ const sai_system_port_config_t *sys_port_cfg_list);

            sai_status_t create_voqs();
            sai_status_t create_voq_per_sysport(
                    _In_ sai_object_id_t sys_port_id);
            sai_status_t set_system_port_list();

        public:

            virtual sai_status_t initialize_default_objects(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

        protected:

            virtual sai_status_t create_port_dependencies(
                    _In_ sai_object_id_t port_id);

            sai_status_t initialize_voq_switch_objects(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

        protected : // refresh

            virtual sai_status_t refresh_ingress_priority_group(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t port_id);

            virtual sai_status_t refresh_qos_queues(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t port_id);

            virtual sai_status_t refresh_scheduler_groups(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t port_id);

            virtual sai_status_t refresh_bridge_port_list(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t bridge_id);

            virtual sai_status_t refresh_vlan_member_list(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t vlan_id);

            virtual sai_status_t refresh_port_list(
                    _In_ const sai_attr_metadata_t *meta);

            virtual sai_status_t refresh_system_port_list(
                    _In_ const sai_attr_metadata_t *meta);

            virtual sai_status_t refresh_macsec_sci_in_ingress_macsec_acl(
                    _In_ sai_object_id_t object_id);

            virtual sai_status_t refresh_queue_pause_status(
                    _In_ sai_object_id_t object_id);

            virtual sai_status_t refresh_macsec_sa_stat(
                    _In_ sai_object_id_t object_id);

            virtual sai_status_t refresh_port_serdes_id(
                    _In_ sai_object_id_t bridge_id);

            virtual sai_status_t refresh_port_oper_speed(
                    _In_ sai_object_id_t port_id);

            virtual sai_status_t refresh_acl_table_entries(
                    _In_ sai_object_id_t acl_table_id);

            virtual sai_status_t refresh_acl_table_counters(
                    _In_ sai_object_id_t acl_table_id);
        public:

            virtual sai_status_t warm_boot_initialize_objects();

        protected:

            virtual sai_status_t refresh_read_only(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t object_id);

        protected:

            virtual sai_status_t warm_update_queues();

            virtual sai_status_t warm_update_scheduler_groups();

            virtual sai_status_t warm_update_ingress_priority_groups();

            virtual sai_status_t warm_update_switch();

            virtual sai_status_t warm_update_cpu_queues();

        protected: // TODO should be pure

            virtual sai_status_t create_cpu_qos_queues(
                    _In_ sai_object_id_t port_id);

            virtual sai_status_t create_qos_queues_per_port(
                    _In_ sai_object_id_t port_id);

            virtual sai_status_t create_qos_queues();

            virtual sai_status_t create_scheduler_group_tree(
                    _In_ const std::vector<sai_object_id_t>& sgs,
                    _In_ sai_object_id_t port_id);

            virtual sai_status_t create_scheduler_groups_per_port(
                    _In_ sai_object_id_t port_id);

            virtual sai_status_t create_scheduler_groups();

            virtual sai_status_t create_port_serdes();

            virtual sai_status_t create_port_serdes_per_port(
                    _In_ sai_object_id_t port_id);

        protected: // will generate new OID

            virtual sai_status_t create(
                    _In_ sai_object_type_t object_type,
                    _Out_ sai_object_id_t *object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            virtual sai_status_t remove(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id);

            virtual sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t* attr);

            virtual sai_status_t get(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t attr_count,
                    _Out_ sai_attribute_t *attr_list);

        public:

            virtual sai_status_t create(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            virtual sai_status_t remove(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId);

            virtual sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &serializedObjectId,
                    _In_ const sai_attribute_t* attr);

            virtual sai_status_t get(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId,
                    _In_ uint32_t attr_count,
                    _Out_ sai_attribute_t *attr_list);

            virtual sai_status_t bulkCreate(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            virtual sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            virtual sai_status_t bulkSet(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            virtual sai_status_t bulkGet(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const uint32_t *attr_count,
                    _Inout_ sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

           virtual sai_status_t queryAttrEnumValuesCapability(
                              _In_ sai_object_id_t switch_id,
                              _In_ sai_object_type_t object_type,
                              _In_ sai_attr_id_t attr_id,
                             _Inout_ sai_s32_list_t *enum_values_capability);

           virtual sai_status_t queryStatsCapability(
                              _In_ sai_object_id_t switchId,
                              _In_ sai_object_type_t objectType,
                              _Inout_ sai_stat_capability_list_t *stats_capability);

           virtual sai_status_t queryStatsStCapability(
                              _In_ sai_object_id_t switchId,
                              _In_ sai_object_type_t objectType,
                              _Inout_ sai_stat_st_capability_list_t *stats_capability);

           virtual sai_status_t queryAttributeCapability(
                              _In_ sai_object_id_t switch_id,
                              _In_ sai_object_type_t object_type,
                              _In_ sai_attr_id_t attr_id,
                             _Out_ sai_attr_capability_t *attr_capability);

        protected:

            virtual sai_status_t remove_internal(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId);

            virtual sai_status_t create_internal(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            virtual sai_status_t set_internal(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &serializedObjectId,
                    _In_ const sai_attribute_t* attr);

        private:

            sai_object_type_t objectTypeQuery(
                    _In_ sai_object_id_t objectId);

            sai_object_id_t switchIdQuery(
                    _In_ sai_object_id_t objectId);

        public:

            void processFdbEntriesForAging();

        private: // fdb related

            void updateLocalDB(
                    _In_ const sai_fdb_event_notification_data_t &data,
                    _In_ sai_fdb_event_t fdb_event);

            void processFdbInfo(
                    _In_ const FdbInfo &fi,
                    _In_ sai_fdb_event_t fdb_event);

            void findBridgeVlanForPortVlan(
                    _In_ sai_object_id_t port_id,
                    _In_ sai_vlan_id_t vlan_id,
                    _Inout_ sai_object_id_t &bv_id,
                    _Inout_ sai_object_id_t &bridge_port_id);

            bool getLagFromPort(
                    _In_ sai_object_id_t port_id,
                    _Inout_ sai_object_id_t& lag_id);

            bool isLagOrPortRifBased(
                    _In_ sai_object_id_t lag_or_port_id);

        public:

            void process_packet_for_fdb_event(
                    _In_ sai_object_id_t portId,
                    _In_ const std::string& name,
                    _In_ const uint8_t *buffer,
                    _In_ size_t size);

            void debugSetStats(
                    _In_ sai_object_id_t oid,
                    _In_ const std::map<sai_stat_id_t, uint64_t>& stats);

        protected: // custom port

            sai_status_t createPort(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removePort(
                    _In_ sai_object_id_t objectId);

            sai_status_t setPort(
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t* attr);

            bool isPortReadyToBeRemove(
                    _In_ sai_object_id_t portId);

            std::vector<sai_object_id_t> getPortDependencies(
                    _In_ sai_object_id_t portId);

            sai_status_t check_port_dependencies(
                    _In_ sai_object_id_t port_id,
                    _Out_ std::vector<sai_object_id_t>& dep);

            bool check_port_reference_count(
                    _In_ sai_object_id_t port_id);

            bool get_object_list(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_attr_id_t attr_id,
                    _Out_ std::vector<sai_object_id_t>& objlist);

            bool get_port_queues(
                    _In_ sai_object_id_t port_id,
                    _Out_ std::vector<sai_object_id_t>& queues);

            bool get_port_ipgs(
                    _In_ sai_object_id_t port_id,
                    _Out_ std::vector<sai_object_id_t>& ipgs);

            bool get_port_sg(
                    _In_ sai_object_id_t port_id,
                    _Out_ std::vector<sai_object_id_t>& sg);

            bool check_object_default_state(
                    _In_ sai_object_id_t object_id);

            bool check_object_list_default_state(
                    _Out_ const std::vector<sai_object_id_t>& objlist);

            sai_status_t createVoqSystemNeighborEntry(
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

        protected: // custom debug counter

            uint32_t getNewDebugCounterIndex();

            void releaseDebugCounterIndex(
                    _In_ uint32_t index);

            sai_status_t createDebugCounter(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeDebugCounter(
                    _In_ sai_object_id_t objectId);

        public:

            static int promisc(
                    _In_ const char *dev);

        protected: // custom hostif

            sai_status_t createHostif(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeHostif(
                    _In_ sai_object_id_t objectId);

            sai_status_t vs_remove_hostif_tap_interface(
                    _In_ sai_object_id_t hostif_id);

            sai_status_t vs_create_hostif_tap_interface(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            bool hostif_create_tap_veth_forwarding(
                    _In_ const std::string &tapname,
                    _In_ int tapfd,
                    _In_ sai_object_id_t port_id);

            static int vs_create_tap_device(
                    _In_ const char *dev,
                    _In_ int flags);

            static int vs_set_dev_mac_address(
                    _In_ const char *dev,
                    _In_ const sai_mac_t& mac);

            static int get_default_gw_mac_address(
                    _Out_ sai_mac_t& mac);

            int vs_set_dev_mtu(
                    _In_ const char*name,
                    _In_ int mtu);

            int ifup(
                    _In_ const char *dev,
                    _In_ sai_object_id_t port_id,
                    _In_ bool up,
                    _In_ bool explicitNotification);

            std::string vs_get_veth_name(
                    _In_ const std::string& tapname,
                    _In_ sai_object_id_t port_id);

            void send_port_oper_status_notification(
                    _In_ sai_object_id_t port_id,
                    _In_ sai_port_oper_status_t status,
                    _In_ bool force);

            bool hasIfIndex(
                    _In_ int ifIndex) const;

            bool vs_get_oper_speed(
                    _In_ sai_object_id_t port_id,
                    _Out_ uint32_t& speed);

        public: // TODO move inside warm boot load state

            sai_status_t vs_recreate_hostif_tap_interfaces();

            void update_port_oper_status(
                    _In_ sai_object_id_t port_id,
                    _In_ sai_port_oper_status_t port_oper_status);

            std::string dump_switch_database_for_warm_restart() const;

            void syncOnLinkMsg(
                    _In_ std::shared_ptr<EventPayloadNetLinkMsg> payload);

            void send_fdb_event_notification(
                    _In_ const sai_fdb_event_notification_data_t& data);

        public: // Telemetry and Monitor

            void send_tam_tel_type_config_change(
                _In_ sai_object_id_t tam_tel_type_id);

        protected:

            void findObjects(
                    _In_ sai_object_type_t object_type,
                    _In_ const sai_attribute_t &expect,
                    _Out_ std::vector<sai_object_id_t> &objects);

            bool dumpObject(
                    _In_ const sai_object_id_t object_id,
                    _Out_ std::vector<sai_attribute_t> &attrs);

        protected:

            sai_status_t setAclEntry(
                    _In_ sai_object_id_t entry_id,
                    _In_ const sai_attribute_t* attr);

            sai_status_t setAclEntryMACsecFlowActive(
                    _In_ sai_object_id_t entry_id,
                    _In_ const sai_attribute_t* attr);

            sai_status_t setMACsecSA(
                    _In_ sai_object_id_t macsec_sa_id,
                    _In_ const sai_attribute_t* attr);

            sai_status_t setTamTelType(
                _In_ sai_object_id_t tam_tel_type_id,
                _In_ const sai_attribute_t *attr);

            sai_status_t createMACsecPort(
                _In_ sai_object_id_t macsec_sa_id,
                _In_ sai_object_id_t switch_id,
                _In_ uint32_t attr_count,
                _In_ const sai_attribute_t *attr_list);

            sai_status_t createMACsecSA(
                    _In_ sai_object_id_t macsec_sa_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t createMACsecSC(
                    _In_ sai_object_id_t macsec_sa_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeMACsecPort(
                    _In_ sai_object_id_t macsec_port_id);

            sai_status_t removeMACsecSC(
                    _In_ sai_object_id_t macsec_sc_id);

            sai_status_t removeMACsecSA(
                    _In_ sai_object_id_t macsec_sa_id);

            sai_status_t getACLTable(
                    _In_ sai_object_id_t entry_id,
                    _Out_ sai_object_id_t &table_id);

            sai_status_t findPortByMACsecFlow(
                    _In_ sai_object_id_t macsec_flow_id,
                    _Out_ sai_object_id_t &line_port_id);

            std::shared_ptr<HostInterfaceInfo> findHostInterfaceInfoByPort(
                    _In_ sai_object_id_t line_port_id);

            sai_status_t loadMACsecAttrFromMACsecPort(
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _Out_ MACsecAttr &macsec_attr);

            sai_status_t loadMACsecAttrFromMACsecSC(
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _Out_ MACsecAttr &macsec_attr);

            sai_status_t loadMACsecAttrFromMACsecSA(
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _Out_ MACsecAttr &macsec_attr);

            sai_status_t loadMACsecAttr(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _Out_ MACsecAttr &macsec_attr);

            sai_status_t loadMACsecAttr(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _Out_ MACsecAttr &macsec_attr);

            sai_status_t loadMACsecAttrsFromACLEntry(
                    _In_ sai_object_id_t entry_id,
                    _In_ const sai_attribute_t* entry_attr,
                    _In_ sai_object_type_t object_type,
                    _Out_ std::vector<MACsecAttr> &macsec_attrs);

            sai_status_t getMACsecSAPacketNumber(
                    _In_ sai_object_id_t macsec_sa_id,
                    _Out_ sai_attribute_t &attr);

            void retryCreateIngressMaCsecSAs();

            MACsecManager m_macsecManager;

            std::unordered_map<sai_object_id_t, sai_object_id_t> m_macsecFlowPortMap;

            std::unordered_set<MACsecAttr, MACsecAttr::Hash> m_uncreatedIngressMACsecSAs;

        protected:

            constexpr static const int maxDebugCounters = 32;

            std::unordered_set<uint32_t> m_indices;

        protected:

            std::vector<sai_object_id_t> m_port_list;
            std::vector<sai_object_id_t> m_bridge_port_list_port_based;

            std::vector<sai_object_id_t> m_fabric_port_list;

            std::vector<sai_acl_action_type_t> m_ingress_acl_action_list;
            std::vector<sai_acl_action_type_t> m_egress_acl_action_list;

            sai_object_id_t m_cpu_port_id;
            sai_object_id_t m_default_1q_bridge;
            sai_object_id_t m_default_bridge_port_1q_router;
            sai_object_id_t m_default_vlan_id;

            sai_object_id_t m_ecmp_hash_id;
            sai_object_id_t m_lag_hash_id;

            std::vector<sai_object_id_t> m_system_port_list;

        protected:

            constexpr static const int m_maxIPv4RouteEntries = 100000;
            constexpr static const int m_maxIPv6RouteEntries = 10000;

            constexpr static const int m_maxIPv4NextHopEntries = 32000;
            constexpr static const int m_maxIPv6NextHopEntries = 32000;

            constexpr static const int m_maxIPv4NeighborEntries = 4000;
            constexpr static const int m_maxIPv6NeighborEntries = 2000;

            constexpr static const int m_maxNextHopGroupMemberEntries = 16000;
            constexpr static const int m_maxNextHopGroupEntries = 400;

            constexpr static const int m_maxFdbEntries = 800;

            constexpr static const int m_maxSNATEntries = 100;
            constexpr static const int m_maxDNATEntries = 100;
            constexpr static const int m_maxIPMCEntries = 100;
            constexpr static const int m_maxDoubleNATEntries = 50; /* Half of single NAT entry */

            constexpr static const int m_maxAclTables = 3;
            constexpr static const int m_maxAclTableGroups = 200;
            constexpr static const int m_maxAclTableEntries = 1000;
            constexpr static const int m_maxAclTableCounters = 1000;

        protected:

            virtual sai_status_t queryTunnelPeerModeCapability(
                                      _Inout_ sai_s32_list_t *enum_values_capability);

            virtual sai_status_t queryVlanfloodTypeCapability(
                                      _Inout_ sai_s32_list_t *enum_values_capability);

            virtual sai_status_t queryNextHopGroupTypeCapability(
                                      _Inout_ sai_s32_list_t *enum_values_capability);

            virtual sai_status_t queryHashNativeHashFieldListCapability(
                                      _Inout_ sai_s32_list_t *enum_values_capability);

            virtual sai_status_t querySwitchHashAlgorithmCapability(
                                      _Inout_ sai_s32_list_t *enum_values_capability);

            virtual sai_status_t queryPortAutonegFecOverrideSupportCapability(
                                      _Out_ sai_attr_capability_t *attr_capability);

        public: // TODO private

            std::set<FdbInfo> m_fdb_info_set;

            std::map<std::string, std::shared_ptr<HostInterfaceInfo>> m_hostif_info_map;

            std::shared_ptr<RealObjectIdManager> m_realObjectIdManager;
    };
}
