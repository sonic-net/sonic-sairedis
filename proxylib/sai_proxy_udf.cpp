#include "sai_proxy.h"

PROXY_GENERIC_QUAD(UDF,udf)
PROXY_GENERIC_QUAD(UDF_MATCH,udf_match)
PROXY_GENERIC_QUAD(UDF_GROUP,udf_group)

const sai_udf_api_t proxy_udf_api = {

    PROXY_GENERIC_QUAD_API(udf)
    PROXY_GENERIC_QUAD_API(udf_match)
    PROXY_GENERIC_QUAD_API(udf_group)
};
