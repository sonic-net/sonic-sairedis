#include "sai_redis.h"

REDIS_GENERIC_QUAD(METER_BUCKET, meter_bucket);
REDIS_BULK_CREATE(METER_BUCKET, meter_buckets);
REDIS_BULK_REMOVE(METER_BUCKET, meter_buckets);

REDIS_GENERIC_QUAD(METER_BUCKET, meter_policy);
REDIS_BULK_CREATE(METER_BUCKET, meter_policys);
REDIS_BULK_REMOVE(METER_BUCKET, meter_policys);

REDIS_GENERIC_QUAD(METER_BUCKET, meter_rule);
REDIS_BULK_CREATE(METER_BUCKET, meter_rules);
REDIS_BULK_REMOVE(METER_BUCKET, meter_rules);

const sai_dash_meter_api_t redis_dash_meter_api = {

    REDIS_GENERIC_QUAD_API(meter_bucket)
    redis_bulk_create_meter_buckets,
    redis_bulk_remove_meter_buckets,

    REDIS_GENERIC_QUAD_API(meter_policy)
    redis_bulk_create_meter_policys,
    redis_bulk_remove_meter_policys,

    REDIS_GENERIC_QUAD_API(meter_rule)
    redis_bulk_create_meter_rules,
    redis_bulk_remove_meter_rules,
};
