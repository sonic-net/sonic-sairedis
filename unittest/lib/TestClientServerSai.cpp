#include "ClientServerSai.h"

#include "sairedis.h"

#include "DashMeta.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>

using namespace sairedis;

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
        return NULL;

    return nullptr;
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    return 0;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

static const char* client_profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable != NULL && strcmp(variable, SAI_REDIS_KEY_ENABLE_CLIENT) == 0 )
        return "true";
    else
        return NULL;

    return nullptr;
}

static sai_service_method_table_t test_client_services = {
    client_profile_get_value,
    profile_get_next_value
};

TEST(ClientServerSai, ctr)
{
    auto css = std::make_shared<ClientServerSai>();
}

TEST(ClientServerSai, apiInitialize)
{
    auto css = std::make_shared<ClientServerSai>();

    EXPECT_NE(SAI_STATUS_SUCCESS, css->apiInitialize(1, nullptr));

    EXPECT_NE(SAI_STATUS_SUCCESS, css->apiInitialize(0, nullptr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    css = nullptr; // invoke uninitialize in destructor

    css = std::make_shared<ClientServerSai>();

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    EXPECT_NE(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));
}

TEST(ClientServerSai, objectTypeQuery)
{
    auto css = std::make_shared<ClientServerSai>();

    EXPECT_EQ(SAI_OBJECT_TYPE_NULL, css->objectTypeQuery(0x1111111111111111L));
}

TEST(ClientServerSai, switchIdQuery)
{
    auto css = std::make_shared<ClientServerSai>();

    EXPECT_EQ(SAI_NULL_OBJECT_ID, css->switchIdQuery(0x1111111111111111L));
}

TEST(ClientServerSai, queryStatsCapability)
{
    auto css = std::make_shared<ClientServerSai>();

    sai_stat_capability_list_t queue_stats_capability;
    sai_stat_capability_t stat_initializer;
    stat_initializer.stat_enum = 0;
    stat_initializer.stat_modes = 0;
    std::vector<sai_stat_capability_t> qstat_cap_list(2, stat_initializer);
    queue_stats_capability.count = 2;
    queue_stats_capability.list = qstat_cap_list.data();
    queue_stats_capability.list[0].stat_enum = SAI_QUEUE_STAT_WRED_ECN_MARKED_PACKETS;
    queue_stats_capability.list[0].stat_modes = SAI_STATS_MODE_READ;
    queue_stats_capability.list[1].stat_enum = SAI_QUEUE_STAT_PACKETS;
    queue_stats_capability.list[1].stat_modes = SAI_STATS_MODE_READ;

    EXPECT_EQ(SAI_STATUS_FAILURE, css->queryStatsCapability(SAI_NULL_OBJECT_ID, SAI_OBJECT_TYPE_QUEUE, &queue_stats_capability));
}

TEST(ClientServerSai, queryStatsStCapability)
{
    auto css = std::make_shared<ClientServerSai>();

    sai_stat_st_capability_list_t queue_stats_capability;
    sai_stat_st_capability_t stat_initializer;
    stat_initializer.capability.stat_enum = 0;
    stat_initializer.capability.stat_modes = 0;
    stat_initializer.minimal_polling_interval = 0;
    std::vector<sai_stat_st_capability_t> qstat_cap_list(2, stat_initializer);
    queue_stats_capability.count = 2;
    queue_stats_capability.list = qstat_cap_list.data();
    queue_stats_capability.list[0].capability.stat_enum = SAI_QUEUE_STAT_WRED_ECN_MARKED_PACKETS;
    queue_stats_capability.list[0].capability.stat_modes = SAI_STATS_MODE_READ;
    queue_stats_capability.list[0].minimal_polling_interval = 1000;
    queue_stats_capability.list[1].capability.stat_enum = SAI_QUEUE_STAT_PACKETS;
    queue_stats_capability.list[1].capability.stat_modes = SAI_STATS_MODE_READ;
    queue_stats_capability.list[1].minimal_polling_interval = 1000;

    EXPECT_EQ(SAI_STATUS_FAILURE, css->queryStatsStCapability(SAI_NULL_OBJECT_ID, SAI_OBJECT_TYPE_QUEUE, &queue_stats_capability));
}

TEST(ClientServerSai, logSet)
{
    auto css = std::make_shared<ClientServerSai>();

    EXPECT_NE(SAI_STATUS_SUCCESS, css->logSet(SAI_API_PORT, SAI_LOG_LEVEL_NOTICE));

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->logSet(SAI_API_PORT, SAI_LOG_LEVEL_NOTICE));
}

