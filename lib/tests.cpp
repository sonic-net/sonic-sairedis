extern "C" {
#include "saimetadata.h"
}

#include "ContextConfigContainer.h"

#include "swss/logger.h"
#include "swss/table.h"
#include "swss/tokenize.h"

#include "Recorder.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"
#include "meta/Globals.h"

#include <unistd.h>

#include <iostream>
#include <chrono>
#include <vector>

#define ASSERT_EQ(a,b) if ((a) != (b)) { SWSS_LOG_THROW("ASSERT EQ FAILED: " #a " != " #b); }

using namespace saimeta;
using namespace sairedis;

const std::string SairedisRecFilename = "sairedis.rec";

sai_object_type_t sai_object_type_query(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return SAI_OBJECT_TYPE_NULL;
}

sai_object_id_t sai_switch_id_query(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return SAI_NULL_OBJECT_ID;
}

sai_route_entry_t get_route_entry()
{
    SWSS_LOG_ENTER();

    sai_route_entry_t route_entry = { };

    route_entry.vr_id = 0x123456789abcdef;
    route_entry.switch_id = 0x123456789abcdef;
    route_entry.destination.addr.ip4 = 0x12345678;
    route_entry.destination.mask.ip4 = 0xffffffff;

    return route_entry;
}

std::string serialize_route_entry()
{
    SWSS_LOG_ENTER();

    static auto route_entry = get_route_entry();

    return sai_serialize_object_type(SAI_OBJECT_TYPE_ROUTE_ENTRY) + ":" +
        sai_serialize_route_entry(route_entry);
}

std::string serialize_route_entry2()
{
    SWSS_LOG_ENTER();

    static auto route_entry = get_route_entry();

    char buffer[1000];

    int n = sai_serialize_object_type(buffer, SAI_OBJECT_TYPE_ROUTE_ENTRY);
    buffer[n] = ':';

    sai_serialize_route_entry(buffer+n+1, &route_entry);

    return  std::string(buffer);
}

std::string serialize_vlan()
{
    SWSS_LOG_ENTER();

    sai_object_id_t oid = 0x123456789abcdef;

    return sai_serialize_object_type(SAI_OBJECT_TYPE_VLAN) + ":" +
        sai_serialize_object_id(oid);
}

void test_serialize_remove_route_entry(int n)
{
    SWSS_LOG_ENTER();

    std::cout << serialize_route_entry() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_route_entry();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    std::cout << "ms: " << (double)us.count()/1000 << " / " << n << std::endl;
}

void test_deserialize_route_entry_meta(int n)
{
    SWSS_LOG_ENTER();

    // meta key 123ms/10k
    auto s = serialize_route_entry();
    // auto s = sai_serialize_route_entry(get_route_entry());

    std::cout << s << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        sai_object_meta_key_t mk;
        sai_deserialize_object_meta_key(s, mk);

        //sai_route_entry_t route_entry;
        //sai_deserialize_route_entry(s, route_entry);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    std::cout << "ms: " << (double)us.count()/1000 << " / " << n << std::endl;
}

void test_deserialize_route_entry(int n)
{
    SWSS_LOG_ENTER();

    // 7.2 ms
    // 8.9 ms with try/catch
    // 13ms for entire object meta key

    char buffer[1000];

    sai_route_entry_t route_entry = get_route_entry();
    sai_serialize_route_entry(buffer, &route_entry);

    auto s = std::string(buffer);

    std::cout << s << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        sai_deserialize_route_entry(s.c_str(), &route_entry);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    std::cout << "ms: " << (double)us.count()/1000 << " / " << n << std::endl;
}

void test_serialize_remove_oid(int n)
{
    SWSS_LOG_ENTER();

    std::cout << serialize_vlan() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_vlan();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    std::cout << "ms: " << (double)us.count()/1000 << " / " << n << std::endl;
}

sai_fdb_entry_t get_fdb_entry()
{
    SWSS_LOG_ENTER();

    std::string str = "{\"bvid\":\"oid:0x123456789abcdef\",\"mac\":\"52:54:00:79:16:93\",\"switch_id\":\"oid:0x123456789abcdef\"}";

    sai_fdb_entry_t fdb_entry;
    sai_deserialize_fdb_entry(str, fdb_entry);

    return fdb_entry;
}

