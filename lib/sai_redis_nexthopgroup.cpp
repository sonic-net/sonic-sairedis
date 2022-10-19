#include "sai_redis.h"

REDIS_BULK_CREATE(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
REDIS_BULK_REMOVE(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
REDIS_BULK_GET(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
REDIS_BULK_SET(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
REDIS_GENERIC_QUAD(NEXT_HOP_GROUP,next_hop_group);
REDIS_GENERIC_QUAD(NEXT_HOP_GROUP_MEMBER,next_hop_group_member);
REDIS_GENERIC_QUAD(NEXT_HOP_GROUP_MAP,next_hop_group_map);

const sai_next_hop_group_api_t redis_next_hop_group_api = {

    REDIS_GENERIC_QUAD_API(next_hop_group)
    REDIS_GENERIC_QUAD_API(next_hop_group_member)

    redis_bulk_create_next_hop_group_members,
    redis_bulk_remove_next_hop_group_members,
    REDIS_GENERIC_QUAD_API(next_hop_group_map)
    redis_bulk_set_next_hop_group_members,
    redis_bulk_get_next_hop_group_members
};
