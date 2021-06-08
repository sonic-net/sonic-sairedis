#include "sairedis.h"

#include "../syncd/ServiceMethodTable.h"

#include "meta/sai_serialize.h"

#include <string>
#include <map>

using namespace std::placeholders;

class TestClient
{
    public:

        TestClient();

        virtual ~TestClient();

    public:

        void test_create_vlan();

    private:

        int profileGetNextValue(
                _In_ sai_switch_profile_id_t profile_id,
                _Out_ const char** variable,
                _Out_ const char** value);

        const char* profileGetValue(
                _In_ sai_switch_profile_id_t profile_id,
                _In_ const char* variable);

    private:

        std::map<std::string, std::string> m_profileMap;

        std::map<std::string, std::string>::iterator m_profileIter;

        syncd::ServiceMethodTable m_smt;

        sai_service_method_table_t m_test_services;
};

TestClient::TestClient()
{
    SWSS_LOG_ENTER();

    // empty intentionally
}

TestClient::~TestClient()
{
    SWSS_LOG_ENTER();

    // empty intentionally
}

const char* TestClient::profileGetValue(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return NULL;
    }

    auto it = m_profileMap.find(variable);

    if (it == m_profileMap.end())
    {
        SWSS_LOG_NOTICE("%s: NULL", variable);
        return NULL;
    }

    SWSS_LOG_NOTICE("%s: %s", variable, it->second.c_str());

    return it->second.c_str();
}

int TestClient::profileGetNextValue(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        m_profileIter = m_profileMap.begin();
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    if (m_profileIter == m_profileMap.end())
    {
        SWSS_LOG_INFO("iterator reached end");
        return -1;
    }

    *variable = m_profileIter->first.c_str();
    *value = m_profileIter->second.c_str();

    SWSS_LOG_INFO("key: %s:%s", *variable, *value);

    m_profileIter++;

    return 0;
}

#define ASSERT_TRUE(x) \
    if (!(x)) \
{\
    SWSS_LOG_THROW("assert true failed '%s', line: %d", # x, __LINE__);\
}

#define ASSERT_SUCCESS(x) \
    if (x != SAI_STATUS_SUCCESS) \
{\
    SWSS_LOG_THROW("expected success, line: %d, got: %s", __LINE__, sai_serialize_status(x).c_str());\
}

void TestClient::test_create_vlan()
{
    SWSS_LOG_ENTER();

    m_profileMap.clear();

    m_profileMap[SAI_REDIS_KEY_ENABLE_CLIENT] = "true"; // act as a client

    m_profileIter = m_profileMap.begin();

    m_smt.profileGetValue = std::bind(&TestClient::profileGetValue, this, _1, _2);
    m_smt.profileGetNextValue = std::bind(&TestClient::profileGetNextValue, this, _1, _2, _3);

    m_test_services = m_smt.getServiceMethodTable();

    ASSERT_SUCCESS(sai_api_initialize(0, &m_test_services));

    sai_switch_api_t* switch_api;

    ASSERT_SUCCESS(sai_api_query(SAI_API_SWITCH, (void**)&switch_api));

    sai_attribute_t attr;

    // connect to existing switch
    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = false;

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    ASSERT_SUCCESS(switch_api->create_switch(&switch_id, 1, &attr));

    ASSERT_TRUE(switch_id != SAI_NULL_OBJECT_ID);

    SWSS_LOG_NOTICE("switchId: %s", sai_serialize_object_id(switch_id).c_str());

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

    ASSERT_SUCCESS(switch_api->get_switch_attribute(switch_id, 1, &attr));

    SWSS_LOG_NOTICE("got VRID: %s", sai_serialize_object_id(attr.value.oid).c_str());

    sai_vlan_api_t* vlan_api;

    ASSERT_SUCCESS(sai_api_query(SAI_API_VLAN, (void**)&vlan_api));

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 200;

    sai_object_id_t vlan_id;

    ASSERT_SUCCESS(vlan_api->create_vlan(&vlan_id, switch_id, 1, &attr));

    ASSERT_TRUE(vlan_id != SAI_NULL_OBJECT_ID);

    ASSERT_SUCCESS(vlan_api->remove_vlan(vlan_id));

    ASSERT_SUCCESS(sai_api_uninitialize());
}

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    TestClient tc;

    tc.test_create_vlan();

    return EXIT_SUCCESS;
}
