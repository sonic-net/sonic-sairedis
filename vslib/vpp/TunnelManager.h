#pragma once

#include <array>
#include "SwitchVpp.h"
#include "vppxlate/SaiVppXlate.h"

namespace saivs
{
    class SwitchVpp;
    enum class Action {
        CREATE,
        UPDATE,
        DELETE
    };
    /**
     * @brief The TunnelVPPData class represents the VPP data associated with a tunnel.
     */
    class TunnelVPPData {
    public:
        TunnelVPPData() {
            memset(&src_ip, 0, sizeof(src_ip));
            memset(&dst_ip, 0, sizeof(dst_ip));
            memset(&bvi_addr, 0, sizeof(bvi_addr));
        }
        // Common fields
        u_int32_t sw_if_index = 0;
        std::shared_ptr<IpVrfInfo> ip_vrf;

        u_int32_t encap_vrf_id = 0;
        u_int32_t bd_id = 0;
        vpp_ip_addr_t bvi_addr;

        // L2 VXLAN fields
        u_int32_t vni = 0;
        u_int16_t vlan_id = 0;
        sai_ip_address_t src_ip;
        sai_ip_address_t dst_ip;
    };

    class TunnelManager {
    public:
        TunnelManager(SwitchVpp* switch_db);

        // sai_status_t create_tunnel_map_entry(
        //                 _In_ const std::string &serializedObjectId,
        //                 _In_ sai_object_id_t switch_id,
        //                 _In_ uint32_t attr_count,
        //                 _In_ const sai_attribute_t *attr_list);

        /**
         * @brief Create a tunnel encap nexthop entry.
         *
         * This function creates a tunnel encap nexthop entry with the specified attributes.
         *
         * @param serializedObjectId The serialized object ID of the tunnel encap nexthop entry.
         * @param switch_id The switch ID.
         * @param attr_count The number of attributes in the attribute list.
         * @param attr_list The attribute list.
         * @return The status of the operation.
         */
        sai_status_t create_tunnel_encap_nexthop(
                _In_ const std::string& serializedObjectId,
                _In_ sai_object_id_t switch_id,
                _In_ uint32_t attr_count,
                _In_ const sai_attribute_t *attr_list);


        /**
         * @brief Remove a tunnel encap nexthop entry.
         *
         * This function removes the tunnel encap nexthop entry with the specified serialized object ID.
         *
         * @param serializedObjectId The serialized object ID of the tunnel encap nexthop entry to be removed.
         * @return The status of the operation.
         */
        sai_status_t remove_tunnel_encap_nexthop(
                _In_ const std::string& serializedObjectId);

        /**
         * @brief Get the tunnel interface index based on the given nexthop OID.
         *
         * This method returns the tunnel interface associated with the given nexthop OID.
         *
         * @param nexthop_oid The nexthop OID.
         * @param sw_if_index The output parameter to store the tunnel interface index.
         * @return The status of the operation.
         */
        sai_status_t get_tunnel_if(
            _In_  sai_object_id_t nexthop_oid,
            _Out_ u_int32_t &sw_if_index)
        {
            SWSS_LOG_ENTER();

            auto it = m_tunnel_encap_nexthop_map.find(nexthop_oid);
            if (it != m_tunnel_encap_nexthop_map.end()) {
                sw_if_index = it->second.sw_if_index;
                return SAI_STATUS_SUCCESS;
            }
            return SAI_STATUS_ITEM_NOT_FOUND;
        }
        /**
         * @brief Set VxLAN router default MAC address.
         */
        void set_router_mac(const sai_attribute_t* attr);

        /**
         * @brief Get VxLAN router default MAC address.
         */
        const std::array<uint8_t, 6>& get_router_mac() const;

        /**
         * @brief Set VxLAN port.
         */
        void set_vxlan_port(const sai_attribute_t* attr);

