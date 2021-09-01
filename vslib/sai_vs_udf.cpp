#include "sai_vs.h"

VS_GENERIC_QUAD(UDF,udf)
VS_GENERIC_QUAD(UDF_MATCH,udf_match)
VS_GENERIC_QUAD(UDF_GROUP,udf_group)

const sai_udf_api_t vs_udf_api = {

    VS_GENERIC_QUAD_API(udf)
    VS_GENERIC_QUAD_API(udf_match)
    VS_GENERIC_QUAD_API(udf_group)
};
