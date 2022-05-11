#include "FdbEntryKeyHasher.h"

#include <gtest/gtest.h>

#include <memory>

using namespace syncd;

TEST(FdbEntryKeyHasher, operator_eq)
{
    sai_fdb_entry_t a;
    sai_fdb_entry_t b;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(a));

    FdbEntryKeyHasher kh;

    EXPECT_TRUE(kh.operator()(a, b));
}

TEST(FdbEntryKeyHasher, operator_hash)
{
    sai_fdb_entry_t a;

    memset(&a, 0, sizeof(a));

    FdbEntryKeyHasher kh;

    EXPECT_EQ(kh.operator()(a), 0);
}
