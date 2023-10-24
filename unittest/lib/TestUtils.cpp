#include "Utils.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <vector>

using namespace sairedis;

TEST(Utils, clearOidValues)
{
    sai_attribute_t attr;

    sai_object_id_t oids[1];

    attr.id = 1000;

    EXPECT_THROW(Utils::clearOidValues(SAI_OBJECT_TYPE_NULL, 1, &attr), std::runtime_error);

    attr.id = SAI_ACL_ENTRY_ATTR_FIELD_IN_PORT;

    attr.value.aclfield.data.oid = 1;

    Utils::clearOidValues(SAI_OBJECT_TYPE_ACL_ENTRY, 1, &attr);

    EXPECT_EQ(attr.value.aclfield.data.oid, 0);

    attr.id = SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS;

    attr.value.aclfield.enable = true;

    attr.value.aclfield.data.objlist.count = 1;
    attr.value.aclfield.data.objlist.list = oids;

    oids[0] = 1;

    Utils::clearOidValues(SAI_OBJECT_TYPE_ACL_ENTRY, 1, &attr);

    EXPECT_EQ(oids[0], 0);

    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_COUNTER;

    attr.value.aclaction.parameter.oid = 1;

    Utils::clearOidValues(SAI_OBJECT_TYPE_ACL_ENTRY, 1, &attr);

    EXPECT_EQ(attr.value.aclaction.parameter.oid, 0);

    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT_LIST;

    attr.value.aclaction.enable = true;

    attr.value.aclaction.parameter.objlist.count = 1;
    attr.value.aclaction.parameter.objlist.list = oids;

    oids[0] = 1;

    Utils::clearOidValues(SAI_OBJECT_TYPE_ACL_ENTRY, 1, &attr);

    EXPECT_EQ(oids[0], 0);

}

struct ExpectedTimeToReachTargetData
{
    uint64_t half_life_usec;
    uint32_t initial_value;
    uint32_t target_value;
    uint64_t expected_time_to_reach_target_usec;
};

TEST(Utils, TimeToReachTargetValueUsingHalfLifeWithInvalidInput)
{
    std::vector<struct ExpectedTimeToReachTargetData> testData = {
        /*Invalid input when initial value is 0.*/ {5, 0, 10, 0},
        /*Invalid input when target value is 0.*/ {5, 10, 0, 0},
        /*Invalid input when target value is more than initial value.*/
        {5, 5, 10, 0},
        /*Invalid input when initial value is same as target value.*/
        {5, 10, 10, 0},
        /*Invalid input when half life duration is 0.*/ {0, 15, 10, 0}};

    for (const auto &data : testData)
    {
        SCOPED_TRACE(::testing::Message()
                     << "Testing half life(usec): " << data.half_life_usec
                     << ", initial value: " << data.initial_value
                     << ", final value: " << data.target_value
                     << ", expected_time_to_reach_target(usec): "
                     << data.expected_time_to_reach_target_usec);

        EXPECT_EQ(Utils::timeToReachTargetValueUsingHalfLife(
                  data.half_life_usec, data.initial_value, data.target_value),
                  data.expected_time_to_reach_target_usec);
    }
}

TEST(Utils, VerifyTimeToReachTargetValueUsingHalfLife)
{
    std::vector<struct ExpectedTimeToReachTargetData> testData = {
        {30000000, 4500, 500, 95097750},  {30000000, 4500, 1100, 60972644},
        {30000000, 4500, 1700, 42131707}, {8000000, 17000, 1300, 29671609},
        {8000000, 17000, 350, 44816288},  {8000000, 1532, 311, 18403438},
        {8000000, 1532, 1, 84649604},     {5000000, 20000, 2500, 15000000},
        {5000000, 20000, 133, 36162149},  {30000000, 2000, 1500, 12451124}};

    for (const auto &data : testData)
    {
        SCOPED_TRACE(::testing::Message()
                     << "Testing half life(usec): " << data.half_life_usec
                     << ", initial value: " << data.initial_value
                     << ", final value: " << data.target_value
                     << ", expected_time_to_reach_target(usec): "
                     << data.expected_time_to_reach_target_usec);

        EXPECT_EQ(Utils::timeToReachTargetValueUsingHalfLife(
                  data.half_life_usec, data.initial_value, data.target_value),
                  data.expected_time_to_reach_target_usec);
    }
}

struct ValueAFterDecayData
{
    uint64_t time_to_decay_usec;
    uint32_t half_life_usec;
    uint32_t initial_value;
    uint32_t expected_target_value;
};

TEST(Utils, ValueAfterDecayWithInvalidInput)
{
    std::vector<struct ValueAFterDecayData> test_data = {
        /*Invalid input when time to decay duration is 0.*/ {0, 5, 10, 10},
        /*Invalid input when half life is 0.*/ {5, 0, 10, 10},
        /*Invalid input when initial value is 0.*/ {5, 5, 0, 0}};

    for (const auto &data : test_data)
    {
        SCOPED_TRACE(::testing::Message()
                     << "Testing time to decay(usec): " << data.time_to_decay_usec
                     << ", half life(usec): " << data.half_life_usec
                     << ", initial value: " << data.initial_value
                     << ", expected target value: " << data.expected_target_value);

        EXPECT_EQ(Utils::valueAfterDecay(
                  data.time_to_decay_usec, data.half_life_usec, data.initial_value),
                  data.expected_target_value);
    }
}

TEST(Utils, VerifyValueAfterDecay)
{
    std::vector<struct ValueAFterDecayData> test_data = {
        {15345678, 5000000, 20000, 2383},
        {3000000, 5000000, 20000, 13195},
        {37256870, 5000000, 15, 0}};

    for (const auto &data : test_data)
    {
        SCOPED_TRACE(::testing::Message()
                     << "Testing time to decay(usec): " << data.time_to_decay_usec
                     << ", half life(usec): " << data.half_life_usec
                     << ", initial value: " << data.initial_value
                     << ", expected target value: " << data.expected_target_value);

        EXPECT_EQ(Utils::valueAfterDecay(
                  data.time_to_decay_usec, data.half_life_usec, data.initial_value),
                  data.expected_target_value);
    }
}
