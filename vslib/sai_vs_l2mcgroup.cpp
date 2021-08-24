#include "sai_vs.h"

VS_GENERIC_QUAD(L2MC_GROUP,l2mc_group);
VS_GENERIC_QUAD(L2MC_GROUP_MEMBER,l2mc_group_member);

const sai_l2mc_group_api_t vs_l2mc_group_api = {

    VS_GENERIC_QUAD_API(l2mc_group)
    VS_GENERIC_QUAD_API(l2mc_group_member)
};
