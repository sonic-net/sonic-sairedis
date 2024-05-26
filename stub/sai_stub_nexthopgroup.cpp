#include "sai_stub.h"

STUB_BULK_CREATE(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
STUB_BULK_REMOVE(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
STUB_BULK_GET(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
STUB_BULK_SET(NEXT_HOP_GROUP_MEMBER,next_hop_group_members);
STUB_GENERIC_QUAD(NEXT_HOP_GROUP,next_hop_group);
STUB_GENERIC_QUAD(NEXT_HOP_GROUP_MEMBER,next_hop_group_member);
STUB_GENERIC_QUAD(NEXT_HOP_GROUP_MAP,next_hop_group_map);

const sai_next_hop_group_api_t stub_next_hop_group_api = {

    STUB_GENERIC_QUAD_API(next_hop_group)
    STUB_GENERIC_QUAD_API(next_hop_group_member)

    stub_bulk_create_next_hop_group_members,
    stub_bulk_remove_next_hop_group_members,
    STUB_GENERIC_QUAD_API(next_hop_group_map)
    stub_bulk_set_next_hop_group_members,
    stub_bulk_get_next_hop_group_members
};
