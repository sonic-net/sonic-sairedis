#include "sai_stub.h"

STUB_GENERIC_QUAD(ISOLATION_GROUP,isolation_group);
STUB_GENERIC_QUAD(ISOLATION_GROUP_MEMBER,isolation_group_member);

const sai_isolation_group_api_t stub_isolation_group_api = {

    STUB_GENERIC_QUAD_API(isolation_group)
    STUB_GENERIC_QUAD_API(isolation_group_member)
};
