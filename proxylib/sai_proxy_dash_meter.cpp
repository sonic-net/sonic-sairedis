#include "sai_proxy.h"

PROXY_GENERIC_QUAD(METER_BUCKET, meter_bucket);
PROXY_BULK_CREATE(METER_BUCKET, meter_buckets);
PROXY_BULK_REMOVE(METER_BUCKET, meter_buckets);

PROXY_GENERIC_QUAD(METER_BUCKET, meter_policy);
PROXY_BULK_CREATE(METER_BUCKET, meter_policys);
PROXY_BULK_REMOVE(METER_BUCKET, meter_policys);

PROXY_GENERIC_QUAD(METER_BUCKET, meter_rule);
PROXY_BULK_CREATE(METER_BUCKET, meter_rules);
PROXY_BULK_REMOVE(METER_BUCKET, meter_rules);

const sai_dash_meter_api_t proxy_dash_meter_api = {

    PROXY_GENERIC_QUAD_API(meter_bucket)
    proxy_bulk_create_meter_buckets,
    proxy_bulk_remove_meter_buckets,

    PROXY_GENERIC_QUAD_API(meter_policy)
    proxy_bulk_create_meter_policys,
    proxy_bulk_remove_meter_policys,

    PROXY_GENERIC_QUAD_API(meter_rule)
    proxy_bulk_create_meter_rules,
    proxy_bulk_remove_meter_rules,
};
