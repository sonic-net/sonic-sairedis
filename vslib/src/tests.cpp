#include <map>
#include <string>
#include <vector>

#include <unistd.h>

#include "swss/logger.h"
#include "swss/dbconnector.h"
#include "swss/schema.h"
#include "swss/notificationproducer.h"

#include "../../meta/sai_serialize.h"

extern "C" {
#include <sai.h>
}

#include "../inc/sai_vs.h"

const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("request: %s", variable);

    if (std::string(variable) == SAI_KEY_VS_SWITCH_TYPE)
    {
        return SAI_VALUE_VS_SWITCH_TYPE_MLNX2700;
    }

    return NULL;
}

int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    SWSS_LOG_INFO("iterator reached end");
    return -1;
}

sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

#define SUCCESS(x) \
    if ((x) != SAI_STATUS_SUCCESS) \
{\
    SWSS_LOG_THROW("expected success on: %s", #x);\
}

#define NOT_SUCCESS(x) \
    if ((x) == SAI_STATUS_SUCCESS) \
{\
    SWSS_LOG_THROW("expected failure, got success on: %s", #x);\
}

#define ASSERT_TRUE(x)\
{\
    if (!(x))\
    {\
        SWSS_LOG_THROW("assert true failed %s", #x);\
    }\
}

void on_switch_state_change(
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();
}

void on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();
}

void on_port_state_change(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();
}

void on_switch_shutdown_request()
{
    SWSS_LOG_ENTER();
}

void on_packet_event(
        _In_ const void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();
}

void test_ports()
{
    SWSS_LOG_ENTER();

    uint32_t expected_ports = 32;

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    SUCCESS(sai_metadata_sai_switch_api->create_switch(&switch_id, 1, &attr));

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    ASSERT_TRUE(attr.value.u32 == expected_ports);

    std::vector<sai_object_id_t> ports;

    ports.resize(expected_ports);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = expected_ports;
    attr.value.objlist.list = ports.data();

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    ASSERT_TRUE(attr.value.objlist.count == expected_ports);
}

void test_set_readonly_attribute()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    SUCCESS(sai_metadata_sai_switch_api->create_switch(&switch_id, 1, &attr));

    attr.id = SAI_SWITCH_ATTR_PORT_MAX_MTU;
    attr.value.u32 = 42;

    ASSERT_TRUE(sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr) != SAI_STATUS_SUCCESS);

    meta_unittests_enable(true);

    // allow set on readonly attribute
    SUCCESS(meta_unittests_allow_readonly_set_once(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_PORT_MAX_MTU));

    // set on readonly attribute should pass
    SUCCESS(sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr));

    // just scramble value to make sure that GET will succeed
    attr.value.u32 = 1;

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    ASSERT_TRUE(attr.value.u32 == 42);

    // second SET should fail
    ASSERT_TRUE(sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr) != SAI_STATUS_SUCCESS);
}

void test_set_readonly_attribute_via_redis()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    SUCCESS(sai_metadata_sai_switch_api->create_switch(&switch_id, 1, &attr));

    attr.id = SAI_SWITCH_ATTR_PORT_MAX_MTU;
    attr.value.u32 = 42;

    // this set should fail
    ASSERT_TRUE(sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr) != SAI_STATUS_SUCCESS);

    // this scope contains all operations needed to perform set operation on readonly attribute
    {
        swss::DBConnector db(ASIC_DB, "localhost", 6379, 0);
        swss::NotificationProducer vsntf(&db, SAI_VS_UNITTEST_CHANNEL);

        std::vector<swss::FieldValueTuple> entry;

        // needs to be done only once
        vsntf.send(SAI_VS_UNITTEST_ENABLE_UNITTESTS, "true", entry);

        std::string field = "SAI_SWITCH_ATTR_PORT_MAX_MTU";
        std::string value = "42"; // NOTE: normally we need sai_serialize_value()

        swss::FieldValueTuple fvt(field, value);

        entry.push_back(fvt);

        // when using tests from python, user will be required to translate VID2RID here
        // need RID here in form 0xYYYYYYYYYYYYYYY
        std::string data = "SAI_OBJECT_TYPE_SWITCH:" + sai_serialize_object_id(switch_id);

        vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, data, entry);
    }

    // give some time for notification to propagate to vs via redis
    usleep(200*1000);

    // just scramble value to make sure that GET will succeed
    attr.value.u32 = 1;

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    ASSERT_TRUE(attr.value.u32 == 42);

    // second SET should fail
    ASSERT_TRUE(sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr) != SAI_STATUS_SUCCESS);
}