std::string serialize_fdb_entry()
{
    SWSS_LOG_ENTER();

    static auto fdb_entry = get_fdb_entry();

    return sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_ENTRY) + ":" +
        sai_serialize_fdb_entry(fdb_entry);
}

void test_serialize_remove_fdb_entry(int n)
{
    SWSS_LOG_ENTER();

    std::cout << serialize_fdb_entry() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_fdb_entry();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    std::cout << "ms: " << (double)us.count()/1000 << " / " << n << std::endl;
}

std::string serialize_bulk_vlan(int per)
{
    SWSS_LOG_ENTER();

    sai_object_id_t oid = 0x123456789abcdef;

    int n = per;

    std::string str_object_type = sai_serialize_object_type(SAI_OBJECT_TYPE_VLAN);

    std::vector<swss::FieldValueTuple> entries;

    for (int idx = 0; idx < n; ++idx)
    {
        std::string str_attr = "";

        swss::FieldValueTuple fvtNoStatus(sai_serialize_object_id(oid), str_attr);

        entries.push_back(fvtNoStatus);
    }

    std::string joined;

    for (const auto &e: entries)
    {
        joined += "||" + fvField(e);
    }

    return str_object_type + ":" + std::to_string(entries.size()) + joined;
}

void test_serialize_bulk_remove_oid(int n, int per)
{
    SWSS_LOG_ENTER();

    auto str = serialize_bulk_vlan(per);
    std::cout << str.substr(0,100) << " ... len " << str.size() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_bulk_vlan(per);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    //std::cout << "ms: " << (double)us.count()/1000 << " n " << n << " x / per " << per << std::endl;
    std::cout << "ms: " << (double)us.count()/1000.0 / n << " n " << n << " / per " << per << std::endl;
}

std::string serialize_bulk_route_entry(int per)
{
    SWSS_LOG_ENTER();

    static auto route_entry = get_route_entry();

    int n = per;

    std::string str_object_type = sai_serialize_object_type(SAI_OBJECT_TYPE_ROUTE_ENTRY);

    std::vector<swss::FieldValueTuple> entries;

    for (int idx = 0; idx < n; ++idx)
    {
        std::string str_attr = "";

        swss::FieldValueTuple fvtNoStatus(sai_serialize_route_entry(route_entry), str_attr);

        entries.push_back(fvtNoStatus);
    }

    std::string joined;

    for (const auto &e: entries)
    {
        joined += "||" + fvField(e);
    }

    return str_object_type + ":" + std::to_string(entries.size()) + joined;
}

void test_serialize_bulk_remove_route_entry(int n, int per)
{
    SWSS_LOG_ENTER();

    auto str = serialize_bulk_route_entry(per);

    std::cout << str.substr(0,100) << " ... len " << str.size() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_bulk_route_entry(per);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    //std::cout << "ms: " << (double)us.count()/1000 << " n " << n << " x / per " << per << std::endl;
    std::cout << "ms: " << (double)us.count()/1000.0 / n << " n " << n << " / per " << per << std::endl;
}

std::string serialize_bulk_fdb_entry(int per)
{
    SWSS_LOG_ENTER();

    static auto fdb_entry = get_fdb_entry();

    int n = per;

    std::string str_object_type = sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_ENTRY);

    std::vector<swss::FieldValueTuple> entries;

    for (int idx = 0; idx < n; ++idx)
    {
        std::string str_attr = "";

        swss::FieldValueTuple fvtNoStatus(sai_serialize_fdb_entry(fdb_entry), str_attr);

        entries.push_back(fvtNoStatus);
    }

    std::string joined;

    for (const auto &e: entries)
    {
        joined += "||" + fvField(e);
    }

    return str_object_type + ":" + std::to_string(entries.size()) + joined;
}

void test_serialize_bulk_remove_fdb_entry(int n, int per)
{
    SWSS_LOG_ENTER();

    auto str = serialize_bulk_fdb_entry(per);

    std::cout << str.substr(0,100) << " ... len " << str.size() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_bulk_fdb_entry(per);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    //std::cout << "ms: " << (double)us.count()/1000 << " n " << n << " x / per " << per << std::endl;
    std::cout << "ms: " << (double)us.count()/1000.0 / n << " n " << n << " / per " << per << std::endl;
}

