#include "DummySaiInterface.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(DummySaiInterface, queryApiVersion)
{
    DummySaiInterface sai;

    sai.initialize(0,0);

    sai_api_version_t version;

    EXPECT_EQ(sai.queryApiVersion(NULL), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_SUCCESS);
}
