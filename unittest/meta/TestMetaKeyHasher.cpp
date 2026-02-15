#include "MetaKeyHasher.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(MetaKeyHasher, operator_eq_route_entry)
{
    sai_route_entry_t ra;
    sai_route_entry_t rb;

    memset(&ra, 0, sizeof(ra));
    memset(&rb, 0, sizeof(ra));

    sai_object_meta_key_t ma = { .objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY, .objectkey = { .key = { .route_entry = ra } } };
    sai_object_meta_key_t mb = { .objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY, .objectkey = { .key = { .route_entry = rb } } };

    MetaKeyHasher mh;

    EXPECT_TRUE(mh.operator()(ma, mb));
}

TEST(MetaKeyHasher, operator_eq_nat_entry)
{
    sai_nat_entry_t ra;
    sai_nat_entry_t rb;

    memset(&ra, 0, sizeof(ra));
    memset(&rb, 0, sizeof(ra));

    sai_object_meta_key_t ma = { .objecttype = SAI_OBJECT_TYPE_NAT_ENTRY, .objectkey = { .key = { .nat_entry = ra } } };
    sai_object_meta_key_t mb = { .objecttype = SAI_OBJECT_TYPE_NAT_ENTRY, .objectkey = { .key = { .nat_entry = rb } } };

    MetaKeyHasher mh;

    EXPECT_TRUE(mh.operator()(ma, mb));
}

TEST(MetaKeyHasher, operator_eq_inseg_entry)
{
    sai_inseg_entry_t ra;
    sai_inseg_entry_t rb;

    memset(&ra, 0, sizeof(ra));
    memset(&rb, 0, sizeof(ra));

    sai_object_meta_key_t ma = { .objecttype = SAI_OBJECT_TYPE_INSEG_ENTRY, .objectkey = { .key = { .inseg_entry = ra } } };
    sai_object_meta_key_t mb = { .objecttype = SAI_OBJECT_TYPE_INSEG_ENTRY, .objectkey = { .key = { .inseg_entry = rb } } };

    MetaKeyHasher mh;

    EXPECT_TRUE(mh.operator()(ma, mb));
}

TEST(MetaKeyHasher, operator_eq)
{
    sai_object_meta_key_t ma;
    sai_object_meta_key_t mb;

    memset(&ma, 0, sizeof(ma));
    memset(&mb, 0, sizeof(mb));

    ma.objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY;
    mb.objecttype = SAI_OBJECT_TYPE_INSEG_ENTRY;

    MetaKeyHasher mh;

    EXPECT_FALSE(mh.operator()(ma, mb));
}

TEST(MetaKeyHasher, operator_hash)
{
    sai_object_meta_key_t ma;

    memset(&ma, 0, sizeof(ma));

    MetaKeyHasher mh;

    ma.objecttype = SAI_OBJECT_TYPE_NAT_ENTRY;

    EXPECT_EQ(mh.operator()(ma), 0);

    ma.objecttype = SAI_OBJECT_TYPE_INSEG_ENTRY;

    EXPECT_EQ(mh.operator()(ma), 0);

    ma.objecttype = SAI_OBJECT_TYPE_IPMC_ENTRY;

    EXPECT_EQ(mh.operator()(ma), 0);

    ma.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;

    EXPECT_EQ(mh.operator()(ma), 0);

    ma.objecttype = SAI_OBJECT_TYPE_IPMC_ENTRY;
    ma.objectkey.key.ipmc_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    ma.objectkey.key.ipmc_entry.source.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

    EXPECT_EQ(mh.operator()(ma), 0);

    ma.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    ma.objectkey.key.l2mc_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    ma.objectkey.key.l2mc_entry.source.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

    EXPECT_EQ(mh.operator()(ma), 0);
}

TEST(MetaKeyHasher, operator_eq_l2mc)
{
    sai_object_meta_key_t ma;
    sai_object_meta_key_t mb;

    memset(&ma, 0, sizeof(ma));
    memset(&mb, 0, sizeof(mb));

    ma.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;
    mb.objecttype = SAI_OBJECT_TYPE_L2MC_ENTRY;

    ma.objectkey.key.l2mc_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    ma.objectkey.key.l2mc_entry.source.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

    mb.objectkey.key.l2mc_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    mb.objectkey.key.l2mc_entry.source.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

    MetaKeyHasher mh;

    EXPECT_TRUE(mh.operator()(ma, mb));
}

TEST(MetaKeyHasher, operator_eq_ipck)
{
    sai_object_meta_key_t ma;
    sai_object_meta_key_t mb;

    memset(&ma, 0, sizeof(ma));
    memset(&mb, 0, sizeof(mb));

    ma.objecttype = SAI_OBJECT_TYPE_IPMC_ENTRY;
    mb.objecttype = SAI_OBJECT_TYPE_IPMC_ENTRY;

    ma.objectkey.key.ipmc_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    ma.objectkey.key.ipmc_entry.source.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

    mb.objectkey.key.ipmc_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    mb.objectkey.key.ipmc_entry.source.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

    MetaKeyHasher mh;

    EXPECT_TRUE(mh.operator()(ma, mb));
}
