#include "DummySaiInterface.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(DummySaiInterface, queryApiVersion)
{
    DummySaiInterface sai;

    sai.apiInitialize(0,0);

    sai_api_version_t version;

    EXPECT_EQ(sai.queryApiVersion(NULL), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_SUCCESS);
}

TEST(DummySaiInterface, bulkGet)
{
    DummySaiInterface sai;

    sai.apiInitialize(0,0);

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

TEST(DummySaiInterface, create)
{
    DummySaiInterface sai;

    sai.apiInitialize(0,0);

    sai_object_id_t oid;
    sai.create(SAI_OBJECT_TYPE_SWITCH,&oid, 0, 0, 0);

    EXPECT_NE(oid, SAI_NULL_OBJECT_ID);
}