        /**
         * @brief Create L2 VXLAN tunnel for EVPN.
         *
         * This function is called when a SAI P2P tunnel object is created,
         * typically triggered by EVPN discovering a remote VTEP via BGP IMET routes.
         *
         * @param tunnel_oid The SAI tunnel object ID.
         * @param sw_if_index Output parameter for the VPP interface index.
         * @return SAI_STATUS_SUCCESS on success or if skipped (P2MP tunnel),
         *         error status on failure.
         */
        sai_status_t create_l2_vxlan_tunnel(
            _In_ sai_object_id_t tunnel_oid,
            _Out_ uint32_t& sw_if_index);
        
        sai_status_t remove_l2_vxlan_tunnel(
            _In_ sai_object_id_t tunnel_oid);

    private:
        SwitchVpp* m_switch_db;
        std::array<uint8_t, 6> m_router_mac;
        u_int16_t m_vxlan_port;
        //nexthop SAI object ID to sw_if_index map
        std::unordered_map<sai_object_id_t, TunnelVPPData> m_tunnel_encap_nexthop_map;
        // Map from tunnel SAI OID to VPP tunnel data (L2 VXLAN / EVPN)
        std::unordered_map<sai_object_id_t, TunnelVPPData> m_l2_tunnel_map;

        sai_status_t tunnel_encap_nexthop_action(
                        _In_ const SaiObject* tunnel_nh_obj,
                        _In_ Action action);

        /**
         * @brief Create VPP VXLAN tunnel encapsulation.
         *
         * @param req VPP VXLAN tunnel request parameters.
         * @param tunnel_data Output tunnel data with sw_if_index set on success.
         * @param skip_neighbor When true, skip adding IP neighbor entries.
         *        Set to true for L2 VXLAN tunnels that don't need L3 neighbor
         *        entries for inner-ether forwarding.
         * @return SAI_STATUS_SUCCESS on success, error status on failure.
         */
        sai_status_t create_vpp_vxlan_encap(
                        _In_  vpp_vxlan_tunnel_t& req,
                        _Out_ TunnelVPPData& tunnel_data,
                        _In_  bool skip_neighbor = false);

        /**
         * @brief Remove VPP VXLAN tunnel encapsulation.
         *
         * @param req VPP VXLAN tunnel request parameters.
         * @param tunnel_data Tunnel data with sw_if_index of tunnel to remove.
         * @param skip_neighbor When true, skip removing IP neighbor entries.
         *        Must match the value used during creation.
         * @return SAI_STATUS_SUCCESS on success, error status on failure.
         */
        sai_status_t remove_vpp_vxlan_encap(
                        _In_  vpp_vxlan_tunnel_t& req,
                        _In_ TunnelVPPData& tunnel_data,
                        _In_  bool skip_neighbor = false);

        sai_status_t create_vpp_vxlan_decap(
                        _Out_ TunnelVPPData& tunnel_data);

        sai_status_t remove_vpp_vxlan_decap(
                        _In_ TunnelVPPData& tunnel_data);
    };

