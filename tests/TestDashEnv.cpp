#include "TestDashEnv.h"

#include "sairedis.h"
#include "syncd/ServiceMethodTable.h"

#define ASSERT_SUCCESS(e) ASSERT_EQ((e), SAI_STATUS_SUCCESS)

static int profileGetNextValue(
    _In_ sai_switch_profile_id_t profile_id,
    _Out_ const char **variable,
    _Out_ const char **value)
{
    // SWSS_LOG_ENTER(); // disabled

    return 0;
}

static const char* profileGetValue(
    _In_ sai_switch_profile_id_t profile_id,
    _In_ const char *variable,
    _In_ bool is_client)
{
    // SWSS_LOG_ENTER(); // disabled

    if (is_client && !std::string(variable).compare(SAI_REDIS_KEY_ENABLE_CLIENT)) {
        return "true";
    }

    return NULL;
}

TestDashEnv* TestDashEnv::instance()
{
    // SWSS_LOG_ENTER(); // disabled

    static TestDashEnv* env = new TestDashEnv();

    return env;
}

void TestDashEnv::StopSyncd()
{
    SWSS_LOG_ENTER();

    int rc = std::system("killall -q -9 syncd vssyncd");
    ASSERT_TRUE(rc == 0 || rc == 256);
}

void TestDashEnv::StartSyncd()
{
    SWSS_LOG_ENTER();

    int rc = std::system("./vssyncd -SUu -p \"NVDAMBF2H536C/vsprofile.ini\" -z redis_sync &");
    ASSERT_EQ(rc, 0);
}

void TestDashEnv::FlushRedis()
{
    SWSS_LOG_ENTER();

    int rc = std::system("redis-cli flushall");
    ASSERT_EQ(rc, 0);
}

void TestDashEnv::CreateSwitch()
{
    SWSS_LOG_ENTER();

    using namespace std::placeholders;

    syncd::ServiceMethodTable smt;
    smt.profileGetValue = std::bind(profileGetValue, _1, _2, !m_manage_syncd);
    smt.profileGetNextValue = profileGetNextValue;
    auto sai_smt = smt.getServiceMethodTable();

    ASSERT_EQ(sai_api_initialize(0, &sai_smt), SAI_STATUS_SUCCESS);

    sai_switch_api_t *switch_api;
    ASSERT_EQ(sai_api_query(SAI_API_SWITCH, (void **)&switch_api), SAI_STATUS_SUCCESS);

    sai_attribute_t attr;

    if (m_manage_syncd)
    {
        attr.id = SAI_REDIS_SWITCH_ATTR_RECORD;
        attr.value.booldata = true;
        ASSERT_EQ(switch_api->set_switch_attribute(SAI_NULL_OBJECT_ID, &attr), SAI_STATUS_SUCCESS);

        attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;
        ASSERT_EQ(switch_api->set_switch_attribute(SAI_NULL_OBJECT_ID, &attr), SAI_STATUS_SUCCESS);

        attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
        attr.value.s32 = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;
        ASSERT_EQ(switch_api->set_switch_attribute(SAI_NULL_OBJECT_ID, &attr), SAI_STATUS_SUCCESS);
    }

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = m_manage_syncd;
    ASSERT_EQ(switch_api->create_switch(&m_switch_id, 1, &attr), SAI_STATUS_SUCCESS);

    ASSERT_NE(m_switch_id, SAI_NULL_OBJECT_ID);

    if (m_manage_syncd)
    {
        attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;
        ASSERT_EQ(switch_api->set_switch_attribute(m_switch_id, &attr), SAI_STATUS_SUCCESS);
    }
}

TestDashEnv::TestDashEnv()
{
    m_manage_syncd = getenv(ENV_CLIENT_MODE_OPT) == nullptr;
}

void TestDashEnv::SetUp()
{
    SWSS_LOG_ENTER();

    if (m_manage_syncd) {
        StopSyncd();
        FlushRedis();
        StartSyncd();
    }

    CreateSwitch();
}

void TestDashEnv::TearDown()
{
    SWSS_LOG_ENTER();

    auto status = sai_api_uninitialize();
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    if (m_manage_syncd) {
        StopSyncd();
    }
}
