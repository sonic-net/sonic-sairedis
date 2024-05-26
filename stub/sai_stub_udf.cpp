#include "sai_stub.h"

STUB_GENERIC_QUAD(UDF,udf)
STUB_GENERIC_QUAD(UDF_MATCH,udf_match)
STUB_GENERIC_QUAD(UDF_GROUP,udf_group)

const sai_udf_api_t stub_udf_api = {

    STUB_GENERIC_QUAD_API(udf)
    STUB_GENERIC_QUAD_API(udf_match)
    STUB_GENERIC_QUAD_API(udf_group)
};