const std::vector<swss::FieldValueTuple> get_values(const std::vector<std::string>& items)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    // timestamp|action|objecttype:objectid|attrid=value,...
    for (size_t i = 3; i <items.size(); ++i)
    {
        const std::string& item = items[i];

        auto start = item.find_first_of("=");

        auto field = item.substr(0, start);
        auto value = item.substr(start + 1);

        swss::FieldValueTuple entry(field, value);

        values.push_back(entry);
    }

    return values;
}

SaiAttributeList* get_route_entry_list()
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    values.push_back(swss::FieldValueTuple("SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION","SAI_PACKET_ACTION_FORWARD"));
    values.push_back(swss::FieldValueTuple("SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID","oid:0x1000000000001"));

    return new SaiAttributeList(SAI_OBJECT_TYPE_ROUTE_ENTRY, values, false);
}

std::string serialize_create_route_entry()
{
    SWSS_LOG_ENTER();

    static auto route_entry = get_route_entry();

    static SaiAttributeList *list = get_route_entry_list();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            list->get_attr_count(),
            list->get_attr_list(),
            false);

    std::string joined;

    for (const auto &e: entry)
    {
        joined += "||" + fvField(e) + "|" + fvValue(e);
    }

    return sai_serialize_object_type(SAI_OBJECT_TYPE_ROUTE_ENTRY) + ":" +
        sai_serialize_route_entry(route_entry) + ":" + joined;
}

void test_serialize_create_route_entry(int n)
{
    SWSS_LOG_ENTER();

    std::cout << serialize_create_route_entry() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_create_route_entry();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    std::cout << "ms: " << (double)us.count()/1000 << " / " << n << std::endl;
}

SaiAttributeList* get_fdb_entry_list()
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    values.push_back(swss::FieldValueTuple("SAI_FDB_ENTRY_ATTR_TYPE","SAI_FDB_ENTRY_TYPE_DYNAMIC"));
    values.push_back(swss::FieldValueTuple("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID","oid:0x3a000000000fb"));
    values.push_back(swss::FieldValueTuple("SAI_FDB_ENTRY_ATTR_PACKET_ACTION","SAI_PACKET_ACTION_FORWARD"));

    return new SaiAttributeList(SAI_OBJECT_TYPE_FDB_ENTRY, values, false);
}

std::string serialize_create_fdb_entry()
{
    SWSS_LOG_ENTER();

    static auto fdb_entry = get_fdb_entry();

    static SaiAttributeList *list = get_fdb_entry_list();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_FDB_ENTRY,
            list->get_attr_count(),
            list->get_attr_list(),
            false);

    std::string joined;

    for (const auto &e: entry)
    {
        joined += "||" + fvField(e) + "|" + fvValue(e);
    }

    return sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_ENTRY) + ":" +
        sai_serialize_fdb_entry(fdb_entry) + ":" + joined;
}

void test_serialize_create_fdb_entry(int n)
{
    SWSS_LOG_ENTER();

    std::cout << serialize_create_fdb_entry() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_create_fdb_entry();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    std::cout << "ms: " << (double)us.count()/1000 << " / " << n << std::endl;
}

SaiAttributeList* get_oid_list()
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    values.push_back(swss::FieldValueTuple("SAI_VLAN_ATTR_VLAN_ID","2"));

    return new SaiAttributeList(SAI_OBJECT_TYPE_VLAN, values, false);
}

std::string serialize_create_oid()
{
    SWSS_LOG_ENTER();

    static sai_object_id_t oid = 0x123456789abcdef;

    static SaiAttributeList *list = get_oid_list();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_VLAN,
            list->get_attr_count(),
            list->get_attr_list(),
            false);

    std::string joined;

    for (const auto &e: entry)
    {
        joined += "||" + fvField(e) + "|" + fvValue(e);
    }

    return sai_serialize_object_type(SAI_OBJECT_TYPE_VLAN) + ":" +
        sai_serialize_object_id(oid) + "|" + joined;
}

void test_serialize_create_oid(int n)
{
    SWSS_LOG_ENTER();

    std::cout << serialize_create_oid() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_create_oid();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    std::cout << "ms: " << (double)us.count()/1000 << " / " << n << std::endl;
}

