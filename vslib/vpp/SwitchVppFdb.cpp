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

        //Set interface state up
        interface_set_state(hw_ifname, true);
    }
    else if (tagging_mode == SAI_VLAN_TAGGING_MODE_UNTAGGED)
    {
        hw_ifname = hwifname;

        //Create bridge and set the l2 port
        set_sw_interface_l2_bridge(hw_ifname,bridge_id, true, VPP_API_PORT_TYPE_NORMAL);

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
    }
    else if (tagging_mode == SAI_VLAN_TAGGING_MODE_TAGGED)
    {

        // set interface l2 tag-rewrite GigabitEthernet0/8/0.200 disable
        snprintf(host_subifname, sizeof(host_subifname), "%s.%u", hw_ifname, vlan_id);
        hw_ifname = host_subifname;
        // Remove the l2 port from bridge
        set_sw_interface_l2_bridge(hw_ifname, bridge_id, false, VPP_API_PORT_TYPE_NORMAL);

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

    // Set mode and lb. SONiC config does not have provision to pass mode and load balancing algorithm
    mode = VPP_BOND_API_MODE_XOR;
    lb = VPP_BOND_API_LB_ALGO_L34;

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
    return vpp_create_lag_member(attr_count, attr_list);
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
    uint32_t bond_id;
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

    if (!bond_info.lcp_created) {
        // create tap and lcp for the Bond intf after first member is added to ensure tap mac = member mac = bond mac
        std::ostringstream tap_stream;
        bond_id = bond_info.id;
        tap_stream << "be" << bond_id;
        std::string tap = tap_stream.str();

        const char *hw_ifname;
        hw_ifname = vpp_get_swif_name(bond_if_idx);
        configure_lcp_interface(hw_ifname, tap.c_str(), true);

        // add tc filter to redirect traffic from tap to PortChannel
        std::stringstream cmd;
        std::string res;

        cmd << "tc qdisc add dev be" << bond_id << " ingress";
        ret = swss::exec(cmd.str(), res);
        if (ret) {
            SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        }

        cmd.str("");
        cmd.clear();
        cmd << "tc filter add dev be" << bond_id << " parent ffff: protocol all prio 2 u32 match u32 0 0 flowid 1:1 action mirred ingress redirect dev PortChannel" << bond_id;
        ret = swss::exec(cmd.str(), res);
        if (ret) {
            SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        }

        // update the lag to bond map
        bond_info.lcp_created = true;
        m_lag_bond_map[lag_oid] = bond_info;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeLagMember(
        _In_ sai_object_id_t lag_member_oid)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS_QUIET(vpp_remove_lag_member(lag_member_oid));

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
            }
            break;

        case FLUSH_BY_INTERFACE | FLUSH_BY_BD_ID:
        case FLUSH_ALL:
        case FLUSH_BY_INTERFACE| FLUSH_BY_BD_ID| FLUSH_ALL: /*flush all*/
            {
                auto ret = l2fib_flush_all();
                SWSS_LOG_NOTICE(" Flush ALL fdb entry ret_val: %d", ret);
            }
            break;

        default:
            SWSS_LOG_ERROR(" Unable to find attrs for FDB_FLUSH %d", mode);
            return SAI_STATUS_FAILURE;
            break;

    }

    return SAI_STATUS_SUCCESS;
}