    class TunnelManagerSRv6 {
    public:
        TunnelManagerSRv6(SwitchVpp* switch_db);
        /**
         * @brief Creates a new MySID (Segment Identifier) entry.
         *
         * @param[in] serializedObjectId The serialized object ID.
         * @param[in] switch_id The switch object ID.
         * @param[in] attr_count The number of attributes in the attribute list.
         * @param[in] attr_list The list of attributes for the SID entry.
         * @return SAI_STATUS_SUCCESS on success, or an appropriate error code on failure.
         */
        sai_status_t create_my_sid_entry(
            _In_ const std::string &serializedObjectId,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

        /**
         * @brief Removes a MySID (Segment Identifier) entry.
         *
         * @param[in] serializedObjectId The serialized object ID of the SID entry to be removed.
         * @return SAI_STATUS_SUCCESS on success, or an appropriate error code on failure.
         */
        sai_status_t remove_my_sid_entry(
            _In_ const std::string &serializedObjectId);

        /**
         * @brief Creates a SID list.
         *
         * @param[in] serializedObjectId The serialized object ID.
         * @param[in] switch_id The switch object ID.
         * @param[in] attr_count The number of attributes in the attribute list.
         * @param[in] attr_list The list of attributes.
         * @return SAI_STATUS_SUCCESS on success, or an appropriate error code on failure.
         */
        sai_status_t create_sidlist(
            _In_ const std::string &serializedObjectId,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

        /**
         * @brief Removes a SID list based on the serialized object ID.
         *
         * @param[in] serializedObjectId The serialized object ID of the SID list to be removed.
         * @return SAI_STATUS_SUCCESS on success, or an appropriate error code on failure.
         */
        sai_status_t remove_sidlist(
            _In_ const std::string &serializedObjectId);

        /**
         * @brief Creates a SID list route entry.
         *
         * @param[in] serializedObjectId The serialized object ID.
         * @param[in] switch_id The SAI object ID of the switch.
         * @param[in] attr_count The number of attributes in the attribute list.
         * @param[in] attr_list The list of attributes.
         * @return SAI_STATUS_SUCCESS on success, or an appropriate error code on failure.
         */
        sai_status_t create_sidlist_route_entry(
            _In_ const std::string &serializedObjectId,
            _In_ sai_object_id_t switch_id,
            _In_ uint32_t attr_count,
            _In_ const sai_attribute_t *attr_list);

        /**
         * @brief Removes a SID list route entry.
         *
         * @param[in] serializedObjectId The serialized object ID of the SID list route entry to be removed.
         * @param[in] next_hop_oid The object ID of the next hop associated with the SID list route entry.
         * @return SAI_STATUS_SUCCESS on success, or an appropriate error code otherwise.
         */
        sai_status_t remove_sidlist_route_entry(
            _In_ const std::string &serializedObjectId,
            _In_ sai_object_id_t next_hop_oid);

    private:
        SwitchVpp* m_switch_db;

        sai_status_t add_remove_my_sid_entry(
                        _In_ const SaiObject* my_sid_obj,
                        _In_ bool is_del);

        sai_status_t fill_my_sid_entry(
                        _In_ const SaiObject* my_sid_obj,
                        _Out_ vpp_my_sid_entry_t &my_sid);

        sai_status_t fill_next_hop(
                        _In_ sai_object_id_t next_hop_oid,
                        _Out_ vpp_ip_addr_t &nh_ip,
                        _Out_ uint32_t &vlan_idx,
                        _Out_ char (&if_name)[64]);

        sai_status_t create_sidlist_internal(
                        _In_ const std::string &serializedObjectId,
                        _In_ uint32_t attr_count,
                        _In_ const sai_attribute_t *attr_list);

        sai_status_t fill_sidlist(
                        _In_ const std::string &sidlist_id,
                        _In_ uint32_t attr_count,
                        _In_ const sai_attribute_t *attr_list,
                        _Out_ vpp_sidlist_t &sidlist);

        vpp_ip_addr_t generate_bsid(
                        _In_ sai_object_id_t sid_list_oid);

        sai_status_t remove_sidlist_internal(
                        _In_ const std::string &serializedObjectId);

        sai_status_t sr_steer_add_remove(
                        _In_ const std::string &serializedObjectId,
                        _In_ sai_object_id_t next_hop_oid,
                        _In_ bool is_del);

        sai_status_t fill_sr_steer(
                        _In_ const std::string &serializedObjectId,
                        _In_ sai_object_id_t next_hop_oid,
                        _Out_ vpp_sr_steer_t  &sr_steer);

        sai_status_t fill_bsid_set_src_addr(
                        _In_ sai_object_id_t nexthop_oid,
                        _Out_ vpp_ip_addr_t &bsid);
    };
}