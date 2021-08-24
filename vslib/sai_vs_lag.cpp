#include "sai_vs.h"

VS_BULK_CREATE(LAG_MEMBER,lag_members);
VS_BULK_REMOVE(LAG_MEMBER,lag_members);

VS_GENERIC_QUAD(LAG,lag);
VS_GENERIC_QUAD(LAG_MEMBER,lag_member);

const sai_lag_api_t vs_lag_api = {

    VS_GENERIC_QUAD_API(lag)
    VS_GENERIC_QUAD_API(lag_member)

    vs_bulk_create_lag_members,
    vs_bulk_remove_lag_members,
};
