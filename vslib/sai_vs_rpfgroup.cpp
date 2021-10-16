#include "sai_vs.h"

VS_GENERIC_QUAD(RPF_GROUP,rpf_group);
VS_GENERIC_QUAD(RPF_GROUP_MEMBER,rpf_group_member);

const sai_rpf_group_api_t vs_rpf_group_api = {

    VS_GENERIC_QUAD_API(rpf_group)
    VS_GENERIC_QUAD_API(rpf_group_member)
};
