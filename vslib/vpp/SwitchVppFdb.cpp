#include "SwitchVpp.h"

#include "swss/exec.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiVppXlate.h"

#include "SwitchVppUtils.h"

using namespace saivs;

/**
 * @brief FDB_ENTRY FLUSH Modes.
 */
 typedef enum _fdb_flush_mode_t
 {
     FLUSH_BY_INTERFACE = 1, /* Flushing DYNAMIC FDB_ENTRY on Interface */
     FLUSH_BY_BD_ID = 2,     /* Flushing DYNAMIC FDB_ENTRY on Bridge */
     FLUSH_ALL = 4,          /* Flushing all DYNAMIC FDB_ENTRY on all */
 } fdb_flush_mode;

// utility function to check whether a bridge port is of type TUNNEL
bool SwitchVpp::is_tunnel_bridge_port(
        _In_ sai_object_id_t br_port_id)
{
    SWSS_LOG_ENTER();

    try
    {
        auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT)
                                         .at(sai_serialize_object_id(br_port_id));

        auto meta = sai_metadata_get_attr_metadata(
                        SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_TYPE);

        auto it = br_port_attrs.find(meta->attridname);

        if (it != br_port_attrs.end() &&
            it->second->getAttr()->value.s32 == SAI_BRIDGE_PORT_TYPE_TUNNEL)
        {
            return true;
        }
    }
    catch (const std::exception &e)
    {
        SWSS_LOG_WARN("is_tunnel_bridge_port: exception for %s: %s",
                sai_serialize_object_id(br_port_id).c_str(), e.what());
    }

    return false;
}

sai_status_t SwitchVpp::createVlanMember(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_VLAN_MEMBER, sid, switch_id, attr_count, attr_list));

    return vpp_create_vlan_member(attr_count, attr_list);

}

