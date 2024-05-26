#include "sai_stub.h"

STUB_GENERIC_QUAD(L2MC_GROUP,l2mc_group);
STUB_GENERIC_QUAD(L2MC_GROUP_MEMBER,l2mc_group_member);

const sai_l2mc_group_api_t stub_l2mc_group_api = {

    STUB_GENERIC_QUAD_API(l2mc_group)
    STUB_GENERIC_QUAD_API(l2mc_group_member)
};
