#pragma once

#include <string>
#include <map>
#include <vector>

#include "sairedis.h"
#include "syncd/ServiceMethodTable.h"
#include "meta/sai_serialize.h"

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

class TestClient
{
    public:

        TestClient();

        virtual ~TestClient();

    public:

        void test_create_switch();

        void test_create_vlan();

        void test_bulk_create_vlan();

        void test_query_api();

        void test_fdb_flush();

        void test_stats();

    protected:

        int profileGetNextValue(
                _In_ sai_switch_profile_id_t profile_id,
                _Out_ const char** variable,
                _Out_ const char** value);

        const char* profileGetValue(
                _In_ sai_switch_profile_id_t profile_id,
                _In_ const char* variable);

    protected:

        void setup();

        void teardown();

    protected:

        std::map<std::string, std::string> m_profileMap;

        std::map<std::string, std::string>::iterator m_profileIter;

        syncd::ServiceMethodTable m_smt;

        sai_service_method_table_t m_test_services;

        sai_object_id_t m_switch_id;
};

