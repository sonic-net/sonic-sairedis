#include "sai_proxy.h"

PROXY_GENERIC_QUAD(RPF_GROUP,rpf_group);
PROXY_GENERIC_QUAD(RPF_GROUP_MEMBER,rpf_group_member);

const sai_rpf_group_api_t proxy_rpf_group_api = {

    PROXY_GENERIC_QUAD_API(rpf_group)
    PROXY_GENERIC_QUAD_API(rpf_group_member)
};
