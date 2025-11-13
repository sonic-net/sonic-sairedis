#pragma once

#include "SwitchStateBase.h"

#include "IpVrfInfo.h"
#include "SaiObjectDB.h"
#include "BitResourcePool.h"
#include "TunnelManager.h"
#include "SwitchVppNexthop.h"
#include "SwitchVppAcl.h"

#include "vppxlate/SaiVppXlate.h"

#include <list>

#define BFD_MUTEX std::lock_guard<std::mutex> lock(bfdMapMutex);

namespace saivs
{
    class SwitchVpp:
        public SwitchStateBase
    {
        public:

            // name hidding

            using saivs::SwitchStateBase::create;
            using saivs::SwitchStateBase::get;
            using saivs::SwitchStateBase::set;

        public:

            SwitchVpp(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config);

            SwitchVpp(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config,
                    _In_ std::shared_ptr<WarmBootState> warmBootState);

            virtual ~SwitchVpp() = default;

        protected:

            virtual sai_status_t create_cpu_qos_queues(
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t create_qos_queues_per_port(
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t create_qos_queues() override;

            virtual sai_status_t create_scheduler_group_tree(
                    _In_ const std::vector<sai_object_id_t>& sgs,
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t create_scheduler_groups_per_port(
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t set_maximum_number_of_childs_per_scheduler_group() override;

            virtual sai_status_t refresh_bridge_port_list(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t bridge_id) override;

            virtual sai_status_t warm_update_queues() override;

            virtual sai_status_t create_port_serdes() override;

            virtual sai_status_t create_port_serdes_per_port(
                    _In_ sai_object_id_t port_id) override;

        private: // from vpp VirtualSwitchSaiInterface

            void setPortStats(
                    _In_ sai_object_id_t oid);

            bool port_to_hostif_list(
                    _In_ sai_object_id_t oid,
                    _Inout_ std::string& if_name);

            bool port_to_hwifname(
                    _In_ sai_object_id_t oid,
                    _Inout_ std::string& if_name);

        public: // from VirtualSwitchSaiInterface changed functions

            virtual sai_status_t queryAttributeCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Out_ sai_attr_capability_t *capability) override;

            virtual uint64_t getObjectTypeAvailability(
                    _In_ sai_object_type_t object_type) override;

            virtual sai_status_t getStatsExt(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Out_ uint64_t *counters) override;

            virtual void processFdbEntriesForAging() override;

        public:

            virtual sai_status_t create(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t remove(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId) override;

            virtual sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &serializedObjectId,
                    _In_ const sai_attribute_t* attr) override;

            virtual sai_status_t get(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &serializedObjectId,
                    _In_ uint32_t attr_count,
                    _Out_ sai_attribute_t *attr_list) override;

        protected:

            virtual sai_status_t create_internal(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t remove_internal(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId) override;

            virtual sai_status_t set_internal(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &serializedObjectId,
                    _In_ const sai_attribute_t* attr) override;

            virtual sai_status_t createPort(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t setPort(
                    _In_ sai_object_id_t portId,
                    _In_ const sai_attribute_t* attr) override;

            virtual sai_status_t setAclEntry(
                    _In_ sai_object_id_t entry_id,
                    _In_ const sai_attribute_t* attr) override;

            virtual sai_status_t bulkCreate(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

        protected: // hostif

            static int vs_create_tap_device(
                    _In_ const char *dev,
                    _In_ int flags);

            static int vs_set_dev_mac_address(
                    _In_ const char *dev,
                    _In_ const sai_mac_t& mac);

            static int promisc(
                    _In_ const char *dev);

            static sai_status_t add_tc_filter_redirect(
                    _In_ const std::string& tap,
                    _In_ const std::string& hostIfname);

            virtual bool hostif_create_tap_veth_forwarding(
                    _In_ const std::string &tapname,
                    _In_ int tapfd,
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t vs_create_hostif_tap_interface(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t vs_remove_hostif_tap_interface(
                    _In_ sai_object_id_t hostif_id) override;

            virtual bool hasIfIndex(
                    _In_ int ifIndex) const override;

        private:

            // std::map<sai_object_id_t, std::string> phMap; // TODO to be removed

        public: // VPP (SwitchStateBase.h)

            sai_status_t vpp_dp_initialize();

            sai_status_t vpp_create_default_1q_bridge();

            sai_status_t createVlanMember(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t vpp_create_vlan_member(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeVlanMember(
                    _In_ sai_object_id_t objectId);

            sai_status_t vpp_remove_vlan_member(
                    _In_ sai_object_id_t vlan_member_oid);

            sai_status_t vpp_create_bvi_interface(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t vpp_delete_bvi_interface(
                    _In_ sai_object_id_t bvi_obj_id);
            sai_status_t createLag(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);
            sai_status_t vpp_create_lag(
                    _In_ sai_object_id_t lag_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);
            sai_status_t removeLag(
                    _In_ sai_object_id_t lag_oid);
            sai_status_t vpp_remove_lag(
                    _In_ sai_object_id_t lag_oid);
	    sai_status_t createLagMember(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);
	    sai_status_t vpp_create_lag_member(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);
	    sai_status_t removeLagMember(
                    _In_ sai_object_id_t lag_member_oid);
	    sai_status_t vpp_remove_lag_member(
                    _In_ sai_object_id_t lag_member_oid);

            /* FDB Entry and Flush SAI Objects */
            sai_status_t FdbEntryadd(
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t vpp_fdbentry_add(
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t FdbEntrydel(
                    _In_ const std::string &serializedObjectId);

            sai_status_t vpp_fdbentry_del(
                    _In_ const std::string &serializedObjectId);

            sai_status_t vpp_fdbentry_flush(
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            /* BFD Session */
            sai_status_t bfd_session_add(
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t vpp_bfd_session_add(
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t bfd_session_del(
                    _In_ const std::string &serializedObjectId);

            sai_status_t vpp_bfd_session_del(
                    _In_ const std::string &serializedObjectId);

        private: // VPP // BFD related

            typedef struct _vpp_bfd_info_t // TODO to separate file
            {
                //uint32_t sw_if_index;
                bool multihop; sai_ip_address_t local_addr;
                sai_ip_address_t peer_addr;

                // Define the < operator for comparison
                bool operator<(const _vpp_bfd_info_t& other) const
                {
                    // compare local IP address first
                    int cmp = std::memcmp(&local_addr, &other.local_addr, sizeof(sai_ip_address_t));

                    if (cmp != 0)
                    {
                        return cmp < 0;
                    }

                    // compare peer IP address
                    cmp = std::memcmp(&peer_addr, &other.peer_addr, sizeof(sai_ip_address_t));

                    if (cmp != 0)
                    {
                        return cmp < 0;
                    }

                    // compare multihop flag
                    return multihop < other.multihop;
                }
            } vpp_bfd_info_t;

            std::map<vpp_bfd_info_t, sai_object_id_t> m_bfd_info_map;
            std::mutex bfdMapMutex; // TODO fix naming

            void send_bfd_state_change_notification(
                    _In_ sai_object_id_t bfd_oid,
                    _In_ sai_bfd_session_state_t state,
                    _In_ bool force);

            void update_bfd_session_state(
                    _In_ sai_object_id_t bfd_oid,
                    _In_ sai_bfd_session_state_t state);

            sai_status_t asyncBfdStateUpdate(vpp_bfd_state_notif_t *bfd_notif);

        public: // VPP

            // TODO wiird function, max attr_count

            sai_status_t get_max(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &serializedObjectId,
                    _In_ const uint32_t max_attr_count,
                    _Out_ uint32_t *attr_count,
                    _Out_ sai_attribute_t *attr_list);

        public: // VPP

            std::shared_ptr<SaiDBObject> get_sai_object(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serialized_object_id);

            static int vpp_promisc(
                    _In_ const char *dev);

            static int vpp_create_tap_device(
                    _In_ const char *dev,
                    _In_ int flags);

        public: // VPP

            friend class TunnelManager;

        private: // VPP

            SaiObjectDB m_object_db;
            TunnelManager m_tunnel_mgr;

        private: // VPP

            std::map<sai_object_id_t, std::shared_ptr<IpVrfInfo>> vrf_objMap;
            bool nbr_env_read = false;
            bool nbr_active = false;
            std::map<std::string, std::string> m_intf_prefix_map;
            std::unordered_map<std::string, uint32_t> lpbInstMap;
            std::unordered_map<std::string, std::string> lpbIpToHostIfMap;
            std::unordered_map<std::string, std::string> lpbIpToIfMap;
            std::unordered_map<std::string, std::string> lpbHostIfToVppIfMap;

        protected: // VPP

            sai_status_t fillNHGrpMember(
                    nexthop_grp_member_t *nxt_grp_member,
                    sai_object_id_t next_hop_oid,
                    uint32_t next_hop_weight,
                    uint32_t next_hop_sequence);

            sai_status_t IpRouteNexthopGroupEntry(
                    _In_ sai_object_id_t next_hop_grp_oid,
                    _Out_ nexthop_grp_config_t **nxthop_group);

            sai_status_t IpRouteNexthopEntry(
                    _In_ sai_object_id_t next_hop_oid,
                    _Out_ nexthop_grp_config_t **nxthop_group_cfg);

            sai_status_t createNexthop(
                    _In_ const std::string& serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeNexthop(
                    _In_ const std::string &serializedObjectId);

            sai_status_t createNexthopGroupMember(
                    _In_ const std::string& serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeNexthopGroupMember(
                    _In_ const std::string& serializedObjectId);

        protected: // VPP

            sai_status_t createRouterif(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeRouterif(
                    _In_ sai_object_id_t objectId);

            sai_status_t vpp_create_router_interface(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t vpp_update_router_interface(
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t vpp_remove_router_interface(
                    _In_ sai_object_id_t objectId);

            sai_status_t vpp_router_interface_remove_vrf(
                    _In_ sai_object_id_t obj_id);

            sai_status_t vpp_add_del_intf_ip_addr (
                    _In_ sai_ip_prefix_t& ip_prefix,
                    _In_ sai_object_id_t nexthop_oid,
                    _In_ bool is_add);

            sai_status_t vpp_add_del_intf_ip_addr_norif (
                    _In_ const std::string& ip_prefix_key,
                    _In_ sai_route_entry_t& route_entry,
                    _In_ bool is_add);

            sai_status_t vpp_interface_ip_address_update (
                    _In_ const char *hw_ifname,
                    _In_ const std::string &serializedObjectId,
                    _In_ bool is_add);

            sai_status_t process_interface_loopback (
                    _In_ const std::string &serializedObjectId,
                    _In_ bool &isLoopback,
                    _In_ bool is_add);

            sai_status_t vpp_add_lpb_intf_ip_addr (
                    _In_ const std::string &serializedObjectId);

            sai_status_t vpp_del_lpb_intf_ip_addr (
                    _In_ const std::string &serializedObjectId);

            sai_status_t vpp_get_router_intf_name (
                    _In_ sai_ip_prefix_t& ip_prefix,
                    _In_ sai_object_id_t rif_id,
                    std::string& nexthop_ifname);

            int getNextLoopbackInstance();

            void markLoopbackInstanceDeleted(
                    _In_ int instance);

            bool vpp_intf_get_prefix_entry(
                    _In_ const std::string& intf_name,
                    _In_ std::string& ip_prefix);

            void vpp_intf_remove_prefix_entry(
                    _Inout_ const std::string& intf_name);

            sai_status_t asyncIntfStateUpdate(
                    _In_ const char *hwif_name,
                    _In_ bool link_up);

            sai_status_t vpp_set_interface_state (
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t vlan_id,
                    _In_ bool is_up);
            // set ethernet interface mtu including L2 header
            sai_status_t vpp_set_port_mtu (
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t vlan_id,
                    _In_ uint32_t mtu);
            // set sw interface mtu excluding L2 header
            sai_status_t vpp_set_interface_mtu (
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t vlan_id,
                    _In_ uint32_t mtu);

            sai_status_t UpdatePort(
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeVrf(
                    _In_ sai_object_id_t objectId);

            std::shared_ptr<IpVrfInfo> vpp_get_ip_vrf(
                    _In_ sai_object_id_t objectId);

            sai_status_t addRemoveIpNbr(
                    _In_ const std::string &serializedObjectId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ bool is_add);

            sai_status_t addIpNbr(
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeIpNbr(
                    _In_ const std::string &serializedObjectId);

            bool is_ip_nbr_active();

            sai_status_t addIpRoute(
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);
            sai_status_t removeIpRoute(
                    _In_ const std::string &serializedObjectId);

            sai_status_t IpRouteNexthopEntry(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    sai_ip_address_t *ip_address,
                    sai_object_id_t *next_rif_oid);

            std::string convertIPToString(
                    _In_ const sai_ip_addr_t &ipAddress);

            std::string convertIPv6ToString(
                    _In_ const sai_ip_addr_t &ipAddress,
                    _In_ int ipFamily);

            std::string extractDestinationIP(
                    _In_ const std::string &serializedObjectId);

            unsigned long ipToInteger(
                    _In_ const std::string &ipAddress);

            sai_status_t IpRouteAddRemove(
                    _In_ const SaiObject* route_obj,
                    _In_ bool is_add);

            sai_status_t updateIpRoute(
                    _In_ const std::string &serializedObjectId,
                    _In_ const sai_attribute_t *attr_list);

            int vpp_add_ip_vrf(
                    _In_ sai_object_id_t objectId,
                    _In_ uint32_t vrf_id);

            int vpp_del_ip_vrf(
                    _In_ sai_object_id_t objectId);

            int vpp_get_vrf_id(
                    _In_ const char *linux_ifname,
                    _Out_ uint32_t *vrf_id);

        private: // VPP

            typedef struct vpp_ace_cntr_info_ // TODO to separate file
            {
                sai_object_id_t tbl_oid;
                sai_object_id_t ace_oid;
                uint32_t acl_index;
                uint32_t ace_index;

            } vpp_ace_cntr_info_t;

            std::map<sai_object_id_t, std::list<sai_object_id_t>> m_acl_tbl_rules_map;
            std::map<sai_object_id_t, std::list<std::string>> m_acl_tbl_hw_ports_map;
            std::map<sai_object_id_t, uint32_t> m_acl_swindex_map;
            std::map<sai_object_id_t, uint32_t> m_tunterm_acl_swindex_map;
            std::map<sai_object_id_t, std::list<sai_object_id_t>> m_acl_tbl_grp_mbr_map;
            std::map<sai_object_id_t, std::list<sai_object_id_t>> m_acl_tbl_grp_ports_map;
            std::map<sai_object_id_t, vpp_ace_cntr_info_t> m_ace_cntr_info_map;

        protected: // VPP

            sai_status_t createAclEntry(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeAclEntry(
                    _In_ const std::string &serializedObjectId);

            sai_status_t addRemoveAclEntrytoMap(
                    _In_ sai_object_id_t entry_id,
                    _In_ sai_object_id_t tbl_oid,
                    _In_ bool is_add);

            sai_status_t getAclTableId(
                    _In_ sai_object_id_t entry_id, sai_object_id_t *tbl_oid);

            sai_status_t AclTblConfig(
                    _In_ sai_object_id_t tbl_oid);

            sai_status_t AclTblRemove(
                    _In_ sai_object_id_t tbl_oid);

            sai_status_t AclAddRemoveCheck(
                    _In_ sai_object_id_t tbl_oid);

            sai_status_t aclTableRemove(
                    _In_ const std::string &serializedObjectId);

            /**
             * @brief Retrieves ACEs and list of ordered ACE info for a given table.
             *
             * @param[in] tbl_oid The object ID of the table for which to retrieve ACEs.
             * @param[out] n_total_entries The total number of ACEs.
             * @param[out] aces Pointer to the array of ACEs.
             * @param[out] ordered_aces Ordered ACE info list.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t get_sorted_aces(
                    _In_ sai_object_id_t tbl_oid,
                    _Out_ size_t &n_total_entries,
                    _Out_ acl_tbl_entries_t *&aces,
                    _Out_ std::list<ordered_ace_list_t> &ordered_aces);

            /**
             * @brief Cleans up AclTblConfig variables.
             *
             * @param[in] aces Pointer to the ACEs to be cleaned up.
             * @param[in] ordered_aces Ordered ACE list to be cleaned up.
             * @param[in] acl Pointer to the VS ACL to be cleaned up.
             * @param[in] tunterm_acl Pointer to the VS tunnel termination ACL to be cleaned up.
             */
            void cleanup_acl_tbl_config(
                    _In_ acl_tbl_entries_t *&aces,
                    _In_ std::list<ordered_ace_list_t> &ordered_aces,
                    _In_ vpp_acl_t *&acl,
                    _In_ vpp_tunterm_acl_t *&tunterm_acl);

            /**
             * @brief Fills ACL and tunnel termination ACL rules based on the provided ACEs.
             *
             * @param[in] aces Pointer to ACEs.
             * @param[in] ordered_aces Reference to Ordered ACE list.
             * @param[in] acl Pointer to the allocated VS ACL.
             * @param[in] tunterm_acl Pointer to the allocated VS tunnel termination ACL.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t fill_acl_rules(
                    _In_ acl_tbl_entries_t *aces,
                    _In_ std::list<ordered_ace_list_t> &ordered_aces,
                    _In_ vpp_acl_t *acl,
                    _In_ vpp_tunterm_acl_t *tunterm_acl);

            /**
             * @brief Creates or replaces the provided ACL in VS.
             *
             * @param[in] acl Pointer to the VS ACL to be added or replaced.
             * @param[in] tbl_oid Object ID of the ACL table where the entry will be added or replaced.
             * @param[in] aces Pointer to the ACEs.
             * @param[in] ordered_aces Ordered ACE list.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t acl_add_replace(
                    _In_ vpp_acl_t *&acl,
                    _In_ sai_object_id_t tbl_oid,
                    _In_ acl_tbl_entries_t *aces,
                    _In_ std::list<ordered_ace_list_t> &ordered_aces);

            /**
             * @brief Allocates memory for VS ACL.
             *
             * @param[in] n_entries Number of entries in the ACL.
             * @param[in] tbl_oid Object ID of the table to which the ACL belongs.
             * @param[out] acl_name Name of the allocated ACL.
             * @param[out] acl Pointer to the allocated ACL.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t allocate_acl(
                    _In_ size_t n_entries,
                    _In_ sai_object_id_t tbl_oid,
                    _Out_ char (&acl_name)[64],
                    _Out_ vpp_acl_t *&acl);

            /**
             * @brief Allocates memory for VS tunnel termination ACL.
             *
             * @param[in] n_tunterm_entries Number of tunnel termination entries to allocate.
             * @param[in] tbl_oid Object ID of the ACL table.
             * @param[out] acl_name Name of the allocated ACL.
             * @param[out] tunterm_acl Pointer to the allocated tunnel termination ACL.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t allocate_tunterm_acl(
                    _In_ size_t n_tunterm_entries,
                    _In_ sai_object_id_t tbl_oid,
                    _Out_ char (&acl_name)[64],
                    _Out_ vpp_tunterm_acl_t *&tunterm_acl);

            /**
             * @brief Counts the total number of ACL rules and tunnel termination ACL rules, and sets is_tunterm in the ordered ACE list.
             *
             * @param[in] aces Pointer to ACEs.
             * @param[in] ordered_aces Reference to the ordered ACE list.
             * @param[out] n_entries Reference to the number of ACL entries.
             * @param[out] n_tunterm_entries Reference to the number of tunnel termination ACL entries.
             */
            void count_tunterm_acl_rules(
                    _In_ acl_tbl_entries_t *aces,
                    _In_ std::list<ordered_ace_list_t> &ordered_aces,
                    _Out_ size_t &n_entries,
                    _Out_ size_t &n_tunterm_entries);

            /**
             * @brief Deletes the hardware ports map associated with the given table object ID.
             *
             * @param[in] tbl_oid The object ID of the table whose hardware ports map is to be deleted.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t tbl_hw_ports_map_delete(
                    _In_ sai_object_id_t tbl_oid);

            /**
             * @brief Deletes a tunnel termination ACL in VS.
             *
             * @param[in] tbl_oid The object ID of the ACL table.
             * @param[in] table_delete Boolean flag indicating whether the overall ACL table is being deleted.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t tunterm_acl_delete(
                    _In_ sai_object_id_t tbl_oid,
                    _In_ bool table_delete);

            /**
             * @brief Creates or replaces the provided tunnel termination ACL in VS.
             *
             * @param[in] acl Pointer to the tunnel termination ACL.
             * @param[in] tbl_oid Object ID of the ACL table where the entry will be added or replaced.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t tunterm_acl_add_replace(
                    _In_ vpp_tunterm_acl_t *acl,
                    _In_ sai_object_id_t tbl_oid);

            /**
             * @brief Sets the redirect action for a tunnel termination ACL rule.
             *
             * @param[in] attr_id The ID of the SAI attribute to be updated.
             * @param[in] value Pointer to the SAI attribute value.
             * @param[in] rule Pointer to the tunnel termination ACL rule to be updated.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t tunterm_set_action_redirect(
                    _In_ sai_acl_entry_attr_t attr_id,
                    _In_ const sai_attribute_value_t *value,
                    _In_ vpp_tunterm_acl_rule_t *rule);

            /**
             * @brief Updates an ACE field for a tunnel termination ACL rule.
             *
             * @param[in] attr_id The ID of the SAI attribute to be updated.
             * @param[in] value Pointer to the SAI attribute value.
             * @param[in] rule Pointer to the tunnel termination ACL rule to be updated.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t tunterm_acl_rule_field_update(
                    _In_ sai_acl_entry_attr_t attr_id,
                    _In_ const sai_attribute_value_t *value,
                    _In_ vpp_tunterm_acl_rule_t *rule);

            /**
             * @brief Binds or unbinds a tunnel termination ACL table to/from an interface.
             *
             * @param[in] tbl_oid The object ID of the ACL table to be bound or unbound.
             * @param[in] is_add A boolean flag indicating whether to bind (true) or unbind (false) the ACL table.
             * @param[in] hwif_name The name of the hardware interface to which the ACL table is to be bound or from which it is to be unbound.
             * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
             */
            sai_status_t tunterm_acl_bindunbind(
                    _In_ sai_object_id_t tbl_oid,
                    _In_ bool is_add,
                    _In_ std::string hwif_name);

            sai_status_t aclTableCreate(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t aclDefaultAllowConfigure(
                    _In_ sai_object_id_t tbl_oid);

            sai_status_t acl_rule_range_get(
                    _In_ const sai_object_list_t *range_list,
                    _Out_ sai_u32_range_t *range_limit_list,
                    _Out_ sai_acl_range_type_t *range_type_list,
                    _Out_ uint32_t *range_count);

            sai_status_t acl_range_attr_get(
                    _In_ const std::string &serializedObjectId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _Out_ sai_attribute_t *attr_range);

            sai_status_t removeAclGrp(
                    _In_ const std::string &serializedObjectId);

            sai_status_t setAclGrpMbr(
                    _In_ sai_object_id_t member_oid,
                    _In_ const sai_attribute_t* attr);

            sai_status_t removeAclGrpMbr(
                    _In_ const std::string &serializedObjectId);

            sai_status_t createAclGrpMbr(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t addRemoveAclGrpMbr(
                    _In_ sai_object_id_t member_id,
                    _In_ sai_object_id_t tbl_grp_oid,
                    _In_ bool is_add);

            sai_status_t getAclTableGroupId(
                    _In_ sai_object_id_t member_id,
                    _Out_ sai_object_id_t *tbl_grp_oid);

            sai_status_t addRemovePortTblGrp(
                    _In_ sai_object_id_t port_oid,
                    _In_ sai_object_id_t tbl_grp_oid,
                    _In_ bool is_add);

            sai_status_t aclBindUnbindPort(
                    _In_ sai_object_id_t port_oid,
                    _In_ sai_object_id_t tbl_grp_oid,
                    _In_ bool is_input,
                    _In_ bool is_bind);

            sai_status_t aclBindUnbindPorts(
                    _In_ sai_object_id_t tbl_grp_oid,
                    _In_ sai_object_id_t tbl_oid,
                    _In_ bool is_bind);

            sai_status_t getAclEntryStats(
                    _In_ sai_object_id_t ace_cntr_oid,
                    _In_ uint32_t attr_count,
                    _Out_ sai_attribute_t *attr_list);

        public: // VPP

            sai_status_t aclGetVppIndices(
                    _In_ sai_object_id_t ace_oid,
                    _Out_ uint32_t *acl_index,
                    _Out_ uint32_t *ace_index);

            bool vpp_get_hwif_name (
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t vlan_id,
                    _Out_ std::string& ifname);

        public:

            virtual sai_status_t initialize_default_objects(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

        protected: // VPP
            typedef struct platform_bond_info_ {
                uint32_t sw_if_index;
                uint32_t id;
                bool lcp_created;
            } platform_bond_info_t;

            void populate_if_mapping();

            const char *tap_to_hwif_name(const char *name);

            const char *hwif_to_tap_name(const char *name);

            uint32_t find_new_bond_id();
            sai_status_t get_lag_bond_info(const sai_object_id_t lag_id, platform_bond_info_t &bond_info);
            int remove_lag_to_bond_entry (const sai_object_id_t lag_id);

            void vppProcessEvents ();

            void startVppEventsThread();

        private: // VPP

            std::map<std::string, std::string> m_hostif_hwif_map;
            std::map<std::string, std::string> m_hwif_hostif_map;
            int mapping_init = 0;
            bool m_run_vpp_events_thread = true;
            bool VppEventsThreadStarted = false;
            std::shared_ptr<std::thread> m_vpp_thread;

        private: // VPP
	    std::map<sai_object_id_t, platform_bond_info_t> m_lag_bond_map;
	    std::mutex LagMapMutex;

            static int currentMaxInstance;

            std::set<int> availableInstances;

            //1-4095 BD are statically allocated for VLAN by VLAN-ID. Use 4K-16K for dynamic allocation
            static const uint32_t dynamic_bd_id_base =  4*1024;
            static const uint16_t dynamic_bd_id_pool_size =  12*1024;

            BitResourcePool dynamic_bd_id_pool = BitResourcePool(dynamic_bd_id_pool_size, dynamic_bd_id_base);

            std::set<FdbInfo> m_fdb_info_set;

            std::map<std::string, std::shared_ptr<HostInterfaceInfo>> m_hostif_info_map;

            // SRv6 object tracking for CRM
            constexpr static const int m_maxMySidEntries = 1000;
            uint32_t m_srv6_my_sid_count = 0;

            std::shared_ptr<RealObjectIdManager> m_realObjectIdManager;

            friend class TunnelManagerSRv6;

            TunnelManagerSRv6 m_tunnel_mgr_srv6;

        protected: // switch capability related
            virtual sai_status_t queryHashNativeHashFieldListCapability(
                _Inout_ sai_s32_list_t *enum_values_capability) override;

            virtual sai_status_t querySwitchHashAlgorithmCapability(
                _Inout_ sai_s32_list_t *enum_values_capability) override;

    };
}
