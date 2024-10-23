#include "SaiInterface.h"
#include "DummySaiInterface.h"

#include <gtest/gtest.h>

#include <memory>

#include "swss/logger.h"

using namespace saimeta;
using namespace sairedis;

static sai_object_type_t objects_types_to_verify[] {
    SAI_OBJECT_TYPE_SWITCH,
    SAI_OBJECT_TYPE_FDB_ENTRY,
    SAI_OBJECT_TYPE_ROUTE_ENTRY,
    SAI_OBJECT_TYPE_NEIGHBOR_ENTRY,
    SAI_OBJECT_TYPE_NAT_ENTRY,
    SAI_OBJECT_TYPE_INSEG_ENTRY,
    SAI_OBJECT_TYPE_MY_SID_ENTRY,
    (sai_object_type_t)SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY,
    (sai_object_type_t)SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY,
    (sai_object_type_t)SAI_OBJECT_TYPE_VIP_ENTRY,
    (sai_object_type_t)SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY,
    (sai_object_type_t)SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY,
    (sai_object_type_t)SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY,
    (sai_object_type_t)SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY,
};

TEST(SaiInterface, create)
{
    DummySaiInterface ds;

    SaiInterface *sai = &ds;

    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_NULL, .objectkey = { .key = { .object_id = 0 } } };

    for (auto ot: objects_types_to_verify)
    {
        mk.objecttype = ot;
        EXPECT_EQ(SAI_STATUS_SUCCESS, sai->create(mk, 0, 0, nullptr));
    }

    mk.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    EXPECT_EQ(SAI_STATUS_FAILURE, sai->create(mk, 0, 0, nullptr));
}

TEST(SaiInterface, remove)
{
    DummySaiInterface ds;

    SaiInterface *sai = &ds;

    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_NULL, .objectkey = { .key = { .object_id = 0 } } };

    EXPECT_EQ(SAI_STATUS_FAILURE, sai->remove(mk));

    for (auto ot: objects_types_to_verify)
    {
        mk.objecttype = ot;
        EXPECT_EQ(SAI_STATUS_SUCCESS, sai->remove(mk));
    }

    mk.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    EXPECT_EQ(SAI_STATUS_FAILURE, sai->remove(mk));
}

TEST(SaiInterface, set)
{
    DummySaiInterface ds;

    SaiInterface *sai = &ds;

    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_NULL, .objectkey = { .key = { .object_id = 0 } } };

    EXPECT_EQ(SAI_STATUS_FAILURE, sai->set(mk, nullptr));

    for (auto ot: objects_types_to_verify)
    {
        mk.objecttype = ot;
        EXPECT_EQ(SAI_STATUS_SUCCESS, sai->set(mk, nullptr));
    }

    mk.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    EXPECT_EQ(SAI_STATUS_FAILURE, sai->set(mk, nullptr));
}

TEST(SaiInterface, get)
{
    DummySaiInterface ds;

    SaiInterface *sai = &ds;

    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_NULL, .objectkey = { .key = { .object_id = 0 } } };

    EXPECT_EQ(SAI_STATUS_FAILURE, sai->get(mk, 0, nullptr));

    for (auto ot: objects_types_to_verify)
    {
        mk.objecttype = ot;
        EXPECT_EQ(SAI_STATUS_SUCCESS, sai->get(mk, 0, nullptr));
    }

    mk.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    EXPECT_EQ(SAI_STATUS_FAILURE, sai->get(mk, 0, nullptr));
}

TEST(SaiInterface, stats_meter_bucket_entry)
{
    DummySaiInterface ds;

    SaiInterface *s = &ds;

    const sai_meter_bucket_entry_t *m = nullptr;

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, s->getStats(m, 0, 0, 0));
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, s->getStatsExt(m, 0, nullptr, SAI_STATS_MODE_READ, nullptr));
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, s->clearStats(m, 0, nullptr));
}