void test_set_stats_via_redis()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    SUCCESS(sai_metadata_sai_switch_api->create_switch(&switch_id, 1, &attr));

    std::vector<sai_object_id_t> ports;

    uint32_t expected_ports = 32;

    ports.resize(expected_ports);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = expected_ports;
    attr.value.objlist.list = ports.data();

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    // this scope contains all operations needed to perform set stats on object
    {
        swss::DBConnector db(ASIC_DB, "localhost", 6379, 0);
        swss::NotificationProducer vsntf(&db, SAI_VS_UNITTEST_CHANNEL);

        std::vector<swss::FieldValueTuple> entry;

        // needs to be done only once
        vsntf.send(SAI_VS_UNITTEST_ENABLE_UNITTESTS, "true", entry);

        std::string field = "SAI_PORT_STAT_IF_IN_ERRORS";
        std::string value = "42"; // uint64_t

        swss::FieldValueTuple fvt(field, value);

        entry.push_back(fvt);

        std::string data = sai_serialize_object_id(ports.at(0)); // set on 1st port

        vsntf.send(SAI_VS_UNITTEST_SET_STATS_OP, data, entry);
    }

    // give some time for notification to propagate to vs via redis
    usleep(200*1000);

    sai_port_stat_t ids[1] = { SAI_PORT_STAT_IF_IN_ERRORS };
    uint64_t counters[1] = { 0 } ;

    SUCCESS(sai_metadata_sai_port_api->get_port_stats(ports.at(0), 1, (const sai_stat_id_t *)ids, counters));

    ASSERT_TRUE(counters[0] == 42);
}

void sai_fdb_event_notification(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("got fdb notification count %u", count);
}

sai_fdb_entry_t create_fdb_entry(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t vlan_id,
        _In_ sai_object_id_t bp_id,
        _In_ sai_fdb_entry_type_t type,
        _In_ uint8_t mac_end)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = type;

    sai_mac_t mac = {1,1,0,0,0,0};

    mac[5] = mac_end;

    sai_fdb_entry_t fe;

    fe.switch_id = switch_id;
    fe.bv_id = vlan_id;
    memcpy(fe.mac_address, mac, sizeof(sai_mac_t));

    SUCCESS(sai_metadata_sai_fdb_api->create_fdb_entry(&fe, 1, &attr));

    attr.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attr.value.oid = bp_id;

    SUCCESS(sai_metadata_sai_fdb_api->set_fdb_entry_attribute(&fe, &attr));

    return fe;
}

#define ASSERT_FDB_EXISTS(fdb) \
{\
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;\
    SUCCESS(sai_metadata_sai_fdb_api->get_fdb_entry_attribute(&fdb, 1, &attr));\
}

#define ASSERT_FDB_NOT_EXISTS(fdb) \
{\
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;\
    NOT_SUCCESS(sai_metadata_sai_fdb_api->get_fdb_entry_attribute(&fdb, 1, &attr));\
}

#define CREATE_ENTRIES()\
        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 0, NULL));\
        auto v2bp1d = create_fdb_entry(switch_id, vlan2_id, bp1, SAI_FDB_ENTRY_TYPE_DYNAMIC, 1);\
        auto v2bp1s = create_fdb_entry(switch_id, vlan2_id, bp1, SAI_FDB_ENTRY_TYPE_STATIC,  2);\
        auto v2bp2d = create_fdb_entry(switch_id, vlan2_id, bp2, SAI_FDB_ENTRY_TYPE_DYNAMIC, 3);\
        auto v2bp2s = create_fdb_entry(switch_id, vlan2_id, bp2, SAI_FDB_ENTRY_TYPE_STATIC,  4);\
        auto v3bp1d = create_fdb_entry(switch_id, vlan3_id, bp1, SAI_FDB_ENTRY_TYPE_DYNAMIC, 5);\
        auto v3bp1s = create_fdb_entry(switch_id, vlan3_id, bp1, SAI_FDB_ENTRY_TYPE_STATIC,  6);\
        auto v3bp2d = create_fdb_entry(switch_id, vlan3_id, bp2, SAI_FDB_ENTRY_TYPE_DYNAMIC, 7);\
        auto v3bp2s = create_fdb_entry(switch_id, vlan3_id, bp2, SAI_FDB_ENTRY_TYPE_STATIC,  8);

