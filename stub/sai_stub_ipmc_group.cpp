#include "sai_stub.h"

STUB_GENERIC_QUAD(IPMC_GROUP,ipmc_group);
STUB_GENERIC_QUAD(IPMC_GROUP_MEMBER,ipmc_group_member);

const sai_ipmc_group_api_t stub_ipmc_group_api = {

    STUB_GENERIC_QUAD_API(ipmc_group)
    STUB_GENERIC_QUAD_API(ipmc_group_member)
};
