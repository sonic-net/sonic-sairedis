#include "Sai.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(Sai, bulkGet)
{
    Sai sai;

    sai_object_id_t oids[1] = {0};
    uint32_t attrcount[1] = {0};
    sai_attribute_t* attrs[1] = {0};
    sai_status_t statuses[1] = {0};

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                1,
                oids,
                attrcount,
                attrs,
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses));
}

