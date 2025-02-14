#include "SwitchVpp.h"
#include "SwitchVppNexthop.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"
#include "meta/NotificationPortStateChange.h"

#include "swss/logger.h"
#include "swss/exec.h"
#include "swss/converter.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#include "vppxlate/SaiVppXlate.h"

#include <list>

using namespace saivs;

#define CHECK_STATUS_QUIET(status) {                        \
    sai_status_t _status = (status);                        \
    if (_status != SAI_STATUS_SUCCESS) { return _status; } }

sai_status_t SwitchVpp::IpRouteNexthopGroupEntry(
    _In_ sai_object_id_t next_hop_grp_oid,
    _Out_ nexthop_grp_config_t **nxthop_group)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    int32_t group_type;
    auto nhg_soid = sai_serialize_object_id(next_hop_grp_oid);

    attr.id = SAI_NEXT_HOP_GROUP_ATTR_TYPE;
    auto nhg_obj = get_sai_object(SAI_OBJECT_TYPE_NEXT_HOP_GROUP, nhg_soid);
    if (!nhg_obj) {
        SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_NEXT_HOP_GROUP SaiObject: %s", nhg_soid.c_str());
        return SAI_STATUS_FAILURE;
    }

    CHECK_STATUS_QUIET(nhg_obj->get_mandatory_attr(attr));
    if (attr.value.s32 != SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP &&
        attr.value.s32 != SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_ORDERED_ECMP) {
        SWSS_LOG_ERROR("Unsupported type (%d) in nexthop group %s", attr.value.s32, nhg_soid.c_str());
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    group_type = attr.value.s32;
    auto member_map = nhg_obj->get_child_objs(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER);
    if (member_map == nullptr || member_map->size() == 0) {
        SWSS_LOG_INFO("Empty nexthop_group. OID %s",
            nhg_soid.c_str());
        return SAI_STATUS_FAILURE;
    }

    uint32_t next_hop_sequence = 0;
    std::map<uint32_t, nexthop_grp_member_t> nh_member_map;
    for (auto pair : *member_map) {
        auto member_obj = pair.second;
        sai_object_id_t next_hop_oid;
        uint32_t next_hop_weight = 1;
        nexthop_grp_member_t mbr;

        attr.id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID;
        CHECK_STATUS_QUIET(member_obj->get_mandatory_attr(attr));
        next_hop_oid = attr.value.oid;

        attr.id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_WEIGHT;
        member_obj->get_attr(attr);
        if (member_obj->get_attr(attr) == SAI_STATUS_SUCCESS)
        {
            next_hop_weight = attr.value.u32;
        }

        if (group_type == SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_ORDERED_ECMP) {
            attr.id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_SEQUENCE_ID;
            CHECK_STATUS_QUIET(member_obj->get_mandatory_attr(attr));
            next_hop_sequence = attr.value.u32;
        }
        else {
            // sequence_id will not be set if it is not ordered_ecmp. Then just use the order of the member.
            next_hop_sequence++;
        }

        if(fillNHGrpMember(&mbr, next_hop_oid, next_hop_weight, next_hop_sequence) != SAI_STATUS_SUCCESS) {
            return SAI_STATUS_FAILURE;
        }
        nh_member_map[next_hop_sequence] = mbr;
    }
    nexthop_grp_config_t *nxthop_grp_cfg;

    size_t grp_size = sizeof(nexthop_grp_config_t) + (nh_member_map.size() * sizeof(nexthop_grp_member_t));

    nxthop_grp_cfg = (nexthop_grp_config_t *) calloc(1, grp_size);
    if (nxthop_grp_cfg == NULL) {
        SWSS_LOG_ERROR("Failed to allocate memory for nxthop_grp_cfg. member size %zu", nh_member_map.size());
        return SAI_STATUS_FAILURE;
    }
    nexthop_grp_member_t *nxt_grp_member;
    nxt_grp_member = nxthop_grp_cfg->grp_members;
    for (auto pair : nh_member_map) {
        *nxt_grp_member = pair.second;
        nxt_grp_member++;
    }
    nxthop_grp_cfg->grp_type = group_type;
    nxthop_grp_cfg->nmembers = (uint32_t) nh_member_map.size();

    *nxthop_group = nxthop_grp_cfg;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::IpRouteNexthopEntry(
        _In_ sai_object_id_t next_hop_oid,
        _Out_ nexthop_grp_config_t **nxthop_group_cfg)
{
    SWSS_LOG_ENTER();

    sai_status_t                 status;
    nexthop_grp_config_t        *nxthop_group;

    nxthop_group = (nexthop_grp_config_t *)
      calloc(1, sizeof(nexthop_grp_config_t) + (1 * sizeof(nexthop_grp_member_t)));
    if (!nxthop_group) {
        SWSS_LOG_ERROR("Failed to allocate memory for nxthop_grp_cfg. member size 1");
        return SAI_STATUS_FAILURE;
    }
    nxthop_group->nmembers = 1;

    nexthop_grp_member_t *nxt_grp_member = nxthop_group->grp_members;

    status = fillNHGrpMember(nxt_grp_member, next_hop_oid, 1, 0);

    if (status != SAI_STATUS_SUCCESS) {
        free(nxthop_group);
        return status;
    }

    *nxthop_group_cfg = nxthop_group;
    return SAI_STATUS_SUCCESS;
}

// This function is responsible for filling the nexthop group member structure
// with the necessary information such as the next hop IP address, weight, sequence,
// and router interface or tunnel interface information.
// It takes the next hop object ID, next hop weight, and next hop sequence as input
// and retrieves the required attributes from the next hop object.
// The function returns SAI_STATUS_SUCCESS if the member is filled successfully,
// otherwise it returns an appropriate error status.
sai_status_t
SwitchVpp::fillNHGrpMember(nexthop_grp_member_t *nxt_grp_member, sai_object_id_t next_hop_oid, uint32_t next_hop_weight, uint32_t next_hop_sequence)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    auto nh_soid = sai_serialize_object_id(next_hop_oid);

    if (SAI_OBJECT_TYPE_NEXT_HOP != RealObjectIdManager::objectTypeQuery(next_hop_oid)) {
        SWSS_LOG_ERROR("Not a SAI_OBJECT_TYPE_NEXT_HOP: %s", nh_soid.c_str());
        return SAI_STATUS_FAILURE;
    }

    auto nh_obj = get_sai_object(SAI_OBJECT_TYPE_NEXT_HOP, nh_soid);
    if (!nh_obj) {
        SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_NEXT_HOP SaiObject: %s", nh_soid.c_str());
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_NEXT_HOP_ATTR_TYPE;
    CHECK_STATUS_QUIET(nh_obj->get_mandatory_attr(attr));
    int32_t next_hop_type = attr.value.s32;
    if (next_hop_type != SAI_NEXT_HOP_TYPE_IP && next_hop_type != SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP) {
        return SAI_STATUS_NOT_IMPLEMENTED;
    }

    attr.id = SAI_NEXT_HOP_ATTR_IP;
    sai_ip_address_t ip_address;
    CHECK_STATUS_QUIET(nh_obj->get_mandatory_attr(attr));
    ip_address = attr.value.ipaddr;

    nxt_grp_member->addr = ip_address;
    nxt_grp_member->weight = next_hop_weight;
    nxt_grp_member->seq_id = next_hop_sequence;
    nxt_grp_member->sw_if_index = ~0;

    switch (next_hop_type) {
    case SAI_NEXT_HOP_TYPE_IP:
        attr.id = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID;
        if (get(SAI_OBJECT_TYPE_NEXT_HOP, next_hop_oid, 1, &attr) == SAI_STATUS_SUCCESS) {
            nxt_grp_member->rif_oid = attr.value.oid;
        }
        break;
    case SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP: {
        u_int32_t sw_if_index;
        if (m_tunnel_mgr.get_tunnel_if(next_hop_oid, sw_if_index) == SAI_STATUS_SUCCESS) {
            nxt_grp_member->sw_if_index = sw_if_index;
            SWSS_LOG_INFO("Got tunnel interface %d for nexthop %s", sw_if_index,
                           sai_serialize_object_id(next_hop_oid).c_str());
        } else {
            SWSS_LOG_ERROR("Failed to get tunnel interface name for nexthop %s",
                           sai_serialize_object_id(next_hop_oid).c_str());
            return SAI_STATUS_FAILURE;
        }
        break;
    }
    default:
        break;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t
SwitchVpp::createNexthop(
                _In_ const std::string& serializedObjectId,
                _In_ sai_object_id_t switch_id,
                _In_ uint32_t attr_count,
                _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_value_t     *next_hop_type;
    uint32_t                        attr_index;

    CHECK_STATUS(find_attrib_in_list(attr_count, attr_list, SAI_NEXT_HOP_ATTR_TYPE,
                                 &next_hop_type, &attr_index));
    if (next_hop_type->s32 == SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP) {
        //Deligate the creation of tunnel encap nexthop to tunnel manager
        CHECK_STATUS(m_tunnel_mgr.create_tunnel_encap_nexthop(serializedObjectId, switch_id, attr_count, attr_list));
    }
    return create_internal(SAI_OBJECT_TYPE_NEXT_HOP, serializedObjectId, switch_id, attr_count, attr_list);
}

sai_status_t SwitchVpp::removeNexthop(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_attribute_t                 attr;
    sai_status_t                    status;

    auto nh_obj = get_sai_object(SAI_OBJECT_TYPE_NEXT_HOP, serializedObjectId);

    if (!nh_obj) {
        SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_NEXT_HOP SaiObject: %s", serializedObjectId.c_str());
    } else {
        attr.id = SAI_NEXT_HOP_ATTR_TYPE;
        status = nh_obj->get_attr(attr);
        if(status != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Missing SAI_NEXT_HOP_ATTR_TYPE in %s", serializedObjectId.c_str());
        }
        else if (attr.value.s32 == SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP) {
            CHECK_STATUS(m_tunnel_mgr.remove_tunnel_encap_nexthop(serializedObjectId));
        }
    }

    return remove_internal(SAI_OBJECT_TYPE_NEXT_HOP, serializedObjectId);
}

sai_status_t
SwitchVpp::createNexthopGroupMember(
                _In_ const std::string& serializedObjectId,
                _In_ sai_object_id_t switch_id,
                _In_ uint32_t attr_count,
                _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t        status;
    sai_attribute_t     attr;

    SaiCachedObject nhg_mbr_obj(this, SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, serializedObjectId, attr_count, attr_list);
    attr.id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID;
    CHECK_STATUS_QUIET(nhg_mbr_obj.get_mandatory_attr(attr));
    SWSS_LOG_INFO("Creating NHG member %s in nhg %s", serializedObjectId.c_str(), sai_serialize_object_id(attr.value.oid).c_str());
    auto nhg_obj = nhg_mbr_obj.get_linked_object(SAI_OBJECT_TYPE_NEXT_HOP_GROUP, SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID);
    if (nhg_obj == nullptr) {
        SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_NEXT_HOP_GROUP from %s", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }

    //call create_internal to update the mapping from NHG to NHG_MBRs, which is used to update the routes
    status = create_internal(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, serializedObjectId, switch_id, attr_count, attr_list);
    if (status != SAI_STATUS_SUCCESS) {
        return status;
    }

    auto routes = nhg_obj->get_child_objs(SAI_OBJECT_TYPE_ROUTE_ENTRY);
    if (routes == nullptr) {
        return SAI_STATUS_SUCCESS;
    }

    for (auto route : *routes) {
        SWSS_LOG_INFO("NHG member changed. Updating route %s", route.first.c_str());
        IpRouteAddRemove(route.second.get(), false);
        IpRouteAddRemove(route.second.get(), true);
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t
SwitchVpp::removeNexthopGroupMember(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_status_t        status;
    sai_attribute_t     attr;

    auto nhg_mbr_obj = get_sai_object(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, serializedObjectId);
    if (!nhg_mbr_obj) {
        SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER SaiObject: %s", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }
    attr.id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID;
    CHECK_STATUS_QUIET(nhg_mbr_obj->get_mandatory_attr(attr));
    SWSS_LOG_INFO("Deleting NHG member %s from nhg %s", serializedObjectId.c_str(), sai_serialize_object_id(attr.value.oid).c_str());

    auto nhg_obj = nhg_mbr_obj->get_linked_object(SAI_OBJECT_TYPE_NEXT_HOP_GROUP, SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID);
    if (nhg_obj == nullptr) {
        SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_NEXT_HOP_GROUP from %s", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }

    auto routes = nhg_obj->get_child_objs(SAI_OBJECT_TYPE_ROUTE_ENTRY);

    //call remove_internal to update the mapping from NHG to NHG_MBRs
    status = remove_internal(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, serializedObjectId);
    if (status != SAI_STATUS_SUCCESS) {
        return status;
    }

    if (routes == nullptr) {
        return SAI_STATUS_SUCCESS;
    }

    for (auto route : *routes) {
        SWSS_LOG_INFO("NHG member changed. Updating route %s", route.first.c_str());
        IpRouteAddRemove(route.second.get(), false);
        IpRouteAddRemove(route.second.get(), true);
    }
    return SAI_STATUS_SUCCESS;
}
