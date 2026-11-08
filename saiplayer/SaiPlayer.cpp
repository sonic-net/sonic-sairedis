#include "SaiPlayer.h"

#include "sairedis.h"
#include "sairediscommon.h"
#include "VirtualObjectIdManager.h"

#include "meta/sai_serialize.h"
#include "meta/PerformanceIntervalTimer.h"
#include "meta/SaiAttributeList.h"

#include "swss/logger.h"
#include "swss/tokenize.h"

#include <inttypes.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>

/*
 * Since this is player, we record actions from orchagent.  No special case
 * should be needed for switch in case it contains some oid values (like in
 * syncd cold restart) since orchagent should never create switch with oid
 * values set at creation time.
 */

using namespace saiplayer;
using namespace saimeta;
using namespace sairediscommon;
using namespace std::placeholders;

#define CALL_BULK_CREATE_API_WITH_TIMER(entry) \
                SWSS_LOG_INFO("executing BULK create "#entry", count = %zu ", entries.size()); \
                static PerformanceIntervalTimer timer("SaiPlayer::handle_bulk_entry::bulkCreate("#entry")"); \
                timer.start(); \
                status = m_sai->bulkCreate(object_count, entries.data(), attr_counts.data(), \
                                            attr_lists.data(), mode, statuses.data()); \
                timer.stop(); \
                timer.inc(entries.size());

#define CALL_BULK_REMOVE_API_WITH_TIMER(entry) \
                SWSS_LOG_INFO("executing BULK remove " #entry ", count = %zu ", entries.size()); \
                static PerformanceIntervalTimer timer("SaiPlayer::handle_bulk_entry::bulkRemove("#entry")"); \
                timer.start(); \
                status = m_sai->bulkRemove(object_count, entries.data(), \
                                            mode, statuses.data()); \
                timer.stop(); \
                timer.inc(entries.size());

#define CALL_BULK_SET_API_WITH_TIMER(entry) \
                SWSS_LOG_INFO("executing BULK set " #entry ", count = %zu ", entries.size()); \
                static PerformanceIntervalTimer timer("SaiPlayer::handle_bulk_entry::bulkSet("#entry")"); \
                timer.start(); \
                status = m_sai->bulkSet(object_count, entries.data(), \
                                        attr_lists.data(), mode, statuses.data()); \
                timer.stop(); \
                timer.inc(entries.size());

SaiPlayer::SaiPlayer(
        _In_ std::shared_ptr<sairedis::SaiInterface> sai,
        _In_ std::shared_ptr<CommandLineOptions> cmd):
    m_sai(sai),
    m_commandLineOptions(cmd)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("cmd: %s", cmd->getCommandLineString().c_str());

    loadProfileMap();

    if (cmd->m_contextConfig.size())
    {
        SWSS_LOG_NOTICE("using command line contextConfig instead SAI_REDIS_KEY_CONTEXT_CONFIG in profile.ini");

        m_profileMap[SAI_REDIS_KEY_CONTEXT_CONFIG] = cmd->m_contextConfig;
    }

    m_profileIter = m_profileMap.begin();

    m_smt.profileGetValue = std::bind(&SaiPlayer::profileGetValue, this, _1, _2);
    m_smt.profileGetNextValue = std::bind(&SaiPlayer::profileGetNextValue, this, _1, _2, _3);

    m_sn.onFdbEvent = std::bind(&SaiPlayer::onFdbEvent, this, _1, _2);
    m_sn.onPortStateChange = std::bind(&SaiPlayer::onPortStateChange, this, _1, _2);
    m_sn.onQueuePfcDeadlock = std::bind(&SaiPlayer::onQueuePfcDeadlock, this, _1, _2);
    m_sn.onSwitchAsicSdkHealthEvent = std::bind(&SaiPlayer::onSwitchAsicSdkHealthEvent, this, _1, _2, _3, _4, _5, _6);
    m_sn.onSwitchShutdownRequest = std::bind(&SaiPlayer::onSwitchShutdownRequest, this, _1);
    m_sn.onSwitchStateChange = std::bind(&SaiPlayer::onSwitchStateChange, this, _1, _2);
    m_sn.onBfdSessionStateChange = std::bind(&SaiPlayer::onBfdSessionStateChange, this, _1, _2);
    m_sn.onHaSetEvent = std::bind(&SaiPlayer::onHaSetEvent, this, _1, _2);
    m_sn.onHaScopeEvent = std::bind(&SaiPlayer::onHaScopeEvent, this, _1, _2);
    m_sn.onFlowBulkGetSessionEvent = std::bind(&SaiPlayer::onFlowBulkGetSessionEvent, this, _1, _2, _3);
    m_sn.onPortHostTxReady = std::bind(&SaiPlayer::onPortHostTxReady, this, _1, _2, _3);
    m_sn.onIcmpEchoSessionStateChange = std::bind(&SaiPlayer::onIcmpEchoSessionStateChange, this, _1, _2);
    m_switchNotifications= m_sn.getSwitchNotifications();
}

SaiPlayer::~SaiPlayer()
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::loadProfileMap()
{
    SWSS_LOG_ENTER();

    if (m_commandLineOptions->m_profileMapFile.size() == 0)
    {
        SWSS_LOG_NOTICE("profile map file not specified");
        return;
    }

    std::ifstream profile(m_commandLineOptions->m_profileMapFile);

    if (!profile.is_open())
    {
        SWSS_LOG_ERROR("failed to open profile map file: %s: %s",
                m_commandLineOptions->m_profileMapFile.c_str(),
                strerror(errno));

        exit(EXIT_FAILURE);
    }

    std::string line;

    while (getline(profile, line))
    {
        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        size_t pos = line.find("=");

        if (pos == std::string::npos)
        {
            SWSS_LOG_WARN("not found '=' in line %s", line.c_str());
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        m_profileMap[key] = value;

        SWSS_LOG_INFO("insert: %s:%s", key.c_str(), value.c_str());
    }
}

void SaiPlayer::onFdbEvent(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onPortStateChange(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onBfdSessionStateChange(
        _In_ uint32_t count,
        _In_ const sai_bfd_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onIcmpEchoSessionStateChange(
        _In_ uint32_t count,
        _In_ const sai_icmp_echo_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onHaSetEvent(
        _In_ uint32_t count,
        _In_ const sai_ha_set_event_data_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onHaScopeEvent(
        _In_ uint32_t count,
        _In_ const sai_ha_scope_event_data_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onFlowBulkGetSessionEvent(
        _In_ sai_object_id_t flow_bulk_session_id,
        _In_ uint32_t count,
        _In_ const sai_flow_bulk_get_session_event_data_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onPortHostTxReady(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_port_host_tx_ready_status_t host_tx_ready_status)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onQueuePfcDeadlock(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onSwitchAsicSdkHealthEvent(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t data,
        _In_ const sai_u8_list_t description)
{
    SWSS_LOG_ENTER();

    // empty
}

void SaiPlayer::onSwitchShutdownRequest(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("got shutdown request, syncd failed!");
    exit(EXIT_FAILURE);
}

void SaiPlayer::onSwitchStateChange(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    // empty
}

#define EXIT_ON_ERROR(x)\
{\
    sai_status_t s = (x);\
    if (s != SAI_STATUS_SUCCESS)\
    {\
        SWSS_LOG_THROW("fail status: %s", sai_serialize_status(s).c_str());\
    }\
}

sai_object_id_t SaiPlayer::translate_local_to_redis(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("translating local RID %s",
            sai_serialize_object_id(rid).c_str());

    if (rid == SAI_NULL_OBJECT_ID)
    {
        return SAI_NULL_OBJECT_ID;
    }

    auto it = m_local_to_redis.find(rid);

    if (it == m_local_to_redis.end())
    {
        SWSS_LOG_THROW("failed to translate local RID %s",
                sai_serialize_object_id(rid).c_str());
    }

    return it->second;
}

void SaiPlayer::translate_local_to_redis(
        _Inout_ sai_object_list_t& element)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < element.count; i++)
    {
        element.list[i] = translate_local_to_redis(element.list[i]);
    }
}

void SaiPlayer::translate_local_to_redis(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < attr_count; i++)
    {
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(object_type).c_str(),
                    attr.id);
        }

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                attr.value.oid = translate_local_to_redis(attr.value.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                translate_local_to_redis(attr.value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                if (attr.value.aclfield.enable)
                    attr.value.aclfield.data.oid = translate_local_to_redis(attr.value.aclfield.data.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                    translate_local_to_redis( attr.value.aclfield.data.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                if (attr.value.aclaction.enable)
                    attr.value.aclaction.parameter.oid = translate_local_to_redis(attr.value.aclaction.parameter.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                    translate_local_to_redis(attr.value.aclaction.parameter.objlist);
                break;

            default:

                if (meta->isoidattribute)
                {
                    SWSS_LOG_THROW("attribute %s is oid attribute but not handled, FIXME", meta->attridname);
                }

                break;
        }
    }
}

sai_object_type_t SaiPlayer::deserialize_object_type(
        _In_ const std::string& s)
{
    SWSS_LOG_ENTER();

    sai_object_type_t object_type;

    sai_deserialize_object_type(s, object_type);

    return object_type;
}

const std::vector<swss::FieldValueTuple> SaiPlayer::get_values(
        _In_ const std::vector<std::string>& items)
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

#define CHECK_LIST(x)                           \
    if (attr.x.count != get_attr.x.count) {     \
        SWSS_LOG_THROW("get response list count not match recording %u vs %u (expected)", get_attr.x.count, attr.x.count); }

void SaiPlayer::match_list_lengths(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t get_attr_count,
        _In_ sai_attribute_t* get_attr_list,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t* attr_list)
{
    SWSS_LOG_ENTER();

    if (get_attr_count != attr_count)
    {
        SWSS_LOG_THROW("list number don't match %u != %u (%s)", get_attr_count, attr_count,
                sai_serialize_object_type(object_type).c_str());
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t &get_attr = get_attr_list[i];
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(object_type).c_str(),
                    attr.id);
        }

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                CHECK_LIST(value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_UINT8_LIST:
                CHECK_LIST(value.u8list);
                break;

            case SAI_ATTR_VALUE_TYPE_INT8_LIST:
                CHECK_LIST(value.s8list);
                break;

            case SAI_ATTR_VALUE_TYPE_UINT16_LIST:
                CHECK_LIST(value.u16list);
                break;

            case SAI_ATTR_VALUE_TYPE_INT16_LIST:
                CHECK_LIST(value.s16list);
                break;

            case SAI_ATTR_VALUE_TYPE_UINT32_LIST:
                CHECK_LIST(value.u32list);
                break;

            case SAI_ATTR_VALUE_TYPE_INT32_LIST:
                CHECK_LIST(value.s32list);
                break;

            case SAI_ATTR_VALUE_TYPE_VLAN_LIST:
                CHECK_LIST(value.vlanlist);
                break;

            case SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
                CHECK_LIST(value.qosmap);
                break;

            case SAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
                CHECK_LIST(value.ipaddrlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                CHECK_LIST(value.aclfield.data.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
                CHECK_LIST(value.aclfield.data.u8list);
                CHECK_LIST(value.aclfield.mask.u8list);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                CHECK_LIST(value.aclaction.parameter.objlist);
                break;

            default:
                break;
        }
    }
}

void SaiPlayer::match_redis_with_rec(
        _In_ sai_object_id_t get_oid,
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    auto it = m_redis_to_local.find(get_oid);

    if (it == m_redis_to_local.end())
    {
        m_redis_to_local[get_oid] = oid;
        m_local_to_redis[oid] = get_oid;
    }

    if (oid != m_redis_to_local[get_oid])
    {
        SWSS_LOG_THROW("match failed, oid order is mismatch :( oid 0x%" PRIx64 " get_oid 0x%" PRIx64 " second 0x%" PRIx64,
                oid,
                get_oid,
                m_redis_to_local[get_oid]);
    }

    SWSS_LOG_DEBUG("map size: %zu", m_local_to_redis.size());
}

void SaiPlayer::match_redis_with_rec(
        _In_ sai_object_list_t get_objlist,
        _In_ sai_object_list_t objlist)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0 ; i < get_objlist.count; ++i)
    {
        match_redis_with_rec(get_objlist.list[i], objlist.list[i]);
    }
}

void SaiPlayer::match_redis_with_rec(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t get_attr_count,
        _In_ sai_attribute_t* get_attr_list,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t* attr_list)
{
    SWSS_LOG_ENTER();

    if (get_attr_count != attr_count)
    {
        SWSS_LOG_THROW("list number don't match %u != %u (%s)", get_attr_count, attr_count,
                sai_serialize_object_type(object_type).c_str());
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t &get_attr = get_attr_list[i];
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(object_type).c_str(),
                    attr.id);
        }

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                match_redis_with_rec(get_attr.value.oid, attr.value.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                match_redis_with_rec(get_attr.value.objlist, attr.value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                if (attr.value.aclfield.enable)
                    match_redis_with_rec(get_attr.value.aclfield.data.oid, attr.value.aclfield.data.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                    match_redis_with_rec(get_attr.value.aclfield.data.objlist, attr.value.aclfield.data.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                if (attr.value.aclaction.enable)
                    match_redis_with_rec(get_attr.value.aclaction.parameter.oid, attr.value.aclaction.parameter.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                    match_redis_with_rec(get_attr.value.aclaction.parameter.objlist, attr.value.aclaction.parameter.objlist);
                break;

            default:

                // XXX if (meta->isoidattribute)
                if (meta->allowedobjecttypeslength > 0)
                {
                    SWSS_LOG_THROW("attribute %s is oid attribute but not handled, FIXME", meta->attridname);
                }

                break;
        }
    }
}

sai_status_t SaiPlayer::handle_fdb(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_fdb_entry_t fdb_entry;
    sai_deserialize_fdb_entry(str_object_id, fdb_entry);

    fdb_entry.switch_id = translate_local_to_redis(fdb_entry.switch_id);
    fdb_entry.bv_id = translate_local_to_redis(fdb_entry.bv_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&fdb_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&fdb_entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&fdb_entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&fdb_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("fdb other apis not implemented");
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SaiPlayer::handle_neighbor(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_neighbor_entry_t neighbor_entry;
    sai_deserialize_neighbor_entry(str_object_id, neighbor_entry);

    neighbor_entry.switch_id = translate_local_to_redis(neighbor_entry.switch_id);
    neighbor_entry.rif_id = translate_local_to_redis(neighbor_entry.rif_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&neighbor_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&neighbor_entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&neighbor_entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&neighbor_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("neighbor other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_route(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_route_entry_t route_entry;
    sai_deserialize_route_entry(str_object_id, route_entry);

    route_entry.switch_id = translate_local_to_redis(route_entry.switch_id);
    route_entry.vr_id = translate_local_to_redis(route_entry.vr_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&route_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&route_entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&route_entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&route_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("route other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_inseg(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_inseg_entry_t inseg_entry;
    sai_deserialize_inseg_entry(str_object_id, inseg_entry);

    inseg_entry.switch_id = translate_local_to_redis(inseg_entry.switch_id);

    switch(api)
    {
        case SAI_COMMON_API_CREATE:
            return sai_metadata_sai_mpls_api->create_inseg_entry(&inseg_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_metadata_sai_mpls_api->remove_inseg_entry(&inseg_entry);

        case SAI_COMMON_API_SET:
            return sai_metadata_sai_mpls_api->set_inseg_entry_attribute(&inseg_entry, attr_list);

        case SAI_COMMON_API_GET:
            return sai_metadata_sai_mpls_api->get_inseg_entry_attribute(&inseg_entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("inseg other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_direction_lookup(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_direction_lookup_entry_t entry;
    sai_deserialize_direction_lookup_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH direction_lookup other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_eni_ether_address_map(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_eni_ether_address_map_entry_t entry;
    sai_deserialize_eni_ether_address_map_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH eni_ether_address_map_entry other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_vip(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_vip_entry_t entry;
    sai_deserialize_vip_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH vip other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_inbound_routing(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_inbound_routing_entry_t entry;
    sai_deserialize_inbound_routing_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);
    entry.eni_id = translate_local_to_redis(entry.eni_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH inbound_routing other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_pa_validation(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_pa_validation_entry_t entry;
    sai_deserialize_pa_validation_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);
    entry.vnet_id = translate_local_to_redis(entry.vnet_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH pa_validation other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_outbound_routing(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_outbound_routing_entry_t entry;
    sai_deserialize_outbound_routing_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);
    entry.outbound_routing_group_id = translate_local_to_redis(entry.outbound_routing_group_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH outbound_routing other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_outbound_ca_to_pa(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_outbound_ca_to_pa_entry_t entry;
    sai_deserialize_outbound_ca_to_pa_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);
    entry.dst_vnet_id = translate_local_to_redis(entry.dst_vnet_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH outbound_ca_to_pa other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_outbound_port_map_port_range(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_outbound_port_map_port_range_entry_t entry;
    sai_deserialize_outbound_port_map_port_range_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);
    entry.outbound_port_map_id = translate_local_to_redis(entry.outbound_port_map_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH outbound_port_map_port_range other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_global_trusted_vni(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_global_trusted_vni_entry_t entry;
    sai_deserialize_global_trusted_vni_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH global_trusted_vni other apis not implemented");
    }
}

sai_status_t SaiPlayer::handle_dash_eni_trusted_vni(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_eni_trusted_vni_entry_t entry;
    sai_deserialize_eni_trusted_vni_entry(str_object_id, entry);

    entry.switch_id = translate_local_to_redis(entry.switch_id);
    entry.eni_id = translate_local_to_redis(entry.eni_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_sai->create(&entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_sai->remove(&entry);

        case SAI_COMMON_API_SET:
            return m_sai->set(&entry, attr_list);

        case SAI_COMMON_API_GET:
            return m_sai->get(&entry, attr_count, attr_list);

        default:
            SWSS_LOG_THROW("DASH ENI_TRUSTED_VNI_ENTRY other APIs not implemented");
    }
}

void SaiPlayer::update_notifications_pointers(
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Sairedis is updating notifications pointers based on attribute, so when
     * we will do replay it will have invalid pointers from orchagent, so we
     * need to override them after create, and after set.
     */

    sai_metadata_update_attribute_notification_pointers(&m_switchNotifications, attr_count, attr_list);
}

sai_status_t SaiPlayer::handle_generic(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t local_id;
    sai_deserialize_object_id(str_object_id, local_id);

    SWSS_LOG_DEBUG("generic %s for %s:%s",
            sai_serialize_common_api(api).c_str(),
            sai_serialize_object_type(object_type).c_str(),
            str_object_id.c_str());

    sai_object_meta_key_t meta_key;

    meta_key.objecttype = object_type;

    std::vector<sai_attribute_t> swattr;

    switch (api)
    {
        case SAI_COMMON_API_CREATE:

            {
                sai_object_id_t switch_id = m_sai->switchIdQuery(local_id);

                if (switch_id == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_THROW("invalid switch_id translated from VID %s",
                            sai_serialize_object_id(local_id).c_str());
                }

                if (object_type == SAI_OBJECT_TYPE_SWITCH)
                {
                    update_notifications_pointers(attr_count, attr_list);

                    /*
                     * We are creating switch, in both cases local and redis
                     * switch id is deterministic and should be the same.
                     */

                    /*
                     * Since now recording could contain switches from multiple
                     * contexts, we need to pass extra attribute to point to
                     * the right context when creating switch.
                     */

                    swattr.resize(attr_count + 1);

                    memcpy(swattr.data(), attr_list, attr_count * sizeof(sai_attribute_t));

                    swattr[attr_count].id = SAI_REDIS_SWITCH_ATTR_CONTEXT;
                    swattr[attr_count].value.u32 = sairedis::VirtualObjectIdManager::getGlobalContext(switch_id);

                    attr_count++;

                    attr_list = swattr.data();
                }
                else
                {
                    /*
                     * When we creating switch, then switch_id parameter is
                     * ignored, but we can't convert it using vid to rid map,
                     * since rid don't exist yet, so skip translate for switch,
                     * but use translate for all other objects.
                     */

                    switch_id = translate_local_to_redis(switch_id);
                }

                sai_status_t status = m_sai->create(meta_key, switch_id, attr_count, attr_list);

                if (status == SAI_STATUS_SUCCESS)
                {
                    sai_object_id_t rid = meta_key.objectkey.key.object_id;

                    match_redis_with_rec(rid, local_id);

                    SWSS_LOG_INFO("saved VID %s to RID %s",
                            sai_serialize_object_id(local_id).c_str(),
                            sai_serialize_object_id(rid).c_str());
                }
                else
                {
                    SWSS_LOG_ERROR("failed to create %s",
                            sai_serialize_status(status).c_str());
                }

                return status;
            }

        case SAI_COMMON_API_REMOVE:

            {
                meta_key.objectkey.key.object_id = translate_local_to_redis(local_id);

                return m_sai->remove(meta_key);
            }

        case SAI_COMMON_API_SET:

            {
                if (object_type == SAI_OBJECT_TYPE_SWITCH)
                {
                    update_notifications_pointers(1, attr_list);
                }

                meta_key.objectkey.key.object_id = translate_local_to_redis(local_id);

                return m_sai->set(meta_key, attr_list);
            }

        case SAI_COMMON_API_GET:

            {
                meta_key.objectkey.key.object_id = translate_local_to_redis(local_id);

                return m_sai->get(meta_key, attr_count, attr_list);
            }

        default:
            SWSS_LOG_THROW("generic other apis not implemented");
    }
}

void SaiPlayer::handle_get_response(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t get_attr_count,
        _In_ sai_attribute_t* get_attr_list,
        _In_ const std::string& response,
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    // timestamp|action|objecttype:objectid|attrid=value,...
    auto v = swss::tokenize(response, '|');

    if (status != SAI_STATUS_SUCCESS)
    {
        sai_status_t expectedStatus;
        sai_deserialize_status(v.at(2), expectedStatus);

        if (status == expectedStatus)
        {
            // GET api was not successful but status is equal to recording
            return;
        }

        SWSS_LOG_WARN("status is: %s but expected: %s",
                sai_serialize_status(status).c_str(),
                sai_serialize_status(expectedStatus).c_str());
        return;
    }

    //std::cout << "processing " << response << std::endl;

    auto values = get_values(v);

    SaiAttributeList list(object_type, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    match_list_lengths(object_type, get_attr_count, get_attr_list, attr_count, attr_list);

    SWSS_LOG_DEBUG("list match");

    match_redis_with_rec(object_type, get_attr_count, get_attr_list, attr_count, attr_list);

    // NOTE: Primitive values are not matched (recording vs switch/vs), we can add that check
}

void SaiPlayer::performSleep(
        _In_ const std::string& line)
{
    SWSS_LOG_ENTER();

    // timestamp|action|sleeptime
    auto v = swss::tokenize(line, '|');

    if (v.size() < 3)
    {
        SWSS_LOG_THROW("invalid line %s", line.c_str());
    }

    uint32_t useconds;
    sai_deserialize_number(v[2], useconds);

    if (useconds > 0)
    {
        useconds *= 1000; // 1ms resolution is enough for sleep

        SWSS_LOG_NOTICE("usleep(%u)", useconds);
        usleep(useconds);
    }
}

void SaiPlayer::performNotifySyncd(
        _In_ const std::string& request,
        _In_ const std::string& response)
{
    SWSS_LOG_ENTER();

    // timestamp|action|data
    auto r = swss::tokenize(request, '|');
    auto R = swss::tokenize(response, '|');

    if (r[1] != "a" || R[1] != "A")
    {
        SWSS_LOG_THROW("invalid syncd notify request/response %s/%s", request.c_str(), response.c_str());
    }

    if (m_commandLineOptions->m_skipNotifySyncd)
    {
        SWSS_LOG_NOTICE("skipping notify syncd, selected by user");
        return;
    }

    // tell syncd that we are compiling new view
    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
    attr.value.s32 = sai_deserialize_redis_notify_syncd(r[2]);

    /*
     * NOTE: We don't need actual switch to set those attributes.
     */

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    sai_status_t status = m_sai->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    const std::string& responseStatus = R[2];

    sai_status_t response_status;
    sai_deserialize_status(responseStatus, response_status);

    if (status != response_status)
    {
        SWSS_LOG_THROW("response status %s is different than syncd status %s",
                responseStatus.c_str(),
                sai_serialize_status(status).c_str());
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to notify syncd %s",
                sai_serialize_status(status).c_str());
    }

    // OK
}

void SaiPlayer::performFdbFlush(
        _In_ const std::string& request,
        _In_ const std::string response)
{
    SWSS_LOG_ENTER();

    // 2017-05-13.20:47:24.883499|f|SAI_OBJECT_TYPE_FDB_FLUSH:oid:0x21000000000000|
    // 2017-05-13.20:47:24.883499|F|SAI_STATUS_SUCCESS

    // timestamp|action|data
    auto r = swss::tokenize(request, '|');
    auto R = swss::tokenize(response, '|');

    if (r[1] != "f" || R[1] != "F")
    {
        SWSS_LOG_THROW("invalid fdb flush request/response %s/%s", request.c_str(), response.c_str());
    }

    if (r.size() > 3 && r[3].size() != 0)
    {
        SWSS_LOG_NOTICE("%zu %zu, %s", r.size(), r[3].size(), r[3].c_str());
        // TODO currently we support only flush fdb entries with no attributes
        SWSS_LOG_THROW("currently fdb flush supports only no attributes, but some given: %s", request.c_str());
    }

    // objecttype:objectid (object id may contain ':')
    auto& data = r[2];
    auto start = data.find_first_of(":");
    auto str_object_type = data.substr(0, start);
    auto str_object_id  = data.substr(start + 1);

    sai_object_type_t ot = deserialize_object_type(str_object_type);

    if (ot != SAI_OBJECT_TYPE_FDB_FLUSH)
    {
        SWSS_LOG_THROW("expected object type %s, but got: %s on %s",
                sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_FLUSH).c_str(),
                str_object_type.c_str(),
                request.c_str());
    }

    sai_object_id_t local_switch_id;
    sai_deserialize_object_id(str_object_id, local_switch_id);

    if (m_sai->switchIdQuery(local_switch_id) != local_switch_id)
    {
        SWSS_LOG_THROW("fdb flush object is not switch id: %s, switch_id_query: %s",
                str_object_id.c_str(),
                sai_serialize_object_id(m_sai->switchIdQuery(local_switch_id)).c_str());
    }

    auto switch_id = translate_local_to_redis(local_switch_id);

    // TODO currently we support only flush fdb entries with no attributes
    sai_status_t status = m_sai->flushFdbEntries(switch_id, 0, NULL);

    // check status
    sai_status_t expected_status;
    sai_deserialize_status(R[2], expected_status);

    if (status != expected_status)
    {
        SWSS_LOG_THROW("fdb flush got status %s, but expecting: %s",
                sai_serialize_status(status).c_str(),
                R[2].c_str());
    }

    // fdb flush OK
}

std::vector<std::string> SaiPlayer::tokenize(
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

sai_status_t SaiPlayer::handle_bulk_route(
        _In_ const std::vector<std::string> &object_ids,
        _In_ sai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>> &attributes,
        _In_ const std::vector<sai_status_t>& expectedStatuses)
{
    SWSS_LOG_ENTER();

    std::vector<sai_route_entry_t> routes;

    for (size_t i = 0; i < object_ids.size(); ++i)
    {
        sai_route_entry_t route_entry;
        sai_deserialize_route_entry(object_ids[i], route_entry);

        route_entry.vr_id = translate_local_to_redis(route_entry.vr_id);

        routes.push_back(route_entry);

        SWSS_LOG_DEBUG("route: %s", object_ids[i].c_str());
    }

    std::vector<sai_status_t> statuses;

    statuses.resize(expectedStatuses.size());

    if (api == SAI_COMMON_API_BULK_SET)
    {
        /*
         * TODO: since SDK don't support bulk route api yet, we just use our
         * implementation, and later on we can switch to SDK api.
         *
         * TODO: we need to get operation type from recording, currently is not
         * serialized and it is hard coded here.
         */

        std::vector<sai_attribute_t> attrs;

        for (const auto &a: attributes)
        {
            /*
             * Set has only 1 attribute, so we can just join them nicely here.
             */

            attrs.push_back(a->get_attr_list()[0]);
        }

        SWSS_LOG_INFO("executing BULK set route_entry with %zu routes", routes.size());

        sai_status_t status = m_sai->bulkSet(
                (uint32_t)routes.size(),
                routes.data(),
                attrs.data(),
                SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, // TODO we need to get that from recording
                statuses.data());

        // even if API will fail, we swill need to compare all statuses for each entry

        for (size_t i = 0; i < statuses.size(); ++i)
        {
            if (statuses[i] != expectedStatuses[i])
            {
                /*
                 * If recorded statuses are different than received, throw
                 * exception since data don't match.
                 */

                SWSS_LOG_THROW("recorded status is %s but returned is %s on %s",
                        sai_serialize_status(expectedStatuses[i]).c_str(),
                        sai_serialize_status(statuses[i]).c_str(),
                        object_ids[i].c_str());
            }
        }

        return status;
    }
    else if (api == SAI_COMMON_API_BULK_CREATE)
    {
        std::vector<uint32_t> attr_count;

        std::vector<const sai_attribute_t*> attr_list;

        // route can have multiple attributes, so we need to handle them all
        for (const auto &alist: attributes)
        {
            attr_list.push_back(alist->get_attr_list());
            attr_count.push_back(alist->get_attr_count());
        }

        SWSS_LOG_INFO("executing BULK create route_entry with %zu routes", routes.size());

        static PerformanceIntervalTimer timer("SaiPlayer::handle_bulk_route::bulkCreate(route_entry)");

        timer.start();

        sai_status_t status = m_sai->bulkCreate(
                (uint32_t)routes.size(),
                routes.data(),
                attr_count.data(),
                attr_list.data(),
                SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, // TODO we need to get that from recording
                statuses.data());

        timer.stop();

        timer.inc(routes.size());

        for (size_t i = 0; i < statuses.size(); ++i)
        {
            if (statuses[i] != expectedStatuses[i])
            {
                /*
                 * If recorded statuses are different than received, throw
                 * exception since data don't match.
                 */

                SWSS_LOG_THROW("recorded status is %s but returned is %s on %s",
                        sai_serialize_status(expectedStatuses[i]).c_str(),
                        sai_serialize_status(statuses[i]).c_str(),
                        object_ids[i].c_str());
            }
        }

        return status;

    }
    else
    {
        SWSS_LOG_THROW("api %d is not supported in bulk route", api);
    }
}

sai_status_t SaiPlayer::handle_bulk_generic(
        _In_ sai_object_type_t objectType,
        _In_ const std::vector<std::string>& strObjectIds,
        _In_ sai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>>& attributes,
        _In_ const std::vector<sai_status_t>& expectedStatuses)
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> objectIds;

    for (const auto& oid: strObjectIds)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(oid, objectId);

        if (api != SAI_COMMON_API_BULK_CREATE)
        {
            // when creating OID objects, ID don't exists yet, it needs to be created
            // and after success create, it should be matched to local redis db
            objectId = translate_local_to_redis(objectId);
        }

        objectIds.push_back(objectId);
    }

    std::vector<sai_status_t> statuses;

    statuses.resize(expectedStatuses.size());

    if (api == SAI_COMMON_API_BULK_CREATE)
    {
        std::vector<sai_object_id_t> oids;

        oids.resize(expectedStatuses.size());

        std::vector<uint32_t> attr_count;

        std::vector<const sai_attribute_t*> attr_list;

        // object can have multiple attributes, so we need to handle them all
        for (const auto &alist: attributes)
        {
            attr_list.push_back(alist->get_attr_list());
            attr_count.push_back(alist->get_attr_count());
        }

        SWSS_LOG_INFO("executing BULK create %s with %zu entries",
                sai_serialize_object_type(objectType).c_str(),
                objectIds.size());

        // get switch ID from first local object
        sai_object_id_t localSwitchId = m_sai->switchIdQuery(objectIds[0]);
        sai_object_id_t switchId = translate_local_to_redis(localSwitchId);

        sai_status_t status = m_sai->bulkCreate(
                objectType,
                switchId,
                (uint32_t)oids.size(),
                attr_count.data(),
                attr_list.data(),
                SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, // TODO we need to get that from recording
                oids.data(),
                statuses.data());

        for (size_t i = 0; i < statuses.size(); ++i)
        {
            if (statuses[i] != expectedStatuses[i])
            {
                /*
                 * If recorded statuses are different than received, throw
                 * exception since data don't match.
                 */

                SWSS_LOG_THROW("recorded status is %s but returned is %s on %s",
                        sai_serialize_status(expectedStatuses[i]).c_str(),
                        sai_serialize_status(statuses[i]).c_str(),
                        strObjectIds[i].c_str());
            }

            if (statuses[i] == SAI_STATUS_SUCCESS)
            {
                // object was created, we need to match local id and redis id
                match_redis_with_rec(oids[i], objectIds[i]);
            }
        }

        return status;
    }
    else if (api == SAI_COMMON_API_BULK_REMOVE)
    {
        SWSS_LOG_INFO("executing BULK remove %s with %zu entries",
                sai_serialize_object_type(objectType).c_str(),
                objectIds.size());

        sai_status_t status = m_sai->bulkRemove(
                objectType,
                (uint32_t)objectIds.size(),
                objectIds.data(),
                SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, // TODO we need to get that from recording
                statuses.data());

        // even if API will fail, we swill need to compare all statuses for each entry

        for (size_t i = 0; i < statuses.size(); ++i)
        {
            if (statuses[i] != expectedStatuses[i])
            {
                /*
                 * If recorded statuses are different than received, throw
                 * exception since data don't match.
                 */

                SWSS_LOG_THROW("recorded status is %s but returned is %s on %s",
                        sai_serialize_status(expectedStatuses[i]).c_str(),
                        sai_serialize_status(statuses[i]).c_str(),
                        strObjectIds[i].c_str());
            }
        }

        return status;
    }
    else if (api == SAI_COMMON_API_BULK_SET)
    {
        std::vector<sai_attribute_t> attrs;

        for (const auto &a: attributes)
        {
            // set has only 1 attribute, so we can just join them nicely here.

            attrs.push_back(a->get_attr_list()[0]);
        }

        SWSS_LOG_INFO("executing BULK set %s with %zu entries",
                sai_serialize_object_type(objectType).c_str(),
                objectIds.size());

        sai_status_t status = m_sai->bulkSet(
                objectType,
                (uint32_t)objectIds.size(),
                objectIds.data(),
                attrs.data(),
                SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, // TODO we need to get that from recording
                statuses.data());

        // even if API will fail, we swill need to compare all statuses for each entry

        for (size_t i = 0; i < statuses.size(); ++i)
        {
            if (statuses[i] != expectedStatuses[i])
            {
                /*
                 * If recorded statuses are different than received, throw
                 * exception since data don't match.
                 */

                SWSS_LOG_THROW("recorded status is %s but returned is %s on %s",
                        sai_serialize_status(expectedStatuses[i]).c_str(),
                        sai_serialize_status(statuses[i]).c_str(),
                        strObjectIds[i].c_str());
            }
        }

        return status;
    }
    else
    {
        SWSS_LOG_THROW("api %s is not supported in bulk %s",
                sai_serialize_common_api(api).c_str(),
                sai_serialize_object_type(objectType).c_str());
    }
}

sai_status_t SaiPlayer::handle_bulk_entry(
        _In_ const std::vector<std::string> &object_ids,
        _In_ sai_object_type_t object_type,
        _In_ sai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>> &attributes,
        _Out_ std::vector<sai_status_t>& statuses)
{
    SWSS_LOG_ENTER();

    sai_bulk_op_error_mode_t mode = SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR; // TODO we need to get that from recording

    uint32_t object_count = (uint32_t) object_ids.size();

    sai_status_t status = SAI_STATUS_SUCCESS;

    if (api == SAI_COMMON_API_BULK_CREATE)
    {
        std::vector<uint32_t> attr_counts(object_count);
        std::vector<const sai_attribute_t*> attr_lists(object_count);

        for (uint32_t idx = 0; idx < object_count; idx++)
        {
            attr_counts[idx] = attributes[idx]->get_attr_count();
            attr_lists[idx] = attributes[idx]->get_attr_list();
        }

        switch ((int)object_type)
        {
            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            {
                std::vector<sai_route_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_route_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].vr_id = translate_local_to_redis(entries[it].vr_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("route_entry");

            }
            break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            {
                std::vector<sai_neighbor_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_neighbor_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].rif_id = translate_local_to_redis(entries[it].rif_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("neighbor_entry");

            }
            break;

            case SAI_OBJECT_TYPE_FDB_ENTRY:
            {

                std::vector<sai_fdb_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_fdb_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].bv_id = translate_local_to_redis(entries[it].bv_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("fdb_entry");

            }
            break;

            case SAI_OBJECT_TYPE_NAT_ENTRY:
            {

                std::vector<sai_nat_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_nat_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].vr_id = translate_local_to_redis(entries[it].vr_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("nat_entry");

            }
            break;

            case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
            {
                std::vector<sai_direction_lookup_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_direction_lookup_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("direction_lookup_entry");

            }
            break;

            case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
            {
                std::vector<sai_eni_ether_address_map_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_eni_ether_address_map_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("eni_ether_address_map_entry");

            }
            break;

            case SAI_OBJECT_TYPE_VIP_ENTRY:
            {
                std::vector<sai_vip_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_vip_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("vip_entry");

            }
            break;

            case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
            {
                std::vector<sai_inbound_routing_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_inbound_routing_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].eni_id = translate_local_to_redis(entries[it].eni_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("inbound_routing_entry");

            }
            break;

            case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
            {
                std::vector<sai_pa_validation_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_pa_validation_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].vnet_id = translate_local_to_redis(entries[it].vnet_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("pa_validation_entry");

            }
            break;

            case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
            {
                std::vector<sai_outbound_routing_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_outbound_routing_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].outbound_routing_group_id = translate_local_to_redis(entries[it].outbound_routing_group_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("outbound_routing_entry");

            }
            break;

            case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
            {
                std::vector<sai_outbound_ca_to_pa_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_outbound_ca_to_pa_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].dst_vnet_id = translate_local_to_redis(entries[it].dst_vnet_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("outbound_ca_to_pa_entry");

            }
            break;

            case SAI_OBJECT_TYPE_OUTBOUND_PORT_MAP_PORT_RANGE_ENTRY:
            {
                std::vector<sai_outbound_port_map_port_range_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_outbound_port_map_port_range_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].outbound_port_map_id = translate_local_to_redis(entries[it].outbound_port_map_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("outbound_port_map_port_range_entry");

            }
            break;

            case SAI_OBJECT_TYPE_GLOBAL_TRUSTED_VNI_ENTRY:
            {
                std::vector<sai_global_trusted_vni_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_global_trusted_vni_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("global_trusted_vni_entry");

            }
            break;

            case SAI_OBJECT_TYPE_ENI_TRUSTED_VNI_ENTRY:
            {
                std::vector<sai_eni_trusted_vni_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_eni_trusted_vni_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].eni_id = translate_local_to_redis(entries[it].eni_id);
                }

                CALL_BULK_CREATE_API_WITH_TIMER("eni_trusted_vni_entry");

            }
            break;

            default:
                SWSS_LOG_THROW("api %s is not supported in bulk", sai_serialize_common_api(api).c_str());
        }
    }
    else if (api == SAI_COMMON_API_BULK_REMOVE)
    {
        switch ((int)object_type)
        {
            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            {

                std::vector<sai_route_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_route_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].vr_id = translate_local_to_redis(entries[it].vr_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("route_entry");

            }
            break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            {
                std::vector<sai_neighbor_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_neighbor_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].rif_id = translate_local_to_redis(entries[it].rif_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("neighbor_entry");

            }
            break;

            case SAI_OBJECT_TYPE_FDB_ENTRY:
            {

                std::vector<sai_fdb_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_fdb_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].bv_id = translate_local_to_redis(entries[it].bv_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("fdb_entry");

            }
            break;

            case SAI_OBJECT_TYPE_NAT_ENTRY:
            {

                std::vector<sai_nat_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_nat_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].vr_id = translate_local_to_redis(entries[it].vr_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("nat_entry");

            }
            break;

            case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
            {
                std::vector<sai_direction_lookup_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_direction_lookup_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("direction_lookup_entry");

            }
            break;

            case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
            {
                std::vector<sai_eni_ether_address_map_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_eni_ether_address_map_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("eni_ether_address_map_entry");

            }
            break;

            case SAI_OBJECT_TYPE_VIP_ENTRY:
            {
                std::vector<sai_vip_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_vip_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("vip_entry");

            }
            break;

            case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
            {
                std::vector<sai_inbound_routing_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_inbound_routing_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].eni_id = translate_local_to_redis(entries[it].eni_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("inbound_routing_entry");

            }
            break;

            case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
            {
                std::vector<sai_pa_validation_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_pa_validation_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].vnet_id = translate_local_to_redis(entries[it].vnet_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("pa_validation_entry");

            }
            break;

            case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
            {
                std::vector<sai_outbound_routing_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_outbound_routing_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].outbound_routing_group_id = translate_local_to_redis(entries[it].outbound_routing_group_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("outbound_routing_entry");

            }
            break;

            case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
            {
                std::vector<sai_outbound_ca_to_pa_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_outbound_ca_to_pa_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].dst_vnet_id = translate_local_to_redis(entries[it].dst_vnet_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("outbound_ca_to_pa_entry");

            }
            break;

            case SAI_OBJECT_TYPE_OUTBOUND_PORT_MAP_PORT_RANGE_ENTRY:
            {
                std::vector<sai_outbound_port_map_port_range_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_outbound_port_map_port_range_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].outbound_port_map_id = translate_local_to_redis(entries[it].outbound_port_map_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("outbound_port_map_port_range_entry");

            }
            break;

            case SAI_OBJECT_TYPE_GLOBAL_TRUSTED_VNI_ENTRY:
            {
                std::vector<sai_global_trusted_vni_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_global_trusted_vni_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("global_trusted_vni_entry");

            }
            break;

            case SAI_OBJECT_TYPE_ENI_TRUSTED_VNI_ENTRY:
            {
                std::vector<sai_eni_trusted_vni_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_eni_trusted_vni_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].eni_id = translate_local_to_redis(entries[it].eni_id);
                }

                CALL_BULK_REMOVE_API_WITH_TIMER("eni_trusted_vni_entry");

            }
            break;

            default:
                SWSS_LOG_THROW("api %s is not supported in bulk", sai_serialize_common_api(api).c_str());

        }
    }
    else if (api == SAI_COMMON_API_BULK_SET)
    {
        std::vector<sai_attribute_t> attr_lists;

        for (size_t it = 0; it < object_count; it++)
        {
            attr_lists.push_back(attributes[it]->get_attr_list()[0]);
        }
        switch ((int)object_type)
        {
            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            {

                std::vector<sai_route_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_route_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].vr_id = translate_local_to_redis(entries[it].vr_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("route_entry");

            }
            break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            {
                std::vector<sai_neighbor_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_neighbor_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].rif_id = translate_local_to_redis(entries[it].rif_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("neighbor_entry");

            }
            break;

            case SAI_OBJECT_TYPE_FDB_ENTRY:
            {

                std::vector<sai_fdb_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_fdb_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].bv_id = translate_local_to_redis(entries[it].bv_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("fdb_entry");

            }
            break;

            case SAI_OBJECT_TYPE_NAT_ENTRY:
            {

                std::vector<sai_nat_entry_t> entries(object_count);

                for (size_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_nat_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].vr_id = translate_local_to_redis(entries[it].vr_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("nat_entry");

            }
            break;

            case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
            {
                std::vector<sai_direction_lookup_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_direction_lookup_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("direction_lookup_entry");

            }
            break;

            case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
            {
                std::vector<sai_eni_ether_address_map_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_eni_ether_address_map_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("eni_ether_address_map_entry");

            }
            break;

            case SAI_OBJECT_TYPE_VIP_ENTRY:
            {
                std::vector<sai_vip_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_vip_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("vip_entry");

            }
            break;

            case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
            {
                std::vector<sai_inbound_routing_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_inbound_routing_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].eni_id = translate_local_to_redis(entries[it].eni_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("inbound_routing_entry");

            }
            break;

            case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
            {
                std::vector<sai_pa_validation_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_pa_validation_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].vnet_id = translate_local_to_redis(entries[it].vnet_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("pa_validation_entry");

            }
            break;

            case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
            {
                std::vector<sai_outbound_routing_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_outbound_routing_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].outbound_routing_group_id = translate_local_to_redis(entries[it].outbound_routing_group_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("outbound_routing_entry");

            }
            break;

            case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
            {
                std::vector<sai_outbound_ca_to_pa_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_outbound_ca_to_pa_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].dst_vnet_id = translate_local_to_redis(entries[it].dst_vnet_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("outbound_ca_to_pa_entry");

            }
            break;

            case SAI_OBJECT_TYPE_OUTBOUND_PORT_MAP_PORT_RANGE_ENTRY:
            {
                std::vector<sai_outbound_port_map_port_range_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_outbound_port_map_port_range_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].outbound_port_map_id = translate_local_to_redis(entries[it].outbound_port_map_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("outbound_port_map_port_range_entry");

            }
            break;

            case SAI_OBJECT_TYPE_GLOBAL_TRUSTED_VNI_ENTRY:
            {
                std::vector<sai_global_trusted_vni_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_global_trusted_vni_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("global_trusted_vni_entry");

            }
            break;

            case SAI_OBJECT_TYPE_ENI_TRUSTED_VNI_ENTRY:
            {
                std::vector<sai_eni_trusted_vni_entry_t> entries(object_count);

                for (uint32_t it = 0; it < object_count; it++)
                {
                    sai_deserialize_eni_trusted_vni_entry(object_ids[it], entries[it]);

                    entries[it].switch_id = translate_local_to_redis(entries[it].switch_id);
                    entries[it].eni_id = translate_local_to_redis(entries[it].eni_id);
                }

                CALL_BULK_SET_API_WITH_TIMER("eni_trusted_vni_entry");

            }
            break;

            default:
                SWSS_LOG_THROW("api %s is not supported in bulk", sai_serialize_common_api(api).c_str());
        }
    }
    else
    {
        SWSS_LOG_THROW("api %s is not supported in bulk", sai_serialize_common_api(api).c_str());
    }

    return status;
}

sai_status_t SaiPlayer::handle_bulk_object(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &object_ids,
        _In_ sai_common_api_t api,
        _In_ const std::vector<std::shared_ptr<SaiAttributeList>> &attributes,
        _Out_ std::vector<sai_status_t> &statuses)
{
    SWSS_LOG_ENTER();

    if (!object_ids.size())
    {
        SWSS_LOG_ERROR("Object ids is empty in handle bulk generic");
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status;

    uint32_t object_count = (uint32_t) object_ids.size();

    std::vector<sai_object_id_t> local_ids(object_count);

    sai_bulk_op_error_mode_t mode = SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR; // TODO we need to get that from recording

    SWSS_LOG_DEBUG("generic %s for %s:%s",
            sai_serialize_common_api(api).c_str(),
            sai_serialize_object_type(object_type).c_str(),
            object_ids[0].c_str());

    for (uint32_t it = 0; it < object_count; it++)
    {
        sai_deserialize_object_id(object_ids[it], local_ids[it]);
    }

    switch (api)
    {
        case SAI_COMMON_API_BULK_CREATE:

        {
            sai_object_id_t switch_id = m_sai->switchIdQuery(local_ids[0]);
            std::vector<sai_object_id_t> ids(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                if (m_sai->switchIdQuery(local_ids[it]) != switch_id ||
                    switch_id == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_THROW("invalid switch_id translated from VID %s",
                            sai_serialize_object_id(local_ids[it]).c_str());
                }
            }

            std::vector<uint32_t> attr_counts(object_count);

            std::vector<const sai_attribute_t*> attr_lists(object_count);

            for (uint32_t idx = 0; idx < object_count; idx++)
            {
                attr_counts[idx] = attributes[idx]->get_attr_count();
                attr_lists[idx] = attributes[idx]->get_attr_list();
            }

            switch_id = translate_local_to_redis(switch_id);

            status = m_sai->bulkCreate(object_type,
                                            switch_id,
                                            object_count,
                                            attr_counts.data(),
                                            attr_lists.data(),
                                            mode,
                                            ids.data(),
                                            statuses.data());

            if (status == SAI_STATUS_SUCCESS)
            {
                for (uint32_t it = 0; it < object_count; it++)
                {
                    match_redis_with_rec(ids[it], local_ids[it]);

                    SWSS_LOG_INFO("saved VID %s to RID %s",
                            sai_serialize_object_id(local_ids[it]).c_str(),
                            sai_serialize_object_id(ids[it]).c_str());
                }
            }

            return status;
        }
        break;

        case SAI_COMMON_API_BULK_REMOVE:

        {
            std::vector<sai_object_id_t> ids(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                ids[it] = translate_local_to_redis(local_ids[it]);
            }

            status = m_sai->bulkRemove(object_type, object_count, ids.data(), mode, statuses.data());
        }
        break;

        case SAI_COMMON_API_BULK_SET:

        {
            std::vector<sai_object_id_t> ids(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                ids[it] = translate_local_to_redis(local_ids[it]);
            }

            std::vector<sai_attribute_t> attr_list;

            // route can have multiple attributes, so we need to handle them all
            for (const auto &alist: attributes)
            {
                attr_list.push_back(alist->get_attr_list()[0]);
            }

            status = m_sai->bulkSet(object_type, object_count, ids.data(), attr_list.data(), mode, statuses.data());
        }
        break;

        case SAI_COMMON_API_BULK_GET:

        {
            std::vector<sai_object_id_t> ids(object_count);

            for (uint32_t it = 0; it < object_count; it++)
            {
                ids[it] = translate_local_to_redis(local_ids[it]);
            }

            std::vector<uint32_t> attr_counts(object_count);

            std::vector<sai_attribute_t*> attr_lists(object_count);

            for (uint32_t idx = 0; idx < object_count; idx++)
            {
                attr_counts[idx] = attributes[idx]->get_attr_count();
                attr_lists[idx] = attributes[idx]->get_attr_list();
            }

            status = m_sai->bulkGet(object_type,
                                            object_count,
                                            ids.data(),
                                            attr_counts.data(),
                                            attr_lists.data(),
                                            mode,
                                            statuses.data());

            return status;
        }
        break;


        default:
            SWSS_LOG_THROW("generic other apis not implemented");
    }

    return status;
}

void SaiPlayer::processBulk(
        _In_ sai_common_api_t api,
        _In_ const std::string &line)
{
    SWSS_LOG_ENTER();

    if (!line.size())
    {
        return;
    }

    if (api != SAI_COMMON_API_BULK_SET &&
            api != SAI_COMMON_API_BULK_CREATE &&
            api != SAI_COMMON_API_BULK_GET &&
            api != SAI_COMMON_API_BULK_REMOVE)
    {
        SWSS_LOG_THROW("bulk common api %d is not supported yet, FIXME", api);
    }

    /*
     * Here we know we have bulk SET api
     */

    // timestamp|action|objecttype||objectid|attrid=value|...||objectid||objectid|attrid=value|...||...
    auto fields = tokenize(line, "||");

    auto first = fields.at(0); // timestamp|action|objecttype

    std::string str_object_type = swss::tokenize(first, '|').at(2);

    sai_object_type_t object_type = deserialize_object_type(str_object_type);

    std::vector<std::string> object_ids;

    std::vector<std::shared_ptr<SaiAttributeList>> attributes;

    std::vector<sai_status_t> statuses(fields.size());

    // TODO currently we expect bulk API except BULK_GET will always succeed in sync mode
    // we will need to update that, needs to be obtained from recording file
    std::vector<sai_status_t> expectedStatuses(fields.size(), SAI_STATUS_SUCCESS);

    for (size_t idx = 1; idx < fields.size(); ++idx)
    {
        // object_id|attr=value|...
        const std::string &joined = fields[idx];

        auto split = swss::tokenize(joined, '|');

        std::string str_object_id = split.front();

        object_ids.push_back(str_object_id);

        std::vector<swss::FieldValueTuple> entries; // attributes per object id

        // skip front object_id and back status

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

        sai_attribute_t *attr_list = list->get_attr_list();

        uint32_t attr_count = list->get_attr_count();

        if (api != SAI_COMMON_API_BULK_GET)
        {
            translate_local_to_redis(object_type, attr_count, attr_list);
        }

        attributes.push_back(list);
    }

    sai_status_t status = SAI_STATUS_SUCCESS;

    auto info = sai_metadata_get_object_type_info(object_type);

    switch ((int)object_type)
    {
        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
        case SAI_OBJECT_TYPE_FDB_ENTRY:
        case SAI_OBJECT_TYPE_NAT_ENTRY:
        case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
        case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
        case SAI_OBJECT_TYPE_VIP_ENTRY:
        case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
        case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
        case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
        case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
        case SAI_OBJECT_TYPE_OUTBOUND_PORT_MAP_PORT_RANGE_ENTRY:
        case SAI_OBJECT_TYPE_GLOBAL_TRUSTED_VNI_ENTRY:
        case SAI_OBJECT_TYPE_ENI_TRUSTED_VNI_ENTRY:
            status = handle_bulk_entry(object_ids, object_type, api, attributes, statuses);
            break;

        default:

            if (info->isnonobjectid)
            {
                SWSS_LOG_THROW("object %s is non object id, but not handled, FIXME",
                        sai_serialize_object_type(object_type).c_str());
            }

            status = handle_bulk_object(object_type, object_ids, api, attributes, statuses);
            break;
    }

    if (api == SAI_COMMON_API_BULK_GET)
    {
        std::string response;

        do
        {
            // this line may be notification, we need to skip
            std::getline(m_infile, response);
        }
        while (response[response.find_first_of("|") + 1] == 'n');

        const auto tokens = tokenize(response, "||");
        const auto opAndStatus = tokenize(tokens.at(0), "|");

        sai_status_t expectedStatus;
        sai_deserialize_status(opAndStatus.at(2), expectedStatus);

        if (status != expectedStatus)
        {
            SWSS_LOG_WARN("status is: %s but expected: %s",
                    sai_serialize_status(status).c_str(),
                    sai_serialize_status(expectedStatus).c_str());
            return;
        }

        auto valuesIter = tokens.begin() + 1; // Skip operation and status
        for (size_t idx = 0; idx < object_ids.size(); idx++, valuesIter++)
        {
            auto attrValues = tokenize(*valuesIter, "|");

            sai_status_t expectedObjectStatus;
            sai_deserialize_status(attrValues.at(0), expectedObjectStatus);

            if (statuses[idx] != expectedObjectStatus)
            {
                SWSS_LOG_WARN("object status is: %s but expected: %s",
                        sai_serialize_status(statuses[idx]).c_str(),
                        sai_serialize_status(expectedObjectStatus).c_str());
                continue;
            }

            std::vector<swss::FieldValueTuple> values;

            for (size_t attrIdx = 1; attrIdx < attrValues.size(); attrIdx++) // skip status
            {
                auto& attrStr = attrValues[attrIdx];
                auto start = attrStr.find_first_of("=");

                auto field = attrStr.substr(0, start);
                auto value = attrStr.substr(start + 1);

                swss::FieldValueTuple entry(field, value);

                values.push_back(entry);
            }

            SaiAttributeList list(object_type, values, false);

            sai_attribute_t *attr_list = list.get_attr_list();
            uint32_t attr_count = list.get_attr_count();

            match_list_lengths(object_type, attributes[idx]->get_attr_count(),
                attributes[idx]->get_attr_list(), attr_count, attr_list);

            SWSS_LOG_DEBUG("list match");

            match_redis_with_rec(object_type, attributes[idx]->get_attr_count(),
                attributes[idx]->get_attr_list(), attr_count, attr_list);

            // NOTE: Primitive values are not matched (recording vs switch/vs), we can add that check
        }
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("handle bulk executed with failure, status = %s", sai_serialize_status(status).c_str());
    }

    // even if API will fail, we will need to compare all statuses for each entry

    for (size_t i = 0; i < statuses.size(); ++i)
    {
        if (statuses[i] != expectedStatuses[i])
        {
            /*
             * If expected statuses are different than received, throw
             * exception since data don't match.
             */
            SWSS_LOG_THROW("expected status is %s but returned is %s on %s",
                    sai_serialize_status(expectedStatuses[i]).c_str(),
                    sai_serialize_status(statuses[i]).c_str(),
                    object_ids[i].c_str());
        }
    }
}

int SaiPlayer::replay()
{
    //swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    if (m_commandLineOptions->m_files.size() == 0)
    {
        fprintf(stderr, "ERR: need to specify filename\n");

        return -1;
    }

    auto filename = m_commandLineOptions->m_files.at(0);

    SWSS_LOG_NOTICE("using file: %s", filename.c_str());

    m_infile = std::ifstream{filename};

    if (!m_infile.is_open())
    {
        SWSS_LOG_ERROR("failed to open file %s", filename.c_str());
        return -1;
    }

    std::string line;

    while (std::getline(m_infile, line))
    {
        // std::cout << "processing " << line << std::endl;

        sai_common_api_t api = SAI_COMMON_API_CREATE;

        auto p = line.find_first_of("|");

        char op = line[p+1];

        switch (op)
        {
            case 'a':
                {
                    std::string response;

                    do
                    {
                        // this line may be notification, we need to skip
                        if (!std::getline(m_infile, response))
                        {
                            SWSS_LOG_THROW("failed to read next file from file, previous: %s", line.c_str());
                        }
                    }
                    while (response[response.find_first_of("|") + 1] == 'n');

                    performNotifySyncd(line, response);
                }
                continue;

            case 'f':
                {
                    std::string response;

                    do
                    {
                        // this line may be notification, we need to skip
                        if (!std::getline(m_infile, response))
                        {
                            SWSS_LOG_THROW("failed to read next file from file, previous: %s", line.c_str());
                        }
                    }
                    while (response[response.find_first_of("|") + 1] == 'n');

                    performFdbFlush(line, response);
                }
                continue;

            case '@':
                performSleep(line);
                continue;
            case 'c':
                api = SAI_COMMON_API_CREATE;
                break;
            case 'r':
                api = SAI_COMMON_API_REMOVE;
                break;
            case 's':
                api = SAI_COMMON_API_SET;
                break;
            case 'B':
                processBulk(SAI_COMMON_API_BULK_GET, line);
                continue;
            case 'S':
                processBulk(SAI_COMMON_API_BULK_SET, line);
                continue;
            case 'C':
                processBulk(SAI_COMMON_API_BULK_CREATE, line);
                continue;
            case 'R':
                processBulk(SAI_COMMON_API_BULK_REMOVE, line);
                continue;
            case 'g':
                api = SAI_COMMON_API_GET;
                break;
            case 'q':
                // TODO: implement SAI player support for query commands
                continue;
            case 'p':
                // TODO: implement SAI player support for counter polling commands
                continue;
            case 'Q':
                continue; // skip over query responses
            case '#':
            case 'n':
                SWSS_LOG_INFO("skipping op %c line %s", op, line.c_str());
                continue; // skip comment and notification

            default:
                SWSS_LOG_THROW("unknown op %c on line %s", op, line.c_str());
        }

        // timestamp|action|objecttype:objectid|attrid=value,...
        auto fields = swss::tokenize(line, '|');

        // objecttype:objectid (object id may contain ':')
        auto start = fields[2].find_first_of(":");

        auto str_object_type = fields[2].substr(0, start);
        auto str_object_id  = fields[2].substr(start + 1);

        sai_object_type_t object_type = deserialize_object_type(str_object_type);

        auto values = get_values(fields);

        SaiAttributeList list(object_type, values, false);

        sai_attribute_t *attr_list = list.get_attr_list();

        uint32_t attr_count = list.get_attr_count();

        SWSS_LOG_DEBUG("attr count: %u", list.get_attr_count());

        if (api != SAI_COMMON_API_GET)
        {
            translate_local_to_redis(object_type, attr_count, attr_list);
        }

        sai_status_t status;

        auto info = sai_metadata_get_object_type_info(object_type);

        switch ((int)object_type)
        {
            case SAI_OBJECT_TYPE_FDB_ENTRY:
                status = handle_fdb(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                status = handle_neighbor(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                status = handle_route(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_INSEG_ENTRY:
                status = handle_inseg(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
                status = handle_dash_direction_lookup(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
                status = handle_dash_eni_ether_address_map(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_VIP_ENTRY:
                status = handle_dash_vip(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
                status = handle_dash_inbound_routing(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
                status = handle_dash_pa_validation(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
                status = handle_dash_outbound_routing(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
                status = handle_dash_outbound_ca_to_pa(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_OUTBOUND_PORT_MAP_PORT_RANGE_ENTRY:
                status = handle_dash_outbound_port_map_port_range(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_GLOBAL_TRUSTED_VNI_ENTRY:
                status = handle_dash_global_trusted_vni(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_ENI_TRUSTED_VNI_ENTRY:
                status = handle_dash_eni_trusted_vni(str_object_id, api, attr_count, attr_list);
                break;

            default:

                if (info->isnonobjectid)
                {
                    SWSS_LOG_THROW("object %s:%s is non object id, but not handled, FIXME",
                            sai_serialize_object_type(object_type).c_str(),
                            str_object_id.c_str());
                }

                status = handle_generic(object_type, str_object_id, api, attr_count, attr_list);
                break;
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            if (api == SAI_COMMON_API_GET)
            {
                // GET status is checked in handle response
            }
            else
                SWSS_LOG_THROW("failed to execute api: %c: %s", op, sai_serialize_status(status).c_str());
        }

        if (api == SAI_COMMON_API_GET)
        {
            std::string response;

            do
            {
                // this line may be notification, we need to skip
                std::getline(m_infile, response);
            }
            while (response[response.find_first_of("|") + 1] == 'n');

            try
            {
                handle_get_response(object_type, attr_count, attr_list, response, status);
            }
            catch (const std::exception &e)
            {
                SWSS_LOG_NOTICE("line: %s", line.c_str());
                SWSS_LOG_NOTICE("resp (expected): %s", response.c_str());
                SWSS_LOG_NOTICE("got: %s", sai_serialize_status(status).c_str());

                if (api == SAI_COMMON_API_GET && (status == SAI_STATUS_SUCCESS || status == SAI_STATUS_BUFFER_OVERFLOW))
                {
                    // log each get parameter
                    for (uint32_t i = 0; i < attr_count; ++i)
                    {
                        auto meta = sai_metadata_get_attr_metadata(object_type, attr_list[i].id);

                        auto val = sai_serialize_attr_value(*meta, attr_list[i]);

                        SWSS_LOG_NOTICE(" - %s:%s", meta->attridname, val.c_str());
                    }
                }

                exit(EXIT_FAILURE);
            }

            if (api == SAI_COMMON_API_GET && (status == SAI_STATUS_SUCCESS || status == SAI_STATUS_BUFFER_OVERFLOW))
            {
                // log each get parameter
                for (uint32_t i = 0; i < attr_count; ++i)
                {
                    auto meta = sai_metadata_get_attr_metadata(object_type, attr_list[i].id);

                    auto val = sai_serialize_attr_value(*meta, attr_list[i]);

                    SWSS_LOG_NOTICE(" - %s:%s", meta->attridname, val.c_str());
                }
            }
        }
    }

    m_infile.close();

    SWSS_LOG_NOTICE("finished replaying %s with SUCCESS", filename.c_str());

    if (m_commandLineOptions->m_sleep)
    {
        fprintf(stderr, "Reply SUCCESS, sleeping, watching for notifications\n");

        sleep(-1);
    }

    return 0;
}

const char* SaiPlayer::profileGetValue(
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

int SaiPlayer::profileGetNextValue(
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

int SaiPlayer::run()
{
    SWSS_LOG_ENTER();

    if (m_commandLineOptions->m_enableDebug)
    {
        swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);
    }

    m_test_services = m_smt.getServiceMethodTable();

    EXIT_ON_ERROR(m_sai->apiInitialize(0, &m_test_services));

    sai_attribute_t attr;

    /*
     * Notice that we use null object id as switch id, which is fine since
     * those attributes don't need switch.
     */

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    if (m_commandLineOptions->m_inspectAsic)
    {
        attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC;

        EXIT_ON_ERROR(m_sai->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));
    }

    if (m_commandLineOptions->m_syncMode)
    {
        SWSS_LOG_WARN("sync mode is depreacated, please use communication mode");

        attr.id = SAI_REDIS_SWITCH_ATTR_SYNC_MODE;
        attr.value.booldata = true;

        EXIT_ON_ERROR(m_sai->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));

        m_commandLineOptions->m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;
    }

    attr.id = SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE;
    attr.value.s32 = m_commandLineOptions->m_redisCommunicationMode;

    EXIT_ON_ERROR(m_sai->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));

    attr.id = SAI_REDIS_SWITCH_ATTR_RECORD;
    attr.value.booldata = m_commandLineOptions->m_enableRecording;

    EXIT_ON_ERROR(m_sai->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));

    int exitcode = 0;

    if (m_commandLineOptions->m_files.size() > 0)
    {
        attr.id = SAI_REDIS_SWITCH_ATTR_USE_TEMP_VIEW;
        attr.value.booldata = m_commandLineOptions->m_useTempView;

        EXIT_ON_ERROR(m_sai->set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));

        exitcode = replay();
    }

    m_sai->apiUninitialize();

    return exitcode;
}