TEST(ClientServerSai, VerifySaiRedisPortAttrNotSupportedInClientMode)
{
    auto css = std::make_shared<ClientServerSai>();

    // Initialize as sairedis client.
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_client_services));

    sai_attribute_t attr;
    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM;
    attr.value.s32 = SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED;

    EXPECT_EQ(SAI_STATUS_FAILURE, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));
}

TEST(ClientServerSai, SetLinkEventDampingAlgorithm)
{
    auto css = std::make_shared<ClientServerSai>();

    // Initialize as sairedis server.
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    sai_attribute_t attr;
    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM;
    attr.value.s32 = SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED;

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));
}

TEST(ClientServerSai, SetLinkEventDampingConfig)
{
    auto css = std::make_shared<ClientServerSai>();

    // Initialize as sairedis server.
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    // Failure when config is NULL.
    sai_attribute_t attr;
    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG;
    attr.value.ptr = nullptr;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));

    sai_redis_link_event_damping_algo_aied_config_t config = {
      .max_suppress_time = 5000,
      .suppress_threshold = 1500,
      .reuse_threshold = 1200,
      .decay_half_life = 3000,
      .flap_penalty = 1000};

    attr.value.ptr = (void *) &config;

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));
}

TEST(ClientServerSai, SetLinkEventDampingConfigVariousValues)
{
    auto css = std::make_shared<ClientServerSai>();

    // Initialize as sairedis server.
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    sai_attribute_t attr;
    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG;

    // Test with small suppress time
    sai_redis_link_event_damping_algo_aied_config_t config1 = {
      .max_suppress_time = 1000,
      .suppress_threshold = 100,
      .reuse_threshold = 50,
      .decay_half_life = 500,
      .flap_penalty = 10};
    attr.value.ptr = (void *) &config1;
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));

    // Test with large suppress time
    sai_redis_link_event_damping_algo_aied_config_t config2 = {
      .max_suppress_time = 60000,
      .suppress_threshold = 2000,
      .reuse_threshold = 1500,
      .decay_half_life = 30000,
      .flap_penalty = 2000};
    attr.value.ptr = (void *) &config2;
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));

    // Test with zero penalties
    sai_redis_link_event_damping_algo_aied_config_t config3 = {
      .max_suppress_time = 10000,
      .suppress_threshold = 0,
      .reuse_threshold = 0,
      .decay_half_life = 5000,
      .flap_penalty = 0};
    attr.value.ptr = (void *) &config3;
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));
}

TEST(ClientServerSai, SetLinkEventDampingAlgorithmVariousTypes)
{
    auto css = std::make_shared<ClientServerSai>();

    // Initialize as sairedis server.
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    sai_attribute_t attr;
    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM;

    // Test disabled algorithm
    attr.value.s32 = SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_DISABLED;
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));

    // Test AIED algorithm
    attr.value.s32 = SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED;
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));
}

TEST(ClientServerSai, SetDampingConfigOnDifferentObjectTypes)
{
    auto css = std::make_shared<ClientServerSai>();

    // Initialize as sairedis server.
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    sai_attribute_t attr;

    // Test on QUEUE object type (not PORT)
    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM;
    attr.value.s32 = SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED;
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, css->set(SAI_OBJECT_TYPE_QUEUE, SAI_NULL_OBJECT_ID, &attr));

    // Test on VIRTUAL_ROUTER object type
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, css->set(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, SAI_NULL_OBJECT_ID, &attr));

}

