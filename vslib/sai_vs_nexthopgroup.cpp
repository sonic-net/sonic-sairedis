#include "sai_vs.h"

VS_BULK_CREATE(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
VS_BULK_REMOVE(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);

VS_GENERIC_QUAD(NEXT_HOP_GROUP,next_hop_group);
VS_GENERIC_QUAD(NEXT_HOP_GROUP_MEMBER,next_hop_group_member);
VS_GENERIC_QUAD(NEXT_HOP_GROUP_MAP,next_hop_group_map);

const sai_next_hop_group_api_t vs_next_hop_group_api = {

    VS_GENERIC_QUAD_API(next_hop_group)
    VS_GENERIC_QUAD_API(next_hop_group_member)

    vs_bulk_create_next_hop_group_members,
    vs_bulk_remove_next_hop_group_members,
    VS_GENERIC_QUAD_API(next_hop_group_map)
};
