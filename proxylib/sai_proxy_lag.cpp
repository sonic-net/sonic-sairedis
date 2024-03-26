#include "sai_proxy.h"

PROXY_BULK_CREATE(LAG_MEMBER,lag_members);
PROXY_BULK_REMOVE(LAG_MEMBER,lag_members);

PROXY_GENERIC_QUAD(LAG,lag);
PROXY_GENERIC_QUAD(LAG_MEMBER,lag_member);

const sai_lag_api_t proxy_lag_api = {

    PROXY_GENERIC_QUAD_API(lag)
    PROXY_GENERIC_QUAD_API(lag_member)

    proxy_bulk_create_lag_members,
    proxy_bulk_remove_lag_members,
};