std::string serialize_bulk_create_route_entry(int per)
{
    SWSS_LOG_ENTER();

    static auto route_entry = get_route_entry();

    static SaiAttributeList *list = get_route_entry_list();

    std::string str_object_type = sai_serialize_object_type(SAI_OBJECT_TYPE_ROUTE_ENTRY);

    std::vector<swss::FieldValueTuple> entries;

    for (int idx = 0; idx < per; ++idx)
    {
        std::vector<swss::FieldValueTuple> entry =
            SaiAttributeList::serialize_attr_list(SAI_OBJECT_TYPE_ROUTE_ENTRY, list->get_attr_count(), list->get_attr_list(), false);

        std::string str_attr = Globals::joinFieldValues(entry);

        std::string str_status = sai_serialize_status(SAI_STATUS_NOT_EXECUTED);

        std::string joined = str_attr + "|" + str_status;

        swss::FieldValueTuple fvt(sai_serialize_route_entry(route_entry) , joined);

        entries.push_back(fvt);
    }

    std::string joined;

    for (const auto &e: entries)
    {
        joined += "||" + fvField(e) + "|" + fvValue(e);
    }

    return str_object_type + ":" + std::to_string(entries.size()) + joined;
}

void test_serialize_bulk_create_route_entry(int n, int per)
{
    SWSS_LOG_ENTER();

    auto str = serialize_bulk_create_route_entry(per);

    std::cout << str.substr(0,300) << " ... len " << str.size() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_bulk_route_entry(per);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    //std::cout << "ms: " << (double)us.count()/1000 << " n " << n << " x / per " << per << std::endl;
    std::cout << "ms: " << (double)us.count()/1000.0 / n << " n " << n << " / per " << per << std::endl;
}

std::string serialize_bulk_create_oid(int per)
{
    SWSS_LOG_ENTER();

    static sai_object_id_t oid = 0x123456789abcdef;

    static SaiAttributeList *list = get_oid_list();

    std::string str_object_type = sai_serialize_object_type(SAI_OBJECT_TYPE_VLAN);

    std::vector<swss::FieldValueTuple> entries;

    for (int idx = 0; idx < per; ++idx)
    {
        std::vector<swss::FieldValueTuple> entry =
            SaiAttributeList::serialize_attr_list(SAI_OBJECT_TYPE_VLAN, list->get_attr_count(), list->get_attr_list(), false);

        std::string str_attr = Globals::joinFieldValues(entry);

        std::string str_status = sai_serialize_status(SAI_STATUS_NOT_EXECUTED);

        std::string joined = str_attr + "|" + str_status;

        swss::FieldValueTuple fvt(sai_serialize_object_id(oid) , joined);

        entries.push_back(fvt);
    }

    std::string joined;

    for (const auto &e: entries)
    {
        joined += "||" + fvField(e) + "|" + fvValue(e);
    }

    return str_object_type + ":" + std::to_string(entries.size()) + joined;
}

void test_serialize_bulk_create_oid(int n, int per)
{
    SWSS_LOG_ENTER();

    auto str = serialize_bulk_create_oid(per);

    std::cout << str.substr(0,300) << " ... len " << str.size() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        auto s = serialize_bulk_create_oid(per);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = end - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);
    //std::cout << "ms: " << (double)us.count()/1000 << " n " << n << " x / per " << per << std::endl;
    std::cout << "ms: " << (double)us.count()/1000.0 / n << " n " << n << " / per " << per << std::endl;
}

void test_ContextConfigContainer()
{
    SWSS_LOG_ENTER();

    auto ccc = ContextConfigContainer::loadFromFile("context_config.json");
}

static std::vector<std::string> tokenize(
        _In_ std::string input,
        _In_ const std::string &delim)
{
    SWSS_LOG_ENTER();

    /*
     * input is modified so it can't be passed as reference
     */

    std::vector<std::string> tokens;

    size_t pos = 0;

    while ((pos = input.find(delim)) != std::string::npos)
    {
        std::string token = input.substr(0, pos);

        input.erase(0, pos + delim.length());
        tokens.push_back(token);
    }

    tokens.push_back(input);

    return tokens;
}

