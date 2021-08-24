#include "sai_vs.h"

VS_GENERIC_QUAD(ISOLATION_GROUP,isolation_group);
VS_GENERIC_QUAD(ISOLATION_GROUP_MEMBER,isolation_group_member);

const sai_isolation_group_api_t vs_isolation_group_api = {

    VS_GENERIC_QUAD_API(isolation_group)
    VS_GENERIC_QUAD_API(isolation_group_member)
};

