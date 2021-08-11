#include "sai_vs.h"

VS_BULK_CREATE(VLAN_MEMBER,vlan_members);
VS_BULK_REMOVE(VLAN_MEMBER,vlan_members);

VS_GENERIC_QUAD(VLAN,vlan);
VS_GENERIC_QUAD(VLAN_MEMBER,vlan_member);
VS_GENERIC_STATS(VLAN,vlan);

const sai_vlan_api_t vs_vlan_api = {

    VS_GENERIC_QUAD_API(vlan)
    VS_GENERIC_QUAD_API(vlan_member)

    vs_bulk_create_vlan_members,
    vs_bulk_remove_vlan_members,

    VS_GENERIC_STATS_API(vlan)
};
