#include "sai_redis.h"

REDIS_BULK_CREATE(VLAN_MEMBER,vlan_members);
REDIS_BULK_REMOVE(VLAN_MEMBER,vlan_members);
REDIS_GENERIC_QUAD(VLAN,vlan);
REDIS_GENERIC_QUAD(VLAN_MEMBER,vlan_member);
REDIS_GENERIC_STATS(VLAN,vlan);

const sai_vlan_api_t redis_vlan_api = {

    REDIS_GENERIC_QUAD_API(vlan)
    REDIS_GENERIC_QUAD_API(vlan_member)

    redis_bulk_create_vlan_members,
    redis_bulk_remove_vlan_members,

    REDIS_GENERIC_STATS_API(vlan)
};
