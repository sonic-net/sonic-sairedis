#include "sai_stub.h"

STUB_GENERIC_QUAD(RPF_GROUP,rpf_group);
STUB_GENERIC_QUAD(RPF_GROUP_MEMBER,rpf_group_member);

const sai_rpf_group_api_t stub_rpf_group_api = {

    STUB_GENERIC_QUAD_API(rpf_group)
    STUB_GENERIC_QUAD_API(rpf_group_member)
};