static sai_object_type_t deserialize_object_type(
        _In_ const std::string& s)
{
    SWSS_LOG_ENTER();

    sai_object_type_t object_type;

    sai_deserialize_object_type(s, object_type);

    return object_type;
}

static std::vector<std::string> parseFirstRecordedAPI()
{
    SWSS_LOG_ENTER();

    std::ifstream infile(SairedisRecFilename);
    std::string line;

    // skip first line
    std::getline(infile, line);
    std::getline(infile, line);

    std::vector<std::string> tokens;
    std::stringstream sstream(line);
    std::string token;

    const auto delimiter = '|';

    // skip first, it is a timestamp
    std::getline(sstream, token, delimiter);

    while(std::getline(sstream, token, delimiter))
    {
       tokens.push_back(token);
    }

    return tokens;
}

static void test_recorder_enum_value_capability_query_request(
    _In_ sai_object_id_t switch_id,
    _In_ sai_object_type_t object_type,
    _In_ sai_attr_id_t attr_id,
    _In_ const std::vector<std::string>& expectedOutput)
{
    SWSS_LOG_ENTER();

    remove(SairedisRecFilename.c_str());

    Recorder recorder;

    recorder.enableRecording(true);

    sai_s32_list_t enum_values_capability { .count = 0, .list = nullptr };

    recorder.recordQueryAttributeEnumValuesCapability(
        switch_id,
        object_type,
        attr_id,
        &enum_values_capability
    );
    recorder.flush();

    auto tokens = parseFirstRecordedAPI();

    ASSERT_EQ(tokens, expectedOutput);
}

static void test_recorder_enum_value_capability_query_response(
    _In_ sai_status_t status,
    _In_ sai_object_type_t object_type,
    _In_ sai_attr_id_t attr_id,
    _In_ std::vector<int32_t> enumList,
    _In_ const std::vector<std::string>& expectedOutput)
{
    SWSS_LOG_ENTER();

    remove(SairedisRecFilename.c_str());

    Recorder recorder;

    recorder.enableRecording(true);

    sai_s32_list_t enum_values_capability;

    enum_values_capability.count = static_cast<int32_t>(enumList.size());
    enum_values_capability.list = enumList.data();

    recorder.recordQueryAttributeEnumValuesCapabilityResponse(
        status,
        object_type,
        attr_id,
        &enum_values_capability
    );
    recorder.flush();

    auto tokens = parseFirstRecordedAPI();

    ASSERT_EQ(tokens, expectedOutput);
}

static void test_recorder_enum_value_capability_query()
{
    SWSS_LOG_ENTER();

    test_recorder_enum_value_capability_query_request(
        1,
        SAI_OBJECT_TYPE_DEBUG_COUNTER,
        SAI_DEBUG_COUNTER_ATTR_TYPE,
        {
            "q",
            "attribute_enum_values_capability",
            "SAI_OBJECT_TYPE_SWITCH:oid:0x1",
            "SAI_DEBUG_COUNTER_ATTR_TYPE=0",
        }
    );

    test_recorder_enum_value_capability_query_response(
        SAI_STATUS_SUCCESS,
        SAI_OBJECT_TYPE_DEBUG_COUNTER,
        SAI_DEBUG_COUNTER_ATTR_TYPE,
        {
            SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS,
            SAI_DEBUG_COUNTER_TYPE_PORT_OUT_DROP_REASONS,
            SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS,
            SAI_DEBUG_COUNTER_TYPE_SWITCH_OUT_DROP_REASONS,
        },
        {
            "Q",
            "attribute_enum_values_capability",
            "SAI_STATUS_SUCCESS",
            "SAI_DEBUG_COUNTER_ATTR_TYPE=4:SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS,SAI_DEBUG_COUNTER_TYPE_PORT_OUT_DROP_REASONS,"
            "SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS,SAI_DEBUG_COUNTER_TYPE_SWITCH_OUT_DROP_REASONS",
        }
    );

    test_recorder_enum_value_capability_query_request(
        1,
        SAI_OBJECT_TYPE_DEBUG_COUNTER,
        SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST,
        {
            "q",
            "attribute_enum_values_capability",
            "SAI_OBJECT_TYPE_SWITCH:oid:0x1",
            "SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST=0",
        }
    );

    test_recorder_enum_value_capability_query_response(
        SAI_STATUS_SUCCESS,
        SAI_OBJECT_TYPE_DEBUG_COUNTER,
        SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST,
        {
            SAI_IN_DROP_REASON_L2_ANY,
            SAI_IN_DROP_REASON_L3_ANY
        },
        {
            "Q",
            "attribute_enum_values_capability",
            "SAI_STATUS_SUCCESS",
            "SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST=2:SAI_IN_DROP_REASON_L2_ANY,SAI_IN_DROP_REASON_L3_ANY"
        }
    );
}

