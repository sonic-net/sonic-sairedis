#include "sai_proxy.h"

PROXY_GENERIC_QUAD(IPMC_GROUP,ipmc_group);
PROXY_GENERIC_QUAD(IPMC_GROUP_MEMBER,ipmc_group_member);

const sai_ipmc_group_api_t proxy_ipmc_group_api = {

    PROXY_GENERIC_QUAD_API(ipmc_group)
    PROXY_GENERIC_QUAD_API(ipmc_group_member)
};
