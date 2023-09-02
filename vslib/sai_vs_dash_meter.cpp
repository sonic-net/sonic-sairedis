#include "sai_vs.h"

VS_GENERIC_QUAD(METER_BUCKET, meter_bucket);
VS_BULK_CREATE(METER_BUCKET, meter_buckets);
VS_BULK_REMOVE(METER_BUCKET, meter_buckets);

VS_GENERIC_QUAD(METER_BUCKET, meter_policy);
VS_BULK_CREATE(METER_BUCKET, meter_policys);
VS_BULK_REMOVE(METER_BUCKET, meter_policys);

VS_GENERIC_QUAD(METER_BUCKET, meter_rule);
VS_BULK_CREATE(METER_BUCKET, meter_rules);
VS_BULK_REMOVE(METER_BUCKET, meter_rules);

const sai_dash_meter_api_t vs_dash_meter_api = {

    VS_GENERIC_QUAD_API(meter_bucket)
    vs_bulk_create_meter_buckets,
    vs_bulk_remove_meter_buckets,

    VS_GENERIC_QUAD_API(meter_policy)
    vs_bulk_create_meter_policys,
    vs_bulk_remove_meter_policys,

    VS_GENERIC_QUAD_API(meter_rule)
    vs_bulk_create_meter_rules,
    vs_bulk_remove_meter_rules,
};
