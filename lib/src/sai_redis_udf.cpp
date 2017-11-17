#include "sai_redis.h"

REDIS_GENERIC_QUAD(UDF,udf)
REDIS_GENERIC_QUAD(UDF_MATCH,udf_match)
REDIS_GENERIC_QUAD(UDF_GROUP,udf_group)

const sai_udf_api_t redis_udf_api = {

    REDIS_GENERIC_QUAD_API(udf)
    REDIS_GENERIC_QUAD_API(udf_match)
    REDIS_GENERIC_QUAD_API(udf_group)
};
