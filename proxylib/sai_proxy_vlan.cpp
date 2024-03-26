#include "sai_proxy.h"

PROXY_BULK_CREATE(VLAN_MEMBER,vlan_members);
PROXY_BULK_REMOVE(VLAN_MEMBER,vlan_members);
PROXY_GENERIC_QUAD(VLAN,vlan);
PROXY_GENERIC_QUAD(VLAN_MEMBER,vlan_member);
PROXY_GENERIC_STATS(VLAN,vlan);

const sai_vlan_api_t proxy_vlan_api = {

    PROXY_GENERIC_QUAD_API(vlan)
    PROXY_GENERIC_QUAD_API(vlan_member)

    proxy_bulk_create_vlan_members,
    proxy_bulk_remove_vlan_members,

    PROXY_GENERIC_STATS_API(vlan)
};