void test_fdb_flush()
{
    SWSS_LOG_ENTER();

    // TODO we need 10 cases, 10th where nothing was removed

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    SUCCESS(sai_metadata_sai_switch_api->create_switch(&switch_id, 1, &attr));

    // set notification callback

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attr.value.ptr = (void*)&sai_fdb_event_notification;

    SUCCESS(sai_metadata_sai_switch_api->set_switch_attribute(switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    sai_object_id_t bridge_id = attr.value.oid;

    sai_object_id_t list[100];

    attr.id = SAI_BRIDGE_ATTR_PORT_LIST;
    attr.value.objlist.count = 100;
    attr.value.objlist.list = list;

    SUCCESS(sai_metadata_sai_bridge_api->get_bridge_attribute(bridge_id, 1, &attr));

    sai_object_id_t bp1 = list[1];
    sai_object_id_t bp2 = list[2];

    SWSS_LOG_NOTICE("bridge ports: %s, %s",
            sai_serialize_object_id(bp1).c_str(),
            sai_serialize_object_id(bp2).c_str());

    // create 2nd vlan

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    sai_object_id_t vlan2_id;

    SUCCESS(sai_metadata_sai_vlan_api->create_vlan(&vlan2_id, switch_id, 1, &attr));

    SWSS_LOG_NOTICE("vlan 2 id: %s", sai_serialize_object_id(vlan2_id).c_str());

    // create 3rd vlan

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 3;

    sai_object_id_t vlan3_id;

    SUCCESS(sai_metadata_sai_vlan_api->create_vlan(&vlan3_id, switch_id, 1, &attr));

    SWSS_LOG_NOTICE("vlan 3 id: %s", sai_serialize_object_id(vlan3_id).c_str());

    SWSS_LOG_NOTICE("1. flush all entries");

    {
        CREATE_ENTRIES();

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 0, NULL));

        ASSERT_FDB_NOT_EXISTS(v2bp1d);
        ASSERT_FDB_NOT_EXISTS(v2bp1s);
        ASSERT_FDB_NOT_EXISTS(v2bp2d);
        ASSERT_FDB_NOT_EXISTS(v2bp2s);
        ASSERT_FDB_NOT_EXISTS(v3bp1d);
        ASSERT_FDB_NOT_EXISTS(v3bp1s);
        ASSERT_FDB_NOT_EXISTS(v3bp2d);
        ASSERT_FDB_NOT_EXISTS(v3bp2s);
    }

    SWSS_LOG_NOTICE("2a. flush dynamic entries");

    {
        CREATE_ENTRIES();

        attr.id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
        attr.value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 1, &attr));

        ASSERT_FDB_NOT_EXISTS(v2bp1d);
        ASSERT_FDB_EXISTS(v2bp1s);
        ASSERT_FDB_NOT_EXISTS(v2bp2d);
        ASSERT_FDB_EXISTS(v2bp2s);
        ASSERT_FDB_NOT_EXISTS(v3bp1d);
        ASSERT_FDB_EXISTS(v3bp1s);
        ASSERT_FDB_NOT_EXISTS(v3bp2d);
        ASSERT_FDB_EXISTS(v3bp2s);
    }

    SWSS_LOG_NOTICE("2b. flush static entries");

    {
        CREATE_ENTRIES();

        attr.id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
        attr.value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_STATIC;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 1, &attr));

        ASSERT_FDB_EXISTS(v2bp1d);
        ASSERT_FDB_NOT_EXISTS(v2bp1s);
        ASSERT_FDB_EXISTS(v2bp2d);
        ASSERT_FDB_NOT_EXISTS(v2bp2s);
        ASSERT_FDB_EXISTS(v3bp1d);
        ASSERT_FDB_NOT_EXISTS(v3bp1s);
        ASSERT_FDB_EXISTS(v3bp2d);
        ASSERT_FDB_NOT_EXISTS(v3bp2s);
    }

    SWSS_LOG_NOTICE("3a. flush vlan 2 entries");

    {
        CREATE_ENTRIES();

        attr.id = SAI_FDB_FLUSH_ATTR_BV_ID;
        attr.value.oid = vlan2_id;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 1, &attr));

        ASSERT_FDB_NOT_EXISTS(v2bp1d);
        ASSERT_FDB_NOT_EXISTS(v2bp1s);
        ASSERT_FDB_NOT_EXISTS(v2bp2d);
        ASSERT_FDB_NOT_EXISTS(v2bp2s);
        ASSERT_FDB_EXISTS(v3bp1d);
        ASSERT_FDB_EXISTS(v3bp1s);
        ASSERT_FDB_EXISTS(v3bp2d);
        ASSERT_FDB_EXISTS(v3bp2s);
    }

    SWSS_LOG_NOTICE("3b. flush vlan 3 entries");

    {
        CREATE_ENTRIES();

        attr.id = SAI_FDB_FLUSH_ATTR_BV_ID;
        attr.value.oid = vlan3_id;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 1, &attr));

        ASSERT_FDB_EXISTS(v2bp1d);
        ASSERT_FDB_EXISTS(v2bp1s);
        ASSERT_FDB_EXISTS(v2bp2d);
        ASSERT_FDB_EXISTS(v2bp2s);
        ASSERT_FDB_NOT_EXISTS(v3bp1d);
        ASSERT_FDB_NOT_EXISTS(v3bp1s);
        ASSERT_FDB_NOT_EXISTS(v3bp2d);
        ASSERT_FDB_NOT_EXISTS(v3bp2s);
    }

    SWSS_LOG_NOTICE("4a. flush bridge port 1 entries");

    {
        CREATE_ENTRIES();

        attr.id = SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID;
        attr.value.oid = bp1;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 1, &attr));

        ASSERT_FDB_NOT_EXISTS(v2bp1d);
        ASSERT_FDB_NOT_EXISTS(v2bp1s);
        ASSERT_FDB_EXISTS(v2bp2d);
        ASSERT_FDB_EXISTS(v2bp2s);
        ASSERT_FDB_NOT_EXISTS(v3bp1d);
        ASSERT_FDB_NOT_EXISTS(v3bp1s);
        ASSERT_FDB_EXISTS(v3bp2d);
        ASSERT_FDB_EXISTS(v3bp2s);
    }

    SWSS_LOG_NOTICE("4b. flush vlan 3 entries");

    {
        CREATE_ENTRIES();

        attr.id = SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID;
        attr.value.oid = bp2;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 1, &attr));

        ASSERT_FDB_EXISTS(v2bp1d);
        ASSERT_FDB_EXISTS(v2bp1s);
        ASSERT_FDB_NOT_EXISTS(v2bp2d);
        ASSERT_FDB_NOT_EXISTS(v2bp2s);
        ASSERT_FDB_EXISTS(v3bp1d);
        ASSERT_FDB_EXISTS(v3bp1s);
        ASSERT_FDB_NOT_EXISTS(v3bp2d);
        ASSERT_FDB_NOT_EXISTS(v3bp2s);
    }

    sai_attribute_t attrs[3];

    SWSS_LOG_NOTICE("5. flush vlan 2 and and type dynamic entries");

    {
        CREATE_ENTRIES();

        attrs[0].id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
        attrs[0].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;
        attrs[1].id = SAI_FDB_FLUSH_ATTR_BV_ID;
        attrs[1].value.oid = vlan2_id;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 2, attrs));

        ASSERT_FDB_NOT_EXISTS(v2bp1d);
        ASSERT_FDB_EXISTS(v2bp1s);
        ASSERT_FDB_NOT_EXISTS(v2bp2d);
        ASSERT_FDB_EXISTS(v2bp2s);
        ASSERT_FDB_EXISTS(v3bp1d);
        ASSERT_FDB_EXISTS(v3bp1s);
        ASSERT_FDB_EXISTS(v3bp2d);
        ASSERT_FDB_EXISTS(v3bp2s);
    }

    SWSS_LOG_NOTICE("6. flush vlan 2 and and bridge port 2 entries");

    {
        CREATE_ENTRIES();

        attrs[0].id = SAI_FDB_FLUSH_ATTR_BV_ID;
        attrs[0].value.oid = vlan2_id;
        attrs[1].id = SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID;
        attrs[1].value.oid = bp2;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 2, attrs));

        ASSERT_FDB_EXISTS(v2bp1d);
        ASSERT_FDB_EXISTS(v2bp1s);
        ASSERT_FDB_NOT_EXISTS(v2bp2d);
        ASSERT_FDB_NOT_EXISTS(v2bp2s);
        ASSERT_FDB_EXISTS(v3bp1d);
        ASSERT_FDB_EXISTS(v3bp1s);
        ASSERT_FDB_EXISTS(v3bp2d);
        ASSERT_FDB_EXISTS(v3bp2s);
    }

    SWSS_LOG_NOTICE("7. flush type dynamic and and bridge port 2 entries");

    {
        CREATE_ENTRIES();

        attrs[0].id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
        attrs[0].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;
        attrs[1].id = SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID;
        attrs[1].value.oid = bp2;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 2, attrs));

        ASSERT_FDB_EXISTS(v2bp1d);
        ASSERT_FDB_EXISTS(v2bp1s);
        ASSERT_FDB_NOT_EXISTS(v2bp2d);
        ASSERT_FDB_EXISTS(v2bp2s);
        ASSERT_FDB_EXISTS(v3bp1d);
        ASSERT_FDB_EXISTS(v3bp1s);
        ASSERT_FDB_NOT_EXISTS(v3bp2d);
        ASSERT_FDB_EXISTS(v3bp2s);
    }

    SWSS_LOG_NOTICE("8. flush type dynamic and and bridge port 2 and vlan 3 entries");

    {
        CREATE_ENTRIES();

        attrs[0].id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
        attrs[0].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;
        attrs[1].id = SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID;
        attrs[1].value.oid = bp2;
        attrs[2].id = SAI_FDB_FLUSH_ATTR_BV_ID;
        attrs[2].value.oid = vlan3_id;

        SUCCESS(sai_metadata_sai_fdb_api->flush_fdb_entries(switch_id, 3, attrs));

        ASSERT_FDB_EXISTS(v2bp1d);
        ASSERT_FDB_EXISTS(v2bp1s);
        ASSERT_FDB_EXISTS(v2bp2d);
        ASSERT_FDB_EXISTS(v2bp2s);
        ASSERT_FDB_EXISTS(v3bp1d);
        ASSERT_FDB_EXISTS(v3bp1s);
        ASSERT_FDB_NOT_EXISTS(v3bp2d);
        ASSERT_FDB_EXISTS(v3bp2s);
    }
}

