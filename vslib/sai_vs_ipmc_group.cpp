#include "sai_vs.h"

VS_GENERIC_QUAD(IPMC_GROUP,ipmc_group);
VS_GENERIC_QUAD(IPMC_GROUP_MEMBER,ipmc_group_member);

const sai_ipmc_group_api_t vs_ipmc_group_api = {

    VS_GENERIC_QUAD_API(ipmc_group)
    VS_GENERIC_QUAD_API(ipmc_group_member)
};