TEST(ClientServerSai, MultiplePortsDampingConfig)
{
    auto css = std::make_shared<ClientServerSai>();

    // Initialize as sairedis server.
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    sai_attribute_t attr;
    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM;
    attr.value.s32 = SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED;

    // Configure damping on multiple different object IDs
    // (In real code, these would be different port VIDs)
    sai_object_id_t port_ids[] = {0x1000000000000001, 0x1000000000000002, 0x1000000000000003};

    for (size_t i = 0; i < sizeof(port_ids) / sizeof(port_ids[0]); ++i)
    {
        EXPECT_EQ(SAI_STATUS_SUCCESS, css->set(SAI_OBJECT_TYPE_PORT, port_ids[i], &attr));
    }

    // Verify all ports can be configured with different settings
    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG;
    sai_redis_link_event_damping_algo_aied_config_t configs[] = {
        {.max_suppress_time = 5000, .suppress_threshold = 1500, .reuse_threshold = 1200,
         .decay_half_life = 2500, .flap_penalty = 500},
        {.max_suppress_time = 10000, .suppress_threshold = 2000, .reuse_threshold = 1500,
         .decay_half_life = 5000, .flap_penalty = 1000},
        {.max_suppress_time = 15000, .suppress_threshold = 2500, .reuse_threshold = 2000,
         .decay_half_life = 7500, .flap_penalty = 1500},
    };

    for (size_t i = 0; i < sizeof(configs) / sizeof(configs[0]); ++i)
    {
        attr.value.ptr = (void *) &configs[i];
        EXPECT_EQ(SAI_STATUS_SUCCESS, css->set(SAI_OBJECT_TYPE_PORT, port_ids[i], &attr));
    }
}

TEST(ClientServerSai, SetInvalidSaiRedisPortAttribute)
{
    auto css = std::make_shared<ClientServerSai>();

    // Initialize as sairedis server.
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    sai_attribute_t attr;
    // Set an id that is not supported yet.
    attr.id = SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG + 100;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, css->set(SAI_OBJECT_TYPE_PORT, SAI_NULL_OBJECT_ID, &attr));
}

TEST(ClientServerSai, bulkGetClearStats)
{
    auto css = std::make_shared<ClientServerSai>();

    EXPECT_EQ(SAI_STATUS_FAILURE, css->bulkGetStats(SAI_NULL_OBJECT_ID,
                                                    SAI_OBJECT_TYPE_PORT,
                                                    0,
                                                    nullptr,
                                                    0,
                                                    nullptr,
                                                    SAI_STATS_MODE_BULK_READ,
                                                    nullptr,
                                                    nullptr));

    EXPECT_EQ(SAI_STATUS_FAILURE, css->bulkClearStats(SAI_NULL_OBJECT_ID,
                                                      SAI_OBJECT_TYPE_PORT,
                                                      0,
                                                      nullptr,
                                                      0,
                                                      nullptr,
                                                      SAI_STATS_MODE_BULK_CLEAR,
                                                      nullptr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, css->bulkGetStats(SAI_NULL_OBJECT_ID,
                                                            SAI_OBJECT_TYPE_PORT,
                                                            0,
                                                            nullptr,
                                                            0,
                                                            nullptr,
                                                            SAI_STATS_MODE_BULK_READ,
                                                            nullptr,
                                                            nullptr));

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, css->bulkClearStats(SAI_NULL_OBJECT_ID,
                                                              SAI_OBJECT_TYPE_PORT,
                                                              0,
                                                              nullptr,
                                                              0,
                                                              nullptr,
                                                              SAI_STATS_MODE_BULK_CLEAR,
                                                              nullptr));

    css = std::make_shared<ClientServerSai>();
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_client_services));

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, css->bulkGetStats(SAI_NULL_OBJECT_ID,
                                                            SAI_OBJECT_TYPE_PORT,
                                                            0,
                                                            nullptr,
                                                            0,
                                                            nullptr,
                                                            SAI_STATS_MODE_BULK_READ,
                                                            nullptr,
                                                            nullptr));

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, css->bulkClearStats(SAI_NULL_OBJECT_ID,
                                                              SAI_OBJECT_TYPE_PORT,
                                                              0,
                                                              nullptr,
                                                              0,
                                                              nullptr,
                                                              SAI_STATS_MODE_BULK_CLEAR,
                                                              nullptr));
}