void test_get_stats()
{
    SWSS_LOG_ENTER();

    uint32_t expected_ports = 32;

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    SUCCESS(sai_metadata_sai_switch_api->create_switch(&switch_id, 1, &attr));

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    ASSERT_TRUE(attr.value.u32 == expected_ports);

    std::vector<sai_object_id_t> ports;

    ports.resize(expected_ports);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = expected_ports;
    attr.value.objlist.list = ports.data();

    SUCCESS(sai_metadata_sai_switch_api->get_switch_attribute(switch_id, 1, &attr));

    ASSERT_TRUE(attr.value.objlist.count == expected_ports);

    sai_port_stat_t ids[2];

    ids[0] = SAI_PORT_STAT_IF_IN_OCTETS;
    ids[1] = SAI_PORT_STAT_IF_OUT_OCTETS;

    uint64_t values[2];

    values[0] = 42;
    values[1] = 42;

    SUCCESS(sai_metadata_sai_port_api->get_port_stats(ports[0], 2, (const sai_stat_id_t *)ids, values));

    ASSERT_TRUE(values[0] == 0);
    ASSERT_TRUE(values[1] == 0);

    meta_unittests_enable(true);

    values[0] = 77;
    values[1] = 127;

    // setting last bit of count value when unittest are enabled, will cause to perform SET on counters
    SUCCESS(sai_metadata_sai_port_api->get_port_stats(ports[0], 2 | 0x80000000, (const sai_stat_id_t *)ids, values));

    values[0] = 42;
    values[1] = 42;

    meta_unittests_enable(false);

    ids[0] = SAI_PORT_STAT_IF_OUT_OCTETS;
    ids[1] = SAI_PORT_STAT_IF_IN_OCTETS;

    SUCCESS(sai_metadata_sai_port_api->get_port_stats(ports[0], 2, (const sai_stat_id_t *)ids, values));

    ASSERT_TRUE(values[0] == 127);
    ASSERT_TRUE(values[1] == 77);
}

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    SUCCESS(sai_api_initialize(0, (sai_service_method_table_t*)&test_services));

    sai_apis_t apis;
    sai_metadata_apis_query(sai_api_query, &apis);

    //swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);

    test_ports();

    test_set_readonly_attribute();

    test_set_readonly_attribute_via_redis();

    test_fdb_flush();

    test_get_stats();

    test_set_stats_via_redis();

    // make proper uninitialize to close unittest thread
    sai_api_uninitialize();

    return 0;
}