void test_tokenize_bulk_route_entry()
{
    SWSS_LOG_ENTER();

    auto header = "2020-09-24.21:06:54.045505|C|SAI_OBJECT_TYPE_ROUTE_ENTRY";
    auto route = "||{\"dest\":\"20c1:bb0:0:80::/64\",\"switch_id\":\"oid:0x21000000000000\",\"vr\":\"oid:0x3000000000022\"}|SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID=oid:0x500000000066c";

    std::string line = header;

    int per = 100;

    for (int i = 0; i < per; i++)
    {
        line += route;
    }

    auto tstart = std::chrono::high_resolution_clock::now();

    int n = 1000;

    for (int c = 0; c < n; c++)
    {
        auto fields = tokenize(line, "||");

        auto first = fields.at(0); // timestamp|action|objecttype

        std::string str_object_type = swss::tokenize(first, '|').at(2);

        sai_object_type_t object_type = deserialize_object_type(str_object_type);

        std::vector<std::string> object_ids;

        std::vector<std::shared_ptr<SaiAttributeList>> attributes;

        for (size_t idx = 1; idx < fields.size(); ++idx)
        {
            // object_id|attr=value|...
            const std::string &joined = fields[idx];

            auto split = swss::tokenize(joined, '|');

            std::string str_object_id = split.front();

            object_ids.push_back(str_object_id);

            std::vector<swss::FieldValueTuple> entries; // attributes per object id

            SWSS_LOG_DEBUG("processing: %s", joined.c_str());

            for (size_t i = 1; i < split.size(); ++i)
            {
                const auto &item = split[i];

                auto start = item.find_first_of("=");

                auto field = item.substr(0, start);
                auto value = item.substr(start + 1);

                swss::FieldValueTuple entry(field, value);

                entries.push_back(entry);
            }

            // since now we converted this to proper list, we can extract attributes

            std::shared_ptr<SaiAttributeList> list =
                std::make_shared<SaiAttributeList>(object_type, entries, false);

            attributes.push_back(list);
        }
    }

    auto tend = std::chrono::high_resolution_clock::now();
    auto time = tend - tstart;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(time);

    std::cout << "ms: " << (double)us.count()/1000.0 / n << " n " << n << " / per " << per << std::endl;
    std::cout << "s: " << (double)us.count()/1000000.0 << " for total routes: " <<( n * per) << std::endl;
}

int main()
{
    SWSS_LOG_ENTER();

    std::cout << " * test tokenize bulk route entry" << std::endl;

    test_tokenize_bulk_route_entry();

    std::cout << " * test ContextConfigContainer" << std::endl;

    test_ContextConfigContainer();

    std::cout << " * test deserialize route_entry" << std::endl;

    test_deserialize_route_entry_meta(10000);
    test_deserialize_route_entry(10000);

    std::cout << " * test remove" << std::endl;

    test_serialize_remove_route_entry(10000);
    test_serialize_remove_fdb_entry(10000);
    test_serialize_remove_oid(10000);

    std::cout << " * test bulk remove" << std::endl;

    test_serialize_bulk_remove_route_entry(10,10000);
    test_serialize_bulk_remove_fdb_entry(10,10000);
    test_serialize_bulk_remove_oid(100,10000);

    std::cout << " * test create" << std::endl;

    test_serialize_create_route_entry(10000);
    test_serialize_create_fdb_entry(10000);
    test_serialize_create_oid(10000);

    std::cout << " * test bulk create" << std::endl;

    test_serialize_bulk_create_route_entry(10,10000);
    test_serialize_bulk_create_oid(10,10000);

    std::cout << " * test recorder" << std::endl;

    test_recorder_enum_value_capability_query();

    return 0;
}
