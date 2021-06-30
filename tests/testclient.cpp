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

        void test_bulk_create_vlan();

        void test_query_api();

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

void TestClient::test_bulk_create_vlan()
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

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;

    ASSERT_SUCCESS(switch_api->get_switch_attribute(switch_id, 1, &attr));

    sai_object_id_t bridge_id = attr.value.oid;

    sai_bridge_api_t* bridge_api;

    ASSERT_SUCCESS(sai_api_query(SAI_API_BRIDGE, (void**)&bridge_api));

    sai_object_id_t ports[128];

    attr.id = SAI_BRIDGE_ATTR_PORT_LIST;
    attr.value.objlist.count = 128;
    attr.value.objlist.list = ports;

    ASSERT_SUCCESS(bridge_api->get_bridge_attribute(bridge_id, 1, &attr));

    sai_vlan_api_t* vlan_api;

    ASSERT_SUCCESS(sai_api_query(SAI_API_VLAN, (void**)&vlan_api));

    // create vlan

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 200;

    sai_object_id_t vlan_id;

    ASSERT_SUCCESS(vlan_api->create_vlan(&vlan_id, switch_id, 1, &attr));

    ASSERT_TRUE(vlan_id != SAI_NULL_OBJECT_ID);

    // bulk create vlan members

    uint32_t attr_count[2] = { 2, 2 };

    const sai_attribute_t* attr_list[2];

    sai_attribute_t attr0[2];

    attr0[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
    attr0[0].value.oid = vlan_id;

    attr0[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
    attr0[1].value.oid = ports[0];

    attr_list[0] = attr0;

    sai_attribute_t attr1[2];

    attr1[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
    attr1[0].value.oid = vlan_id;

    attr1[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
    attr1[1].value.oid = ports[1];

    attr_list[1] = attr1;

    sai_object_id_t members[2] = { SAI_NULL_OBJECT_ID, SAI_NULL_OBJECT_ID };
    sai_status_t statuses[2];

    auto status = vlan_api->create_vlan_members(
            switch_id,
            2,
            attr_count,
            attr_list,
            SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
            members,
            statuses);

    ASSERT_SUCCESS(status);

    ASSERT_SUCCESS(statuses[0]);
    ASSERT_SUCCESS(statuses[1]);

    ASSERT_TRUE(members[0] != SAI_NULL_OBJECT_ID);
    ASSERT_TRUE(members[1] != SAI_NULL_OBJECT_ID);

    SWSS_LOG_NOTICE("members: %s, %s",
            sai_serialize_object_id(members[0]).c_str(),
            sai_serialize_object_id(members[1]).c_str());

    // bulk remove vlan members

    status = vlan_api->remove_vlan_members(
            2,
            members,
            SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
            statuses);

    ASSERT_SUCCESS(status);

    ASSERT_SUCCESS(statuses[0]);
    ASSERT_SUCCESS(statuses[1]);

    // remove vlan

    ASSERT_SUCCESS(vlan_api->remove_vlan(vlan_id));

    ASSERT_SUCCESS(sai_api_uninitialize());
}

void TestClient::test_query_api()
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

    sai_attr_capability_t cap;

    SWSS_LOG_NOTICE(" * sai_query_attribute_capability");

    ASSERT_SUCCESS(sai_query_attribute_capability(
                switch_id,
                SAI_OBJECT_TYPE_SWITCH,
                SAI_SWITCH_ATTR_INIT_SWITCH,
                &cap));

    int32_t vec[3];
    sai_s32_list_t list;

    list.count = 3;
    list.list = vec;

    SWSS_LOG_NOTICE(" * sai_query_attribute_enum_values_capability");

    ASSERT_SUCCESS(sai_query_attribute_enum_values_capability(
                switch_id,
                SAI_OBJECT_TYPE_DEBUG_COUNTER,
                SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST,
                &list));

    uint64_t count;

    SWSS_LOG_NOTICE(" * sai_object_type_get_availability");

    ASSERT_SUCCESS(sai_object_type_get_availability(
                switch_id,
                SAI_OBJECT_TYPE_DEBUG_COUNTER,
                0,
                NULL,
                &count));

    ASSERT_SUCCESS(sai_api_uninitialize());
}

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    TestClient tc;

    tc.test_create_vlan();

    tc.test_bulk_create_vlan();

    tc.test_query_api();

    return EXIT_SUCCESS;
}
