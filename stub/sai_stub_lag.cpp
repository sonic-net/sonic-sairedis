#include "sai_stub.h"

STUB_BULK_CREATE(LAG_MEMBER,lag_members);
STUB_BULK_REMOVE(LAG_MEMBER,lag_members);

STUB_GENERIC_QUAD(LAG,lag);
STUB_GENERIC_QUAD(LAG_MEMBER,lag_member);

const sai_lag_api_t stub_lag_api = {

    STUB_GENERIC_QUAD_API(lag)
    STUB_GENERIC_QUAD_API(lag_member)

    stub_bulk_create_lag_members,
    stub_bulk_remove_lag_members,
};
