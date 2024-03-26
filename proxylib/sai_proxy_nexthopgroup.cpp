#include "sai_proxy.h"

PROXY_BULK_CREATE(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
PROXY_BULK_REMOVE(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
PROXY_BULK_GET(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
PROXY_BULK_SET(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
PROXY_GENERIC_QUAD(NEXT_HOP_GROUP,next_hop_group);
PROXY_GENERIC_QUAD(NEXT_HOP_GROUP_MEMBER,next_hop_group_member);
PROXY_GENERIC_QUAD(NEXT_HOP_GROUP_MAP,next_hop_group_map);

const sai_next_hop_group_api_t proxy_next_hop_group_api = {

    PROXY_GENERIC_QUAD_API(next_hop_group)
    PROXY_GENERIC_QUAD_API(next_hop_group_member)

    proxy_bulk_create_next_hop_group_members,
    proxy_bulk_remove_next_hop_group_members,
    PROXY_GENERIC_QUAD_API(next_hop_group_map)
    proxy_bulk_set_next_hop_group_members,
    proxy_bulk_get_next_hop_group_members
};
