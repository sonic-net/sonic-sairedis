#include "sai_stub.h"

STUB_BULK_CREATE(VLAN_MEMBER,vlan_members);
STUB_BULK_REMOVE(VLAN_MEMBER,vlan_members);
STUB_GENERIC_QUAD(VLAN,vlan);
STUB_GENERIC_QUAD(VLAN_MEMBER,vlan_member);
STUB_GENERIC_STATS(VLAN,vlan);

const sai_vlan_api_t stub_vlan_api = {

    STUB_GENERIC_QUAD_API(vlan)
    STUB_GENERIC_QUAD_API(vlan_member)

    stub_bulk_create_vlan_members,
    stub_bulk_remove_vlan_members,

    STUB_GENERIC_STATS_API(vlan)
};
