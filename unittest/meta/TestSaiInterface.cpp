#include "SaiInterface.h"
#include "DummySaiInterface.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;
using namespace sairedis;

TEST(SaiInterface, create)
{
    DummySaiInterface ds;

    SaiInterface *sai = &ds;

    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_NULL, .objectkey = { .key = { .object_id = 0 } } };

    EXPECT_EQ(SAI_STATUS_FAILURE, sai->create(mk, 0, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_SWITCH;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->create(mk, 0, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_FDB_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->create(mk, 0, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_NAT_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->create(mk, 0, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_INSEG_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->create(mk, 0, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    EXPECT_EQ(SAI_STATUS_FAILURE, sai->create(mk, 0, 0, nullptr));
}

TEST(SaiInterface, remove)
{
    DummySaiInterface ds;

    SaiInterface *sai = &ds;

    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_NULL, .objectkey = { .key = { .object_id = 0 } } };

    EXPECT_EQ(SAI_STATUS_FAILURE, sai->remove(mk));

    mk.objecttype = SAI_OBJECT_TYPE_SWITCH;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->remove(mk));

    mk.objecttype = SAI_OBJECT_TYPE_FDB_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->remove(mk));

    mk.objecttype = SAI_OBJECT_TYPE_NAT_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->remove(mk));

    mk.objecttype = SAI_OBJECT_TYPE_INSEG_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->remove(mk));

    mk.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    EXPECT_EQ(SAI_STATUS_FAILURE, sai->remove(mk));
}

TEST(SaiInterface, set)
{
    DummySaiInterface ds;

    SaiInterface *sai = &ds;

    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_NULL, .objectkey = { .key = { .object_id = 0 } } };

    EXPECT_EQ(SAI_STATUS_FAILURE, sai->set(mk, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_SWITCH;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->set(mk, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_FDB_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->set(mk, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_NAT_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->set(mk, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_INSEG_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->set(mk, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->set(mk, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_NEIGHBOR_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->set(mk, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    EXPECT_EQ(SAI_STATUS_FAILURE, sai->set(mk, nullptr));
}

TEST(SaiInterface, get)
{
    DummySaiInterface ds;

    SaiInterface *sai = &ds;

    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_NULL, .objectkey = { .key = { .object_id = 0 } } };

    EXPECT_EQ(SAI_STATUS_FAILURE, sai->get(mk, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_SWITCH;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->get(mk, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_FDB_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->get(mk, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_NAT_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->get(mk, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_INSEG_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->get(mk, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->get(mk, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_NEIGHBOR_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sai->get(mk, 0, nullptr));

    mk.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    EXPECT_EQ(SAI_STATUS_FAILURE, sai->get(mk, 0, nullptr));
}
