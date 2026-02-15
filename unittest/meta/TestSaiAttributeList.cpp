#include "SaiAttributeList.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(SaiAttributeList, ctr_vector)
{
    std::vector<swss::FieldValueTuple> vals;

    vals.emplace_back("foo", "bar");

    EXPECT_THROW(std::make_shared<SaiAttributeList>(SAI_OBJECT_TYPE_SWITCH, vals, false), std::runtime_error);

    vals.clear();

    vals.emplace_back("SAI_SWITCH_ATTR_INIT_SWITCH", "true");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    EXPECT_THROW(std::make_shared<SaiAttributeList>((sai_object_type_t)-1, vals, false), std::runtime_error);
#pragma GCC diagnostic pop
}

TEST(SaiAttributeList, ctr_hash)
{
    std::unordered_map<std::string, std::string> hash;

    hash["foo"] = "bar";

    EXPECT_THROW(std::make_shared<SaiAttributeList>(SAI_OBJECT_TYPE_SWITCH, hash, false), std::runtime_error);

    hash.clear();

    hash["NULL"] = "NULL";
    hash["SAI_SWITCH_ATTR_INIT_SWITCH"] = "true";

    SaiAttributeList w(SAI_OBJECT_TYPE_SWITCH, hash, false);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    EXPECT_THROW(std::make_shared<SaiAttributeList>((sai_object_type_t)-1, hash, false), std::runtime_error);
#pragma GCC diagnostic pop
}
