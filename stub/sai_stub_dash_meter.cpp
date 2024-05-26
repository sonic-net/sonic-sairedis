#include "sai_stub.h"

STUB_GENERIC_QUAD(METER_BUCKET, meter_bucket);
STUB_BULK_CREATE(METER_BUCKET, meter_buckets);
STUB_BULK_REMOVE(METER_BUCKET, meter_buckets);

STUB_GENERIC_QUAD(METER_BUCKET, meter_policy);
STUB_BULK_CREATE(METER_BUCKET, meter_policys);
STUB_BULK_REMOVE(METER_BUCKET, meter_policys);

STUB_GENERIC_QUAD(METER_BUCKET, meter_rule);
STUB_BULK_CREATE(METER_BUCKET, meter_rules);
STUB_BULK_REMOVE(METER_BUCKET, meter_rules);

const sai_dash_meter_api_t stub_dash_meter_api = {

    STUB_GENERIC_QUAD_API(meter_bucket)
    stub_bulk_create_meter_buckets,
    stub_bulk_remove_meter_buckets,

    STUB_GENERIC_QUAD_API(meter_policy)
    stub_bulk_create_meter_policys,
    stub_bulk_remove_meter_policys,

    STUB_GENERIC_QUAD_API(meter_rule)
    stub_bulk_create_meter_rules,
    stub_bulk_remove_meter_rules,
};