sai_status_t SwitchVpp::vpp_create_vlan_member(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t br_port_id;

    //find sw_if_index for given l2 interface
    auto attr_type = sai_metadata_get_attr_by_id(SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    br_port_id = attr_type->value.oid;
    sai_object_type_t obj_type = objectTypeQuery(br_port_id);

    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
    {
        SWSS_LOG_ERROR("SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID=%s expected to be BRIDGE PORT but is: %s",
                sai_serialize_object_id(br_port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Skip VPP operations for tunnel bridge ports -- they have no physical port
    if (is_tunnel_bridge_port(br_port_id))
    {
        SWSS_LOG_NOTICE("Skipping VLAN member VPP create for tunnel bridge port %s",
                sai_serialize_object_id(br_port_id).c_str());
        return SAI_STATUS_SUCCESS;
    }

    const char *hwifname = nullptr;
    uint32_t lag_swif_idx;

    auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_id));
    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto bp_attr = br_port_attrs[meta->attridname];
    auto port_id = bp_attr->getAttr()->value.oid;
    obj_type = objectTypeQuery(port_id);

    if (obj_type != SAI_OBJECT_TYPE_PORT && obj_type != SAI_OBJECT_TYPE_LAG )
    {
        SWSS_LOG_NOTICE("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT or LAG but is: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    if (obj_type == SAI_OBJECT_TYPE_PORT)
    {
        std::string if_name;
        bool found = getTapNameFromPortId(port_id, if_name);
        if (found == true)
        {
            hwifname = tap_to_hwif_name(if_name.c_str());
        }else {
            SWSS_LOG_NOTICE("No ports found for bridge port id :%s",sai_serialize_object_id(br_port_id).c_str());
            return SAI_STATUS_FAILURE;
        }
    } else if (obj_type == SAI_OBJECT_TYPE_LAG) {
        platform_bond_info_t bond_info;
        CHECK_STATUS(get_lag_bond_info(port_id, bond_info));
        lag_swif_idx = bond_info.sw_if_index;
        SWSS_LOG_NOTICE("lag swif idx :%d",lag_swif_idx);
	    hwifname =  vpp_get_swif_name(lag_swif_idx);
        SWSS_LOG_NOTICE("lag swif idx :%d swif_name:%s",lag_swif_idx, hwifname);
	    if (hwifname == NULL) {
            SWSS_LOG_NOTICE("LAG is not found for bridge port id :%s",sai_serialize_object_id(br_port_id).c_str());
            return SAI_STATUS_FAILURE;
	    }
    }

    auto attr_vlan_member = sai_metadata_get_attr_by_id(SAI_VLAN_MEMBER_ATTR_VLAN_ID, attr_count, attr_list);

    sai_object_id_t vlan_oid;

    if (attr_vlan_member == NULL)
    {
	    SWSS_LOG_NOTICE("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID was not passed");
	    return SAI_STATUS_FAILURE;
    } else {
	    vlan_oid = attr_vlan_member->value.oid;
    }
    auto attr_vlanid_map = m_objectHash.at(SAI_OBJECT_TYPE_VLAN).at(sai_serialize_object_id(vlan_oid));
    auto md_vlan_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN, SAI_VLAN_ATTR_VLAN_ID);
    auto vlan_id =  attr_vlanid_map.at(md_vlan_id->attridname)->getAttr()->value.u16;

    if (vlan_id == 0)
    {
        SWSS_LOG_NOTICE("attr VLAN object id  was not passed");
        return SAI_STATUS_FAILURE;
    }

    uint32_t bridge_id = (uint32_t)vlan_id;
    auto attr_tag_mode = sai_metadata_get_attr_by_id(SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE, attr_count, attr_list);
    uint32_t tagging_mode = 0;
    const char *hw_ifname;
    char host_subifname[32];

    if (attr_tag_mode == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID was not passed");
        return SAI_STATUS_FAILURE;
    }

    tagging_mode = attr_tag_mode->value.u32;

    if (tagging_mode == SAI_VLAN_TAGGING_MODE_TAGGED)
    {
        /*
         create vpp subinterface and set it as bridge port
        */
        snprintf(host_subifname, sizeof(host_subifname), "%s.%u", hwifname, vlan_id);

        /* The host(tap) subinterface is also created as part of the vpp subinterface creation */
        create_sub_interface(hwifname, vlan_id, vlan_id);

        /* Get new list of physical interfaces from VS */
        refresh_interfaces_list();

        hw_ifname = host_subifname;

        //Create bridge and set the l2 port
        set_sw_interface_l2_bridge(hw_ifname,bridge_id, true, VPP_API_PORT_TYPE_NORMAL);

        swif_bdid_track(hw_ifname, bridge_id);

        //Set interface state up
        interface_set_state(hw_ifname, true);
    }
    else if (tagging_mode == SAI_VLAN_TAGGING_MODE_UNTAGGED)
    {
        hw_ifname = hwifname;

        //Create bridge and set the l2 port
        set_sw_interface_l2_bridge(hw_ifname,bridge_id, true, VPP_API_PORT_TYPE_NORMAL);

        swif_bdid_track(hw_ifname, bridge_id);

        //Set the vlan member to bridge and tags rewrite
        vpp_l2_vtr_op_t vtr_op = L2_VTR_PUSH_1;
        vpp_vlan_type_t push_dot1q = VLAN_DOT1Q;
        uint32_t tag1 = (uint32_t)vlan_id;
        uint32_t tag2 = ~0;
        set_l2_interface_vlan_tag_rewrite(hw_ifname, tag1, tag2, push_dot1q, vtr_op);
    }
    else {
        SWSS_LOG_ERROR("Tagging Mode %d not implemented", tagging_mode);
        return SAI_STATUS_FAILURE;
    }


    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeVlanMember(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    vpp_remove_vlan_member(objectId);

    auto sid = sai_serialize_object_id(objectId);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_VLAN_MEMBER, sid));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_remove_vlan_member(
        _In_ sai_object_id_t vlan_member_oid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;

    sai_status_t status = get(SAI_OBJECT_TYPE_VLAN_MEMBER, vlan_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID is not present");

        return SAI_STATUS_FAILURE;
    }
    sai_object_id_t vlan_oid = attr.value.oid;

    sai_object_type_t obj_type = objectTypeQuery(vlan_oid);

    if (obj_type != SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID is not valid");
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_VLAN, vlan_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_ATTR_VLAN_ID is not present");

        return SAI_STATUS_FAILURE;
    }
    auto vlan_id = attr.value.u16;
    uint32_t bridge_id = (uint32_t)vlan_id;

    attr.id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
    status = get(SAI_OBJECT_TYPE_VLAN_MEMBER, vlan_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID is not present");
        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t br_port_oid = attr.value.oid;

    obj_type = objectTypeQuery(br_port_oid);
    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
    {
        SWSS_LOG_ERROR("SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID=%s expected to be BRIDGE PORT but is: %s",
                sai_serialize_object_id(br_port_oid).c_str(),
                sai_serialize_object_type(obj_type).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Skip VPP operations for tunnel bridge ports -- they have no physical port
    if (is_tunnel_bridge_port(br_port_oid))
    {
        SWSS_LOG_NOTICE("Skipping vlan member remove for TUNNEL bridge port %s",
                sai_serialize_object_id(br_port_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    const char *hw_ifname = nullptr;
    auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_oid));

    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto bp_attr = br_port_attrs[meta->attridname];
    auto port_id = bp_attr->getAttr()->value.oid;
    obj_type = objectTypeQuery(port_id);

    if (obj_type != SAI_OBJECT_TYPE_PORT && obj_type != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_NOTICE("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT or LAG but is: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    if (obj_type == SAI_OBJECT_TYPE_PORT)
    {
        std::string if_name;
        bool found = getTapNameFromPortId(port_id, if_name);
        if (found == true)
        {
            hw_ifname = tap_to_hwif_name(if_name.c_str());
        }else {
            SWSS_LOG_NOTICE("No ports found for bridge port id :%s",sai_serialize_object_id(br_port_oid).c_str());
            return SAI_STATUS_FAILURE;
        }
    } else if (obj_type == SAI_OBJECT_TYPE_LAG) {
        platform_bond_info_t bond_info;
        CHECK_STATUS(get_lag_bond_info(port_id, bond_info));
        uint32_t lag_swif_idx = bond_info.sw_if_index;
        SWSS_LOG_NOTICE("lag swif idx :%d",lag_swif_idx);
	    hw_ifname =  vpp_get_swif_name(lag_swif_idx);
        SWSS_LOG_NOTICE("lag swif idx :%d swif_name:%s",lag_swif_idx, hw_ifname);
	    if (hw_ifname == NULL) {
            SWSS_LOG_NOTICE("LAG port is not found for bridge port id :%s",sai_serialize_object_id(port_id).c_str());
            return SAI_STATUS_FAILURE;
	    }
    }

    attr.id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
    status = get(SAI_OBJECT_TYPE_VLAN_MEMBER, vlan_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE is not present");

        return SAI_STATUS_FAILURE;
    }

    uint32_t tagging_mode = attr.value.s32;
    char host_subifname[32];
    if (tagging_mode == SAI_VLAN_TAGGING_MODE_UNTAGGED)
    {

        //First disable tag-rewrite.
        vpp_l2_vtr_op_t vtr_op =L2_VTR_DISABLED;
        vpp_vlan_type_t push_dot1q = VLAN_DOT1Q;
        uint32_t tag1 = (uint32_t)vlan_id;
        uint32_t tag2 = ~0;
        set_l2_interface_vlan_tag_rewrite(hw_ifname, tag1, tag2, push_dot1q, vtr_op);

        //Remove interface from bridge, interface type should be changed to others types like l3.
        set_sw_interface_l2_bridge(hw_ifname, bridge_id, false, VPP_API_PORT_TYPE_NORMAL);
        swif_bdid_untrack(hw_ifname);
    }
    else if (tagging_mode == SAI_VLAN_TAGGING_MODE_TAGGED)
    {

        // set interface l2 tag-rewrite GigabitEthernet0/8/0.200 disable
        snprintf(host_subifname, sizeof(host_subifname), "%s.%u", hw_ifname, vlan_id);
        hw_ifname = host_subifname;
        // Remove the l2 port from bridge
        set_sw_interface_l2_bridge(hw_ifname, bridge_id, false, VPP_API_PORT_TYPE_NORMAL);
        swif_bdid_untrack(hw_ifname);

        // delete subinterface
        delete_sub_interface(hw_ifname, vlan_id);

        // Get new list of physical interfaces from VS
        refresh_interfaces_list();
    }
    else {

        SWSS_LOG_ERROR("Tagging mode %d not implemented", tagging_mode);
        return SAI_STATUS_FAILURE;
    }

    //Check if the bridge has zero ports left, if so remove the bridge as well
    uint32_t member_count = 0;
    bridge_domain_get_member_count (bridge_id, &member_count);
    if (member_count == 0)
    {
        vpp_bridge_domain_add_del(bridge_id, false);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_create_bvi_interface(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto attr_vlan_oid = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_VLAN_ID, attr_count, attr_list);

    if (attr_vlan_oid == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_VLAN_ID was not passed");
        return SAI_STATUS_SUCCESS;
    }

    sai_object_id_t vlan_oid = attr_vlan_oid->value.oid;

    sai_object_type_t obj_type = objectTypeQuery(vlan_oid);

    if (obj_type != SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_ERROR(" VLAN object type was not passed");
        return SAI_STATUS_SUCCESS;
    }
    auto vlan_attrs = m_objectHash.at(SAI_OBJECT_TYPE_VLAN).at(sai_serialize_object_id(vlan_oid));
    auto md_vlan_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN, SAI_VLAN_ATTR_VLAN_ID);
    auto vlan_id = (uint32_t) vlan_attrs.at(md_vlan_id->attridname)->getAttr()->value.u16;

    if (vlan_id == 0)
    {
	    SWSS_LOG_NOTICE("attr VLAN object id  was not passed");
	    return SAI_STATUS_FAILURE;
    }

    auto attr_mac_addr = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS, attr_count, attr_list);
    if (attr_mac_addr == NULL)
    {
	    SWSS_LOG_NOTICE("attr ROUTER INTERFACE MAC Address is not found");
	    return SAI_STATUS_FAILURE;
    }

    sai_mac_t mac_addr;
    memcpy(mac_addr, attr_mac_addr->value.mac, sizeof(sai_mac_t));

    //Create BVI interface
    create_bvi_interface(mac_addr,vlan_id);

    // Get new list of physical interfaces from VS
    refresh_interfaces_list();

    char hw_bviifname[32];
    const char *hw_ifname;
    snprintf(hw_bviifname, sizeof(hw_bviifname), "bvi%u",vlan_id);
    hw_ifname = hw_bviifname;

    //Create bridge and set the l2 port as BVI
    set_sw_interface_l2_bridge(hw_ifname,vlan_id, true, VPP_API_PORT_TYPE_BVI);

    //Set interface state up
    interface_set_state(hw_ifname, true);

    //Set the bvi as access or untagged port of the bridge
    vpp_l2_vtr_op_t vtr_op = L2_VTR_PUSH_1;
    vpp_vlan_type_t push_dot1q = VLAN_DOT1Q;
    uint32_t tag1 = (uint32_t)vlan_id;
    uint32_t tag2 = ~0;
    set_l2_interface_vlan_tag_rewrite(hw_ifname, tag1, tag2, push_dot1q, vtr_op);

    //Set the arp termination for bridge
    uint32_t bd_id = (uint32_t) vlan_id;
    set_bridge_domain_flags(bd_id, VPP_BD_FLAG_ARP_TERM,true);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_delete_bvi_interface(
        _In_ sai_object_id_t bvi_obj_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    sai_status_t status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, bvi_obj_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_TYPE is not present");
        return SAI_STATUS_FAILURE;
    }

    if (attr.value.s32 != SAI_ROUTER_INTERFACE_TYPE_VLAN)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_TYPE is not VLAN");
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, bvi_obj_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_VLAN_ID is not present");
        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t vlan_oid = attr.value.oid;
    sai_object_type_t obj_type = objectTypeQuery(vlan_oid);

    if (obj_type != SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID is not valid");
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_VLAN, vlan_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_ATTR_VLAN_ID is not present");
        return SAI_STATUS_FAILURE;
    }
    auto vlan_id = attr.value.u16;
    char hw_bviifname[32];
    const char *hw_ifname;
    snprintf(hw_bviifname, sizeof(hw_bviifname), "bvi%u",vlan_id);
    hw_ifname = hw_bviifname;

    //Disable arp termination for bridge
    uint32_t bd_id = (uint32_t) vlan_id;
    set_bridge_domain_flags(bd_id, VPP_BD_FLAG_ARP_TERM, false);

    //First disable tag-rewrite.
    vpp_l2_vtr_op_t vtr_op = L2_VTR_DISABLED;
    vpp_vlan_type_t push_dot1q = VLAN_DOT1Q;
    uint32_t tag1 = (uint32_t)vlan_id;
    uint32_t tag2 = ~0;
    set_l2_interface_vlan_tag_rewrite(hw_ifname, tag1, tag2, push_dot1q, vtr_op);

    //Remove interface from bridge, interface type should be changed to others types like l3.
    set_sw_interface_l2_bridge(hw_ifname, bd_id, false, VPP_API_PORT_TYPE_BVI);

    //Remove the bvi interface
    delete_bvi_interface(hw_ifname);

    // refresh interfaces from VS
    refresh_interfaces_list();

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::get_lag_bond_info(const sai_object_id_t lag_id, platform_bond_info_t &bond_info)
{
    SWSS_LOG_ENTER();

    auto it = m_lag_bond_map.find(lag_id);
    if (it == m_lag_bond_map.end())
    {
        SWSS_LOG_ERROR("failed to find bond info for lag id: %s", sai_serialize_object_id(lag_id).c_str());
        return SAI_STATUS_ITEM_NOT_FOUND;
    }
    bond_info = it->second;
    return SAI_STATUS_SUCCESS;
}

int SwitchVpp::remove_lag_to_bond_entry(const sai_object_id_t lag_oid)
{
    SWSS_LOG_ENTER();

    auto it = m_lag_bond_map.find(lag_oid);

    if (it == m_lag_bond_map.end())
    {
        SWSS_LOG_ERROR("failed to find lag swif index for : %s", sai_serialize_object_id(lag_oid).c_str());
        return ~0;
    }

    SWSS_LOG_NOTICE("Removing lag object swif index: %s", sai_serialize_object_id(lag_oid).c_str());
    m_lag_bond_map.erase(it);
    return 0;
}

sai_status_t SwitchVpp::createLag(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);
    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_LAG, sid, switch_id, attr_count, attr_list));
    return vpp_create_lag(object_id, attr_count, attr_list);

}

/*
 * This function determines the ID of a newly created PortChannel.
 * It does this by querying the list of PortChannels using the ip command and
 * comparing it to the existing LAG interfaces in m_lag_bond_map.
 *
 * This function is necessary due to the way IP addresses are set on interfaces in Sonic-VPP,
 * which requires mapping between the PortChannel interface and the BondEthernet in VPP.
 * Although no issues have been observed during manual and sonic-mgmt testing,
 * it should be noted that this design may theoretically present a race condition,
 * where the wrong ID is returned for a given LAG interface if multiple PortChannels are created concurrently.
 */
uint32_t SwitchVpp::find_new_bond_id()
{
    SWSS_LOG_ENTER();

    std::stringstream cmd;
    std::string res;
    uint32_t bond_id = ~0;

    // Get list of PortChannels from ip command
    cmd << IP_CMD << " -o link show | awk -F': ' '{print $2}' | grep " << PORTCHANNEL_PREFIX;

    int ret = swss::exec(cmd.str(), res);
    if (ret) {
        SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        return ~0;
    }

    if (res.length() == 0) {
        SWSS_LOG_ERROR("No PortChannels found in output of command '%s': %s", cmd.str().c_str(), res.c_str());
        return ~0;
    }

    SWSS_LOG_DEBUG("Output of ip command: %s", res.c_str());

    std::unordered_set<uint32_t> existing_bond_ids;
    for (const auto& entry : m_lag_bond_map) {
        existing_bond_ids.insert(entry.second.id);
    }

    std::istringstream iss(res);
    std::string line;
    bool found_new_bond_id = false;
    while (std::getline(iss, line)) {
        std::string portchannel_name = line.substr(0, line.find('\n'));
        bond_id = std::stoi(portchannel_name.substr(strlen(PORTCHANNEL_PREFIX)));

        if (existing_bond_ids.find(bond_id) == existing_bond_ids.end()) {
            SWSS_LOG_NOTICE("Found new bond id from PortChannel name: %d", bond_id);
            found_new_bond_id = true;
            break;
        }
    }

    return found_new_bond_id ? bond_id : ~0;
}

sai_status_t SwitchVpp::vpp_create_lag(
        _In_ sai_object_id_t lag_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    uint32_t mode, lb;
    uint32_t bond_id = ~0;
    uint32_t swif_idx = ~0;
    const char *hw_ifname;

    // Extract bond_id from PortChannel name
    bond_id = find_new_bond_id();
    if (bond_id == static_cast<uint32_t>(~0))
    {
        SWSS_LOG_ERROR("Bond id could not be found");
        return SAI_STATUS_FAILURE;
    }

    // Set mode and lb. SONiC config does not have provision to pass mode and load balancing algorithm.
    // Select VPP's new opt-in inner-aware LAG hash algorithm (BOND_API_LB_ALGO_L34_INNER, value 6,
    // CLI keyword "l34-inner") so that LAG distribution stays balanced for IPinIP / 6in4 / 4in6 /
    // 6in6 / GRE / NVGRE transit tunnel traffic.  The existing BOND_API_LB_ALGO_L34 (= 1) and the
    // registered hash function "hash-eth-l34" are byte-for-byte unchanged on the VPP side, so a
    // libsaivs that selects value 1 keeps the legacy outer-only hashing behaviour; libsaivs that
    // selects value 6 (this code) gets the inner-aware hash function "hash-eth-l34-inner".
    // ABI compatibility with stock libvppinfra is preserved because the new enum value carries the
    // [backwards_compatible] annotation in src/vnet/bonding/bond.api -- vppapigen excludes it from
    // the CRC of every bond_create* / sw_interface_bond_details / sw_bond_interface_details message.
    mode = VPP_BOND_API_MODE_XOR;
    lb = VPP_BOND_API_LB_ALGO_L34_INNER;

    create_bond_interface(bond_id, mode, lb, &swif_idx);
    if (swif_idx == static_cast<uint32_t>(~0))
    {
        SWSS_LOG_ERROR("failed to create bond interface in VPP for %s", sai_serialize_object_id(lag_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    // Update the lag to bond map
    platform_bond_info_t bond_info = {swif_idx, bond_id, false};
    m_lag_bond_map[lag_id] = bond_info;
    SWSS_LOG_NOTICE("vpp bond interface created for lag_id:%s, swif index:%d, bond_id:%d\n", sai_serialize_object_id(lag_id).c_str(), swif_idx, bond_id);
    refresh_interfaces_list();

    // Set the bond interface state up
    hw_ifname = vpp_get_swif_name(swif_idx);
    SWSS_LOG_NOTICE("Setting lag hw interface state to up :%s",hw_ifname);
    interface_set_state(hw_ifname, true);
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeLag(
        _In_ sai_object_id_t lag_oid)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS_QUIET(vpp_remove_lag(lag_oid));
    auto sid = sai_serialize_object_id(lag_oid);
    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_LAG, sid));
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_remove_lag(
        _In_ sai_object_id_t lag_oid)
{
    SWSS_LOG_ENTER();

    int ret;
    platform_bond_info_t bond_info;

    CHECK_STATUS(get_lag_bond_info(lag_oid, bond_info));
    uint32_t lag_swif_idx = bond_info.sw_if_index;
    auto lag_ifname =  vpp_get_swif_name(lag_swif_idx);
    SWSS_LOG_NOTICE("lag swif idx :%d swif_name:%s",lag_swif_idx, lag_ifname);
    if (lag_ifname == NULL)
    {
        SWSS_LOG_NOTICE("LAG interface name is not found for LAG PORT :%s",sai_serialize_object_id(lag_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    //Delete the Bond interface (also deletes the lcp pair)
    ret = delete_bond_interface(lag_ifname);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("failed to delete bond interface in VPP for %s", sai_serialize_object_id(lag_oid).c_str());
        return SAI_STATUS_FAILURE;
    }
    remove_lag_to_bond_entry(lag_oid);
    refresh_interfaces_list();

    return SAI_STATUS_SUCCESS;
}


sai_status_t SwitchVpp::createLagMember(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_LAG_MEMBER, sid, switch_id, attr_count, attr_list));

    // Add the member to the VPP bond first so the bond (and its LCP/tap) inherits
    // the member MAC, then reflect egress-disable through the member port admin state.
    CHECK_STATUS(vpp_create_lag_member(attr_count, attr_list));

    auto egress_disable = sai_metadata_get_attr_by_id(SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE, attr_count, attr_list);
    if (egress_disable != NULL && egress_disable->value.booldata)
    {
        SWSS_LOG_NOTICE("LAG member %s created with egress disabled, set VPP bond member down", sid.c_str());
        CHECK_STATUS(vpp_set_lag_member_egress_disable(object_id, true));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_create_lag_member(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    bool is_long_timeout = false;
    bool is_passive = false;
    int ret;
    uint32_t bond_if_idx;
    sai_object_id_t lag_oid, lag_port_oid;

    //Get the bond interface index from attr SAI_LAG_MEMBER_ATTR_LAG_ID
    auto attr_type = sai_metadata_get_attr_by_id(SAI_LAG_MEMBER_ATTR_LAG_ID, attr_count, attr_list);
    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_LAG_ID was not passed");
        return SAI_STATUS_FAILURE;
    }
    lag_oid = attr_type->value.oid;
    sai_object_type_t obj_type = objectTypeQuery(lag_oid);

    if (obj_type != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_ERROR(" SAI_LAG_MEMBER_ATTR_LAG_ID = %s expected to be LAG ID but is: %s",
                sai_serialize_object_id(lag_oid).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    platform_bond_info_t bond_info;
    CHECK_STATUS(get_lag_bond_info(lag_oid, bond_info));
    bond_if_idx = bond_info.sw_if_index;
    SWSS_LOG_NOTICE("bond if index is %d\n", bond_if_idx);

    attr_type = sai_metadata_get_attr_by_id(SAI_LAG_MEMBER_ATTR_PORT_ID, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_PORT_ID was not present\n");
        return SAI_STATUS_FAILURE;
    }

    lag_port_oid = attr_type->value.oid;
    SWSS_LOG_NOTICE("lag port id is %s",sai_serialize_object_id(lag_port_oid).c_str());
    obj_type = objectTypeQuery(lag_port_oid);
    if (obj_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_NOTICE("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(lag_port_oid).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    std::string if_name;
    bool found = getTapNameFromPortId(lag_port_oid, if_name);
    const char *hwifname;
    if (found == true)
    {
        hwifname = tap_to_hwif_name(if_name.c_str());
        SWSS_LOG_NOTICE("hwif name for port is %s",hwifname);
    }else {
        SWSS_LOG_NOTICE("No ports found for lag port id :%s",sai_serialize_object_id(lag_port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    ret = create_bond_member(bond_if_idx, hwifname, is_passive, is_long_timeout);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("failed to add bond member in VPP for %s", sai_serialize_object_id(lag_port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    CHECK_STATUS(vpp_ensure_lag_lcp(lag_oid));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_ensure_lag_lcp(
        _In_ sai_object_id_t lag_oid)
{
    SWSS_LOG_ENTER();

    platform_bond_info_t bond_info;
    CHECK_STATUS(get_lag_bond_info(lag_oid, bond_info));

    if (bond_info.lcp_created)
    {
        return SAI_STATUS_SUCCESS;
    }

    uint32_t bond_id = bond_info.id;
    std::ostringstream tap_stream;
    tap_stream << "be" << bond_id;
    std::string tap = tap_stream.str();

    const char *hw_ifname = vpp_get_swif_name(bond_info.sw_if_index);
    if (hw_ifname == NULL)
    {
        SWSS_LOG_ERROR("failed to get VPP bond interface name for %s", sai_serialize_object_id(lag_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    configure_lcp_interface(hw_ifname, tap.c_str(), true);

    std::string portchannel = std::string("PortChannel") + std::to_string(bond_id);
    std::string be = std::string("be") + std::to_string(bond_id);
    CHECK_STATUS(add_tc_filter_redirect(be, portchannel));

    bond_info.lcp_created = true;
    m_lag_bond_map[lag_oid] = bond_info;

    SWSS_LOG_NOTICE("Created LCP and tc redirect for LAG %s", sai_serialize_object_id(lag_oid).c_str());

    return SAI_STATUS_SUCCESS;
}

SwitchVpp::LagMemberEgressDisableAction SwitchVpp::getLagMemberEgressDisableAction(
        _In_ bool requested_egress_disable,
        _In_ bool current_attr_found,
        _In_ bool current_egress_disable)
{
    SWSS_LOG_ENTER();

    if (current_attr_found && current_egress_disable == requested_egress_disable)
    {
        return LagMemberEgressDisableAction::NONE;
    }

    if (!current_attr_found && !requested_egress_disable)
    {
        return LagMemberEgressDisableAction::NONE;
    }

    return requested_egress_disable ? LagMemberEgressDisableAction::DISABLE : LagMemberEgressDisableAction::ENABLE;
}

sai_status_t SwitchVpp::setLagMember(
        _In_ sai_object_id_t lagMemberId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if (attr == nullptr)
    {
        SWSS_LOG_ERROR("LAG member set attribute is null");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto sid = sai_serialize_object_id(lagMemberId);

    auto &objectHash = m_objectHash.at(SAI_OBJECT_TYPE_LAG_MEMBER);
    if (objectHash.find(sid) == objectHash.end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(SAI_OBJECT_TYPE_LAG_MEMBER).c_str(),
                sid.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    if (attr->id != SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE)
    {
        return set_internal(SAI_OBJECT_TYPE_LAG_MEMBER, sid, attr);
    }

    sai_attribute_t current_attr = {};
    current_attr.id = SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE;
    sai_status_t status = get(SAI_OBJECT_TYPE_LAG_MEMBER, lagMemberId, 1, &current_attr);

    // Missing EGRESS_DISABLE means the SAI default is false. Object existence
    // was checked above, so get failure here is treated as missing attr.
    auto action = getLagMemberEgressDisableAction(
            attr->value.booldata,
            status == SAI_STATUS_SUCCESS,
            current_attr.value.booldata);

    if (action == LagMemberEgressDisableAction::NONE)
    {
        return set_internal(SAI_OBJECT_TYPE_LAG_MEMBER, sid, attr);
    }

    if (action == LagMemberEgressDisableAction::DISABLE)
    {
        SWSS_LOG_NOTICE("Disable egress on LAG member %s, set VPP bond member down", sid.c_str());
        CHECK_STATUS(vpp_set_lag_member_egress_disable(lagMemberId, true));
    }
    else
    {
        SWSS_LOG_NOTICE("Enable egress on LAG member %s, set VPP bond member up", sid.c_str());
        CHECK_STATUS(vpp_set_lag_member_egress_disable(lagMemberId, false));
    }

    return set_internal(SAI_OBJECT_TYPE_LAG_MEMBER, sid, attr);
}

sai_status_t SwitchVpp::get_lag_member_port(
        _In_ sai_object_id_t lag_member_oid,
        _Out_ sai_object_id_t& port_oid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_LAG_MEMBER_ATTR_PORT_ID;
    sai_status_t status = get(SAI_OBJECT_TYPE_LAG_MEMBER, lag_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_PORT_ID is not present");
        return SAI_STATUS_FAILURE;
    }

    port_oid = attr.value.oid;
    sai_object_type_t obj_type = objectTypeQuery(port_oid);

    if (obj_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("SAI_LAG_MEMBER_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(port_oid).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

bool SwitchVpp::get_port_admin_state(
        _In_ sai_object_id_t port_oid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_ADMIN_STATE;

    if (get(SAI_OBJECT_TYPE_PORT, port_oid, 1, &attr) == SAI_STATUS_SUCCESS)
    {
        return attr.value.booldata;
    }

    // create_internal does not materialize SAI defaults, so a port whose admin
    // state was never explicitly set has no stored value. An active LAG member
    // port is administratively up unless SONiC explicitly sets it down, so
    // default to up here to preserve that behavior.
    return true;
}

bool SwitchVpp::getLagMemberEffectiveAdminUp(
        _In_ bool admin_up,
        _In_ bool egress_disable)
{
    return admin_up && !egress_disable;
}

sai_status_t SwitchVpp::vpp_apply_member_port_state(
        _In_ sai_object_id_t port_oid,
        _In_ bool egress_disable)
{
    SWSS_LOG_ENTER();

    std::string if_name;
    if (!getTapNameFromPortId(port_oid, if_name))
    {
        SWSS_LOG_ERROR("No port found for lag port id: %s", sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    const char *hwif_name = tap_to_hwif_name(if_name.c_str());

    bool admin_up = get_port_admin_state(port_oid);
    bool effective_up = getLagMemberEffectiveAdminUp(admin_up, egress_disable);

    int ret = interface_set_state(hwif_name, effective_up);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("failed to set VPP bond member %s admin state %s",
                hwif_name, effective_up ? "up" : "down");
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("Set VPP bond member %s effective admin state %s (admin=%s, egress_disable=%s)",
            hwif_name, effective_up ? "up" : "down",
            admin_up ? "up" : "down", egress_disable ? "true" : "false");

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_set_lag_member_egress_disable(
        _In_ sai_object_id_t lag_member_oid,
        _In_ bool egress_disable)
{
    SWSS_LOG_ENTER();

    // The member stays attached to the VPP bond. Egress-disable is reflected by
    // the member port admin state, combined with the port's own SAI admin state:
    // effective up = admin_state && !egress_disable. This keeps the egress-disable
    // and SAI_PORT_ATTR_ADMIN_STATE writers from clobbering each other.
    sai_object_id_t port_oid;
    CHECK_STATUS(get_lag_member_port(lag_member_oid, port_oid));

    // Program VPP first; only record the egress-disable contribution after the
    // VPP update succeeds, so the tracking set never diverges from the dataplane.
    CHECK_STATUS(vpp_apply_member_port_state(port_oid, egress_disable));

    if (egress_disable)
    {
        m_egress_disabled_lag_member_ports.insert(port_oid);
    }
    else
    {
        m_egress_disabled_lag_member_ports.erase(port_oid);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_clear_lag_member_egress_disable(
        _In_ sai_object_id_t lag_member_oid)
{
    SWSS_LOG_ENTER();

    sai_object_id_t port_oid;
    if (get_lag_member_port(lag_member_oid, port_oid) != SAI_STATUS_SUCCESS)
    {
        return SAI_STATUS_SUCCESS;
    }

    if (m_egress_disabled_lag_member_ports.count(port_oid) == 0)
    {
        // The member was not egress-disabled, so its port admin state was never
        // overridden. Leave the normal remove path untouched.
        return SAI_STATUS_SUCCESS;
    }

    // Best-effort restore of the port's own admin state before the member is
    // removed, then drop the egress-disable contribution. The set entry is keyed
    // by port and tied to this member, so clear it even if the restore fails.
    sai_status_t status = vpp_apply_member_port_state(port_oid, false);
    m_egress_disabled_lag_member_ports.erase(port_oid);

    return status;
}

sai_status_t SwitchVpp::removeLagMember(
        _In_ sai_object_id_t lag_member_oid)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS_QUIET(vpp_remove_lag_member(lag_member_oid));

    // Best-effort restore of the member port's own admin state if it was
    // egress-disabled. Removal must still proceed even if the restore fails,
    // since the member is already detached from the VPP bond.
    vpp_clear_lag_member_egress_disable(lag_member_oid);

    auto sid = sai_serialize_object_id(lag_member_oid);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_LAG_MEMBER, sid));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_remove_lag_member(
        _In_ sai_object_id_t lag_member_oid)
{
    SWSS_LOG_ENTER();

    int ret;

    sai_attribute_t attr;

    attr.id = SAI_LAG_MEMBER_ATTR_LAG_ID;

    sai_status_t status = get(SAI_OBJECT_TYPE_LAG_MEMBER, lag_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_LAG_ID is not present");

        return SAI_STATUS_FAILURE;
    }
    sai_object_id_t lag_oid = attr.value.oid;

    sai_object_type_t obj_type = objectTypeQuery(lag_oid);

    if (obj_type != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_LAG_ID is not valid");
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_LAG_MEMBER_ATTR_PORT_ID;

    status = get(SAI_OBJECT_TYPE_LAG_MEMBER, lag_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_PORT_ID is not present");

        return SAI_STATUS_FAILURE;
    }
    sai_object_id_t port_oid = attr.value.oid;

    obj_type = objectTypeQuery(port_oid);

    if (obj_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_PORT_ID is not valid");
        return SAI_STATUS_FAILURE;
    }

    std::string if_name;
    bool found = getTapNameFromPortId(port_oid, if_name);
    const char *lag_member_ifname;
    if (found == true)
    {
        lag_member_ifname = tap_to_hwif_name(if_name.c_str());
	SWSS_LOG_NOTICE("hwif name for port is %s",lag_member_ifname);
    } else {
        SWSS_LOG_NOTICE("No ports found for lag port id :%s",sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    ret = delete_bond_member(lag_member_ifname);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("failed to delete bond member in VPP for %s", sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::FdbEntryadd(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectId, switch_id, attr_count, attr_list));

    vpp_fdbentry_add(serializedObjectId, switch_id, attr_count, attr_list);

    return SAI_STATUS_SUCCESS;

}

sai_status_t SwitchVpp::FdbEntrydel(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    vpp_fdbentry_del(serializedObjectId);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectId));

    return SAI_STATUS_SUCCESS;

}

sai_status_t SwitchVpp::vpp_fdbentry_add(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{

    SWSS_LOG_ENTER();

    sai_fdb_entry_t fdb_entry;
    sai_deserialize_fdb_entry(serializedObjectId, fdb_entry);

    /* Attribute#1 */
    auto attr_type = sai_metadata_get_attr_by_id(SAI_FDB_ENTRY_ATTR_TYPE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_FDB_ENTRY_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }

    bool is_static = (attr_type->value.s32 == SAI_FDB_ENTRY_TYPE_STATIC ? true : false);
    bool is_add = true; /* Adding the entry in FDB*/

    /* Attribute#2 */
    sai_object_id_t br_port_id;
    sai_object_id_t port_id;

    attr_type = sai_metadata_get_attr_by_id(SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    br_port_id = attr_type->value.oid;
    sai_object_type_t obj_type = objectTypeQuery(br_port_id);

    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
    {
        SWSS_LOG_ERROR("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(br_port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Skip VPP FDB add for tunnel bridge ports -- L2 VXLAN FDB is handled
    // separately via the EVPN remote-MAC path, not the per-port FDB path
    if (is_tunnel_bridge_port(br_port_id))
    {
        SWSS_LOG_NOTICE("Skipping FDB add for tunnel bridge port %s",
                sai_serialize_object_id(br_port_id).c_str());
        return SAI_STATUS_SUCCESS;
    }

    auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_id));
    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto bp_attr = br_port_attrs[meta->attridname];
    port_id = bp_attr->getAttr()->value.oid;
    obj_type = objectTypeQuery(port_id);

    if (obj_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_NOTICE("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    /* Need to extract the VLAN ID attached based on the Port_ID */
    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;

    sai_status_t get_status = get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr);

    if (get_status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get port vlan id from port %s",
                sai_serialize_object_id(port_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    uint32_t bd_id = attr.value.u16; /* bd_id is same as VLAN ID for .1Q bridge */

    std::string ifname;
    if (vpp_get_hwif_name(port_id, 0, ifname) == true)
    {
        const char *hwif_name = ifname.c_str();
        auto ret = l2fib_add_del(hwif_name, fdb_entry.mac_address, bd_id, is_add, is_static);
        SWSS_LOG_NOTICE("FDB Entry Added on hwif_name %s Successful ret_val: %d", hwif_name, ret);

    }
    else
    {
        SWSS_LOG_ERROR("FDB_ENTRY failed because of INVALID PORT_ID");

        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_fdbentry_del(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_fdb_entry_t fdb_entry;
    sai_deserialize_fdb_entry(serializedObjectId, fdb_entry);

    sai_object_id_t br_port_id;
    sai_object_id_t port_id;
    bool is_static = false;

    sai_attribute_t attr_list[2];
    /* Attribute#1 */
    attr_list[0].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    /* Attribute#2 */
    attr_list[1].id = SAI_FDB_ENTRY_ATTR_TYPE;

    if (get(SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectId, 1, &attr_list[0]) == SAI_STATUS_SUCCESS)
    {
       if (SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID == attr_list[0].id)
        {
            br_port_id = attr_list[0].value.oid;
        }
        else
        {
            SWSS_LOG_ERROR("DELETE FDB_ENTRY failed because of INVALID ATTR BRIDGE_PORT_ID");
            return SAI_STATUS_FAILURE;
        }

        if (get(SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectId, 1, &attr_list[1]) == SAI_STATUS_SUCCESS)
        {
            if (SAI_FDB_ENTRY_ATTR_TYPE == attr_list[1].id )
            {
                is_static = (attr_list[1].value.s32 == SAI_FDB_ENTRY_TYPE_STATIC ? true : false);
            }
            else
            {
                SWSS_LOG_ERROR("DELETE FDB_ENTRY failed because of INVALID ATTR ENTRY TYPE");
                return SAI_STATUS_FAILURE;
            }
        }
    }
    else
    {
        SWSS_LOG_ERROR(" Invaid Attribute IDs passed for DELETE FDB_ENTRY");
        return SAI_STATUS_FAILURE;
    }
    bool is_add = false; /* Deleting the entry in FDB*/

    sai_object_type_t obj_type = objectTypeQuery(br_port_id);
    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
    {
        SWSS_LOG_ERROR("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(br_port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Skip VPP FDB delete for tunnel bridge ports
    if (is_tunnel_bridge_port(br_port_id))
    {
        SWSS_LOG_NOTICE("Skipping FDB delete for tunnel bridge port %s",
                sai_serialize_object_id(br_port_id).c_str());
        return SAI_STATUS_SUCCESS;
    }

    auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_id));
    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto bp_attr = br_port_attrs[meta->attridname];
    port_id = bp_attr->getAttr()->value.oid;
    obj_type = objectTypeQuery(port_id);

    if (obj_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    /* Need the VLAN ID attached based on the Port_ID */
    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;

    sai_status_t get_status = get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr);

    if (get_status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get port vlan id from port %s",
                sai_serialize_object_id(port_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    uint32_t bd_id = attr.value.u16; /* bd_id is same as VLAN ID for .1Q bridge */

    std::string ifname;

    if (vpp_get_hwif_name(port_id, 0, ifname) == true)
    {
        const char *hwif_name = ifname.c_str();
        auto ret = l2fib_add_del(hwif_name, fdb_entry.mac_address, bd_id, is_add, is_static);
        SWSS_LOG_NOTICE(" Delete FDB_ENTRY on hwif_name %s Successful ret_val: %d", hwif_name, ret);

    }
    else
    {
        SWSS_LOG_ERROR("FDB entry Delete: Invalid ObjectID for the hwif on this bridge");

        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_fdbentry_flush(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attribute;
    sai_object_id_t br_port_id = 0;
    sai_object_id_t port_id;
    uint32_t bd_id = 0;
    uint8_t mode = 0;
    bool is_static_entry = false;

    for (uint32_t i = 0; i < attr_count; i++)
    {
        attribute = attr_list[i];
        switch (attribute.id)
        {
            case SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID:
                {
                    mode |= FLUSH_BY_INTERFACE;
                    br_port_id = attribute.value.oid;
                    sai_object_type_t obj_type = objectTypeQuery(br_port_id);

                    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
                    {
                        SWSS_LOG_ERROR("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID=%s expected to be PORT but is: %s",
                                sai_serialize_object_id(br_port_id).c_str(),
                                sai_serialize_object_type(obj_type).c_str());

                        return SAI_STATUS_FAILURE;
                    }
                }
                break;

            case SAI_FDB_FLUSH_ATTR_BV_ID:
                {
                    mode |= FLUSH_BY_BD_ID;
                    bd_id = attribute.value.u16;
                }
                break;

            case SAI_FDB_FLUSH_ATTR_ENTRY_TYPE:
                {
                    mode |= FLUSH_ALL;
                    is_static_entry = attribute.value.s32;
                    if ( is_static_entry == SAI_FDB_FLUSH_ENTRY_TYPE_STATIC)
                    {
                        SWSS_LOG_ERROR(" Cannot Flush STATIC FDB_ENTRY OBJECTS");
                        return SAI_STATUS_FAILURE;
                    }
                }
                break;

            default:
                SWSS_LOG_ERROR(" Invalid Attributes for fdb entry flush OBJECT");
                return SAI_STATUS_FAILURE;
                break;
        }
    }
    /*
       Here three cases are handled, the FDB_ENTRY's are flushed based on the Attributes set,
       1. If Interface and Type(DYNAMIC is expected here), FLUSH by Interface.
       2. If Bridge_ID(VLAN_ID for .1q) and Type(DYNAMIC is expected here), FLUSH by Bridge ID.
       3. If only Type (DYNAMIC) is set then SONiC FLUSH ALL the dynamic entries.
       */
    SWSS_LOG_NOTICE("VPP_FDB_FLUSH mode is : %d [1,5: Interface, 2,6: Bridge, 3,4,7: Flush ALL, 0: INVALID]", mode);
    switch (mode)
    {
        case FLUSH_BY_INTERFACE:
        case FLUSH_BY_INTERFACE | FLUSH_ALL:/*flush by interface*/
            {
                // Tunnel bridge ports have no physical port -- fall back to flush all
                if (is_tunnel_bridge_port(br_port_id))
                {
                    SWSS_LOG_NOTICE("Tunnel bridge port %s: falling back to flush all",
                            sai_serialize_object_id(br_port_id).c_str());
                    auto ret = l2fib_flush_all();
                    SWSS_LOG_NOTICE("Flush ALL (tunnel bridge port fallback) ret_val: %d", ret);
                    vpp_fdb_entries_invalidate_all();
                    break;
                }

                auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_id));
                auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
                auto bp_attr = br_port_attrs[meta->attridname];
                port_id = bp_attr->getAttr()->value.oid;
                sai_object_type_t obj_type = objectTypeQuery(port_id);

                if (obj_type != SAI_OBJECT_TYPE_PORT)
                {
                    SWSS_LOG_ERROR("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                            sai_serialize_object_id(port_id).c_str(),
                            sai_serialize_object_type(obj_type).c_str());
                    return SAI_STATUS_FAILURE;
                }
                std::string ifname = "";
                if (vpp_get_hwif_name(port_id, 0, ifname) == true)
                {
                    const char *hwif_name = ifname.c_str();
                    auto ret = l2fib_flush_int(hwif_name);
                    SWSS_LOG_NOTICE(" Flush by interface on hwif_name %s  Successful ret_val: %d", hwif_name, ret);
                    vpp_fdb_entries_invalidate_by_port(port_id);
                }
                else
                {
                    SWSS_LOG_ERROR("Flush Interface FDB: Invalid ObjectID for the hwif on this bridge");

                    return SAI_STATUS_FAILURE;
                }
            }
            break;

        case FLUSH_BY_BD_ID:
        case FLUSH_BY_BD_ID | FLUSH_ALL: /*flush by bd_id/vlan id*/
            {
                auto ret = l2fib_flush_bd(bd_id);
                SWSS_LOG_NOTICE(" Flush on bd_id %d Successfull ret_val: %d",bd_id, ret);
                vpp_fdb_entries_invalidate_by_bd(bd_id);
            }
            break;

        case FLUSH_BY_INTERFACE | FLUSH_BY_BD_ID:
        case FLUSH_ALL:
        case FLUSH_BY_INTERFACE| FLUSH_BY_BD_ID| FLUSH_ALL: /*flush all*/
            {
                auto ret = l2fib_flush_all();
                SWSS_LOG_NOTICE(" Flush ALL fdb entry ret_val: %d", ret);
                vpp_fdb_entries_invalidate_all();
            }
            break;

        default:
            SWSS_LOG_ERROR(" Unable to find attrs for FDB_FLUSH %d", mode);
            return SAI_STATUS_FAILURE;
            break;

    }

    return SAI_STATUS_SUCCESS;
}

inline sai_object_id_t SwitchVpp::resolvePortIdFromSwIfIndex(uint32_t sw_if_index)
{
    SWSS_LOG_ENTER();

    auto it = m_swif_to_port_id.find(sw_if_index);
    if (it != m_swif_to_port_id.end())
        return it->second;

    sai_object_id_t port_id = getPortIdFromSwIfIndex(sw_if_index);
    if (port_id != SAI_NULL_OBJECT_ID)
        m_swif_to_port_id[sw_if_index] = port_id;

    return port_id;
}

bool SwitchVpp::generateFdbLearnedOrMoveEvent(const VppFdbKey &key, uint32_t sw_if_index, sai_fdb_event_t event_type)
{
    SWSS_LOG_ENTER();

    bool is_move = (event_type == SAI_FDB_EVENT_MOVE);

    sai_object_id_t port_id = resolvePortIdFromSwIfIndex(sw_if_index);
    if (port_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("FDB: cannot resolve port OID for sw_if_index %u", sw_if_index);
        return false;
    }

    sai_object_id_t bv_id = SAI_NULL_OBJECT_ID;
    sai_object_id_t bridge_port_id = SAI_NULL_OBJECT_ID;

    findBridgeVlanForPortVlan(port_id, (sai_vlan_id_t)key.bd_id, bv_id, bridge_port_id);

    if (bv_id == SAI_NULL_OBJECT_ID || bridge_port_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("FDB: bv_id or bridge_port not found for sw_if_index %u bd %u",
                       sw_if_index, key.bd_id);
        return false;
    }

    std::set<FdbInfo>::iterator existing_it = m_fdb_info_set.end();
    FdbInfo fi;

    if (is_move)
    {
        FdbInfo fi_search;
        fi_search.setVlanId((sai_vlan_id_t)key.bd_id);
        memcpy(fi_search.m_fdbEntry.mac_address, key.mac, sizeof(sai_mac_t));

        existing_it = m_fdb_info_set.find(fi_search);
        if (existing_it == m_fdb_info_set.end())
        {
            SWSS_LOG_WARN("FDB: entry not found in m_fdb_info_set for bd %u, treating as learn",
                          key.bd_id);
            return generateFdbLearnedOrMoveEvent(key, sw_if_index, SAI_FDB_EVENT_LEARNED);
        }
        fi = *existing_it;
    }
    else
    {
        fi.m_fdbEntry.switch_id = m_switch_id;
        fi.m_fdbEntry.bv_id = bv_id;
        memcpy(fi.m_fdbEntry.mac_address, key.mac, sizeof(sai_mac_t));
    }

    fi.setBridgePortId(bridge_port_id);
    fi.setPortId(port_id);
    fi.setVlanId((sai_vlan_id_t)key.bd_id);
    fi.setTimestamp((uint32_t)time(NULL));

    sai_attribute_t attrs[2];
    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = bridge_port_id;

    sai_fdb_event_notification_data_t data;
    data.event_type = event_type;
    data.fdb_entry = fi.getFdbEntry();
    data.attr_count = 2;
    data.attr = attrs;

    auto sid = sai_serialize_fdb_entry(data.fdb_entry);
    sai_status_t status;

    if (is_move)
        status = set_internal(SAI_OBJECT_TYPE_FDB_ENTRY, sid, &attrs[1]);
    else
        status = create_internal(SAI_OBJECT_TYPE_FDB_ENTRY, sid, m_switch_id, 2, attrs);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("FDB: %s failed for %s: %s",
                       is_move ? "set_internal" : "create_internal",
                       sid.c_str(), sai_serialize_status(status).c_str());
        return false;
    }

    if (is_move)
        m_fdb_info_set.erase(existing_it);
    m_fdb_info_set.insert(fi);

    send_fdb_event_notification(data);

    SWSS_LOG_NOTICE("FDB: notified %s for MAC %02x:%02x:%02x:%02x:%02x:%02x bd %u sw_if_index %u",
                    is_move ? "MOVE" : "LEARNED",
                    key.mac[0], key.mac[1], key.mac[2], key.mac[3], key.mac[4], key.mac[5],
                    key.bd_id, sw_if_index);
    return true;
}

bool SwitchVpp::generateFdbAgedEvent(const VppFdbKey &key)
{
    SWSS_LOG_ENTER();

    FdbInfo fi_search;
    fi_search.setVlanId((sai_vlan_id_t)key.bd_id);
    memcpy(fi_search.m_fdbEntry.mac_address, key.mac, sizeof(sai_mac_t));

    auto it = m_fdb_info_set.find(fi_search);
    if (it == m_fdb_info_set.end())
    {
        SWSS_LOG_ERROR("FDB: entry not found in m_fdb_info_set for bd %u", key.bd_id);
        return false;
    }

    FdbInfo fi = *it;

    sai_attribute_t attrs[2];
    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = fi.getBridgePortId();

    sai_fdb_event_notification_data_t data;
    data.event_type = SAI_FDB_EVENT_AGED;
    data.fdb_entry = fi.getFdbEntry();
    data.attr_count = 2;
    data.attr = attrs;

    auto sid = sai_serialize_fdb_entry(data.fdb_entry);
    remove_internal(SAI_OBJECT_TYPE_FDB_ENTRY, sid);

    m_fdb_info_set.erase(it);

    send_fdb_event_notification(data);

    SWSS_LOG_NOTICE("FDB: notified AGED for MAC %02x:%02x:%02x:%02x:%02x:%02x bd %u",
                    key.mac[0], key.mac[1], key.mac[2], key.mac[3], key.mac[4], key.mac[5], key.bd_id);
    return true;
}

void SwitchVpp::swif_bdid_track(const char *hwif_name, uint32_t bd_id)
{
    SWSS_LOG_ENTER();
    uint32_t swif = vpp_get_swif_idx_by_name(hwif_name);
    if (swif != (uint32_t)~0u)
    {
        m_swif_to_bdid[swif] = bd_id;
    }
}

void SwitchVpp::swif_bdid_untrack(const char *hwif_name)
{
    SWSS_LOG_ENTER();
    uint32_t swif = vpp_get_swif_idx_by_name(hwif_name);
    if (swif != (uint32_t)~0u)
    {
        m_swif_to_bdid.erase(swif);

        // Invalidate the sw_if_index -> SAI port OID memoization as well.
        // VPP recycles sw_if_index values after an interface is deleted, so a
        // stale entry would mis-attribute FDB learn/move/age notifications (and
        // vpp_fdb_entries_invalidate_by_port matches) to the previous port OID.
        m_swif_to_port_id.erase(swif);
    }
}

void SwitchVpp::vpp_fdb_entries_invalidate_all()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_INFO("FDB: invalidating all %zu m_vpp_fdb_entries", m_vpp_fdb_entries.size());
    m_vpp_fdb_entries.clear();
}

void SwitchVpp::vpp_fdb_entries_invalidate_by_bd(uint32_t bd_id)
{
    SWSS_LOG_ENTER();
    size_t before = m_vpp_fdb_entries.size();
    for (auto it = m_vpp_fdb_entries.begin(); it != m_vpp_fdb_entries.end(); )
    {
        if (it->first.bd_id == bd_id)
            it = m_vpp_fdb_entries.erase(it);
        else
            ++it;
    }
    SWSS_LOG_INFO("FDB: invalidated by bd_id=%u, removed %zu entries, %zu remain",
                  bd_id, before - m_vpp_fdb_entries.size(), m_vpp_fdb_entries.size());
}

void SwitchVpp::vpp_fdb_entries_invalidate_by_port(sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();
    size_t before = m_vpp_fdb_entries.size();
    for (auto it = m_vpp_fdb_entries.begin(); it != m_vpp_fdb_entries.end(); )
    {
        if (resolvePortIdFromSwIfIndex(it->second) == port_id)
            it = m_vpp_fdb_entries.erase(it);
        else
            ++it;
    }
    SWSS_LOG_INFO("FDB: invalidated by port_id=%s, removed %zu entries, %zu remain",
                  sai_serialize_object_id(port_id).c_str(),
                  before - m_vpp_fdb_entries.size(), m_vpp_fdb_entries.size());
}
