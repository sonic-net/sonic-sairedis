#include "sai_proxy.h"

PROXY_GENERIC_QUAD(L2MC_GROUP,l2mc_group);
PROXY_GENERIC_QUAD(L2MC_GROUP_MEMBER,l2mc_group_member);

const sai_l2mc_group_api_t proxy_l2mc_group_api = {

    PROXY_GENERIC_QUAD_API(l2mc_group)
    PROXY_GENERIC_QUAD_API(l2mc_group_member)
};