#define TEST_ENTRY(OT,ot)                                                \
TEST(ClientServerSai, OT)                                                \
{                                                                        \
    if (saimeta::bypassValidation((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT)) \
    {                                                                    \
        GTEST_SKIP() << "meta validation bypassed for " #OT;             \
    }                                                                    \
    auto css = std::make_shared<ClientServerSai>();                      \
    sai_ ## ot ## _t e = {};                                             \
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));   \
    EXPECT_NE(SAI_STATUS_SUCCESS, css->create(&e, 0, nullptr));          \
    EXPECT_NE(SAI_STATUS_SUCCESS, css->set(&e, nullptr));                \
    EXPECT_NE(SAI_STATUS_SUCCESS, css->get(&e, 0, nullptr));             \
    EXPECT_NE(SAI_STATUS_SUCCESS, css->remove(&e));                      \
                                                                         \
    auto ss = std::make_shared<ClientServerSai>();                       \
    EXPECT_EQ(SAI_STATUS_SUCCESS, ss->apiInitialize(0, &test_services));    \
    EXPECT_NE(SAI_STATUS_SUCCESS, ss->create(&e, 0, nullptr));           \
    EXPECT_NE(SAI_STATUS_SUCCESS, ss->set(&e, nullptr));                 \
    EXPECT_NE(SAI_STATUS_SUCCESS, ss->get(&e, 0, nullptr));              \
    EXPECT_NE(SAI_STATUS_SUCCESS, ss->remove(&e));                       \
}

SAIREDIS_DECLARE_EVERY_ENTRY(TEST_ENTRY)

#define TEST_BULK_ENTRY(OT,ot)                                                                                               \
TEST(ClientServerSai, bulk_ ## OT)                                                                                           \
{                                                                                                                            \
    if (saimeta::bypassValidation((sai_object_type_t)SAI_OBJECT_TYPE_ ## OT))                                                \
    {                                                                                                                        \
        GTEST_SKIP() << "meta validation bypassed for " #OT;                                                                 \
    }                                                                                                                        \
    auto css = std::make_shared<ClientServerSai>();                                                                          \
    sai_ ## ot ## _t e[2] = {};                                                                                              \
    EXPECT_EQ(SAI_STATUS_SUCCESS, css->apiInitialize(0, &test_services));                                                       \
    EXPECT_NE(SAI_STATUS_SUCCESS, css->bulkCreate(0, e, nullptr, nullptr, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, nullptr));    \
    EXPECT_NE(SAI_STATUS_SUCCESS, css->bulkSet(2, e, nullptr, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, nullptr));                \
    EXPECT_NE(SAI_STATUS_SUCCESS, css->bulkRemove(2, e, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, nullptr));                      \
                                                                                                                             \
    auto ss = std::make_shared<ClientServerSai>();                                                                           \
    EXPECT_EQ(SAI_STATUS_SUCCESS, ss->apiInitialize(0, &test_client_services));                                                 \
    EXPECT_NE(SAI_STATUS_SUCCESS, ss->bulkCreate(0, e, nullptr, nullptr, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, nullptr));     \
    EXPECT_NE(SAI_STATUS_SUCCESS, ss->bulkSet(0, e, nullptr, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, nullptr));                 \
    EXPECT_NE(SAI_STATUS_SUCCESS, ss->bulkRemove(0, e, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, nullptr));                       \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(TEST_BULK_ENTRY)


TEST(ClientServerSai, bulkGet)
{
    ClientServerSai sai;

    sai_object_id_t oids[1] = {0};
    uint32_t attrcount[1] = {0};
    sai_attribute_t* attrs[1] = {0};
    sai_status_t statuses[1] = {0};

    EXPECT_NE(SAI_STATUS_SUCCESS,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                1,
                oids,
                attrcount,
                attrs,
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses));
}

