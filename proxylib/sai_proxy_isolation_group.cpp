#include "sai_proxy.h"

PROXY_GENERIC_QUAD(ISOLATION_GROUP,isolation_group);
PROXY_GENERIC_QUAD(ISOLATION_GROUP_MEMBER,isolation_group_member);

const sai_isolation_group_api_t proxy_isolation_group_api = {

    PROXY_GENERIC_QUAD_API(isolation_group)
    PROXY_GENERIC_QUAD_API(isolation_group_member)
};
