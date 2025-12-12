#include <inttypes.h>
#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <mutex>

#include "FlexCounter.h"
#include "VidManager.h"

#include "meta/sai_serialize.h"

#include "swss/redisapi.h"
#include "swss/tokenize.h"

using namespace syncd;
using namespace std;

#define MUTEX std::unique_lock<std::mutex> _lock(m_mtx);
#define MUTEX_UNLOCK _lock.unlock();

static const std::string COUNTER_TYPE_PORT = "Port Counter";
static const std::string COUNTER_TYPE_PORT_DEBUG = "Port Debug Counter";
static const std::string COUNTER_TYPE_QUEUE = "Queue Counter";
static const std::string COUNTER_TYPE_PG = "Priority Group Counter";
static const std::string COUNTER_TYPE_RIF = "Rif Counter";
static const std::string COUNTER_TYPE_SWITCH_DEBUG = "Switch Debug Counter";
static const std::string COUNTER_TYPE_MACSEC_FLOW = "MACSEC Flow Counter";
static const std::string COUNTER_TYPE_MACSEC_SA = "MACSEC SA Counter";
static const std::string COUNTER_TYPE_FLOW = "Flow Counter";
static const std::string COUNTER_TYPE_TUNNEL = "Tunnel Counter";
static const std::string COUNTER_TYPE_BUFFER_POOL = "Buffer Pool Counter";
static const std::string COUNTER_TYPE_ENI = "DASH ENI Counter";
static const std::string COUNTER_TYPE_METER_BUCKET = "DASH Meter Bucket Counter";
static const std::string COUNTER_TYPE_POLICER = "Policer Counter";
static const std::string COUNTER_TYPE_SRV6 = "SRv6 Counter";
static const std::string COUNTER_TYPE_SWITCH = "Switch Counter";
static const std::string ATTR_TYPE_QUEUE = "Queue Attribute";
static const std::string ATTR_TYPE_PG = "Priority Group Attribute";
static const std::string ATTR_TYPE_MACSEC_SA = "MACSEC SA Attribute";
static const std::string ATTR_TYPE_ACL_COUNTER = "ACL Counter Attribute";
static const std::string COUNTER_TYPE_WRED_ECN_QUEUE = "WRED Queue Counter";
static const std::string COUNTER_TYPE_WRED_ECN_PORT = "WRED Port Counter";

static const std::unordered_map<std::string, bool> statusMap =
{
    { "enable",  true  },
    { "disable", false }
};

static const std::unordered_map<std::string, sai_stats_mode_t> statsModeMap =
{
    { STATS_MODE_READ,           SAI_STATS_MODE_READ           },
    { STATS_MODE_READ_AND_CLEAR, SAI_STATS_MODE_READ_AND_CLEAR }
};

const std::map<std::string, std::string> FlexCounter::m_plugIn2CounterType = {
    {QUEUE_PLUGIN_FIELD, COUNTER_TYPE_QUEUE},
    {PG_PLUGIN_FIELD, COUNTER_TYPE_PG},
    {PORT_PLUGIN_FIELD, COUNTER_TYPE_PORT},
    {RIF_PLUGIN_FIELD, COUNTER_TYPE_RIF},
    {BUFFER_POOL_PLUGIN_FIELD, COUNTER_TYPE_BUFFER_POOL},
    {TUNNEL_PLUGIN_FIELD, COUNTER_TYPE_TUNNEL},
    {FLOW_COUNTER_PLUGIN_FIELD, COUNTER_TYPE_FLOW},
    {WRED_QUEUE_PLUGIN_FIELD, COUNTER_TYPE_WRED_ECN_QUEUE},
    {WRED_PORT_PLUGIN_FIELD, COUNTER_TYPE_WRED_ECN_PORT}};

const std::map<std::tuple<sai_object_type_t, std::string>, std::string> FlexCounter::m_objectTypeField2CounterType = {
    {{SAI_OBJECT_TYPE_PORT, PORT_COUNTER_ID_LIST}, COUNTER_TYPE_PORT},
    {{SAI_OBJECT_TYPE_PORT, PORT_DEBUG_COUNTER_ID_LIST}, COUNTER_TYPE_PORT_DEBUG},
    {{SAI_OBJECT_TYPE_QUEUE, QUEUE_COUNTER_ID_LIST}, COUNTER_TYPE_QUEUE},
    {{SAI_OBJECT_TYPE_QUEUE, QUEUE_ATTR_ID_LIST}, ATTR_TYPE_QUEUE},
    {{SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, PG_COUNTER_ID_LIST}, COUNTER_TYPE_PG},
    {{SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, PG_ATTR_ID_LIST}, ATTR_TYPE_PG},
    {{SAI_OBJECT_TYPE_ROUTER_INTERFACE, RIF_COUNTER_ID_LIST}, COUNTER_TYPE_RIF},
    {{SAI_OBJECT_TYPE_SWITCH, SWITCH_DEBUG_COUNTER_ID_LIST}, COUNTER_TYPE_SWITCH_DEBUG},
    {{SAI_OBJECT_TYPE_MACSEC_FLOW, MACSEC_FLOW_COUNTER_ID_LIST}, COUNTER_TYPE_MACSEC_FLOW},
    {{SAI_OBJECT_TYPE_MACSEC_SA, MACSEC_SA_COUNTER_ID_LIST}, COUNTER_TYPE_MACSEC_SA},
    {{SAI_OBJECT_TYPE_MACSEC_SA, MACSEC_SA_ATTR_ID_LIST}, ATTR_TYPE_MACSEC_SA},
    {{SAI_OBJECT_TYPE_ACL_COUNTER, ACL_COUNTER_ATTR_ID_LIST}, ATTR_TYPE_ACL_COUNTER},
    {{SAI_OBJECT_TYPE_COUNTER, FLOW_COUNTER_ID_LIST}, COUNTER_TYPE_FLOW},
    {{SAI_OBJECT_TYPE_POLICER, POLICER_COUNTER_ID_LIST}, COUNTER_TYPE_POLICER},
    {{SAI_OBJECT_TYPE_TUNNEL, TUNNEL_COUNTER_ID_LIST}, COUNTER_TYPE_TUNNEL},
    {{(sai_object_type_t)SAI_OBJECT_TYPE_ENI, ENI_COUNTER_ID_LIST}, COUNTER_TYPE_ENI},
    {{(sai_object_type_t)SAI_OBJECT_TYPE_ENI, DASH_METER_COUNTER_ID_LIST}, COUNTER_TYPE_METER_BUCKET},
    {{SAI_OBJECT_TYPE_COUNTER, SRV6_COUNTER_ID_LIST}, COUNTER_TYPE_SRV6},
    {{SAI_OBJECT_TYPE_SWITCH, SWITCH_COUNTER_ID_LIST}, COUNTER_TYPE_SWITCH},
};

BaseCounterContext::BaseCounterContext(const std::string &name, const std::string &instance):
m_name(name),
m_instanceId(instance)
{
    SWSS_LOG_ENTER();
}

void BaseCounterContext::addPlugins(
    _In_ const std::vector<std::string>& shaStrings)
{
    SWSS_LOG_ENTER();

    for (const auto &sha : shaStrings)
    {
        auto ret = m_plugins.insert(sha);
        if (ret.second)
        {
            SWSS_LOG_NOTICE("%s counters plugin %s registered", m_name.c_str(), sha.c_str());
        }
        else
        {
            SWSS_LOG_ERROR("Plugin %s already registered", sha.c_str());
        }
    }
}

void BaseCounterContext::setNoDoubleCheckBulkCapability(
    _In_ bool noDoubleCheckBulkCapability)
{
    SWSS_LOG_ENTER();
    no_double_check_bulk_capability = noDoubleCheckBulkCapability;
}

void BaseCounterContext::setBulkChunkSize(
    _In_ uint32_t bulkChunkSize)
{
    SWSS_LOG_ENTER();
    default_bulk_chunk_size = bulkChunkSize;
}

void BaseCounterContext::setBulkChunkSizePerPrefix(
    _In_ const std::string& bulkChunkSizePerPrefix)
{
    SWSS_LOG_ENTER();
    m_bulkChunkSizePerPrefix = bulkChunkSizePerPrefix;
}

template <typename StatType,
          typename Enable = void>
struct CounterIds
{
    CounterIds(
            _In_ sai_object_id_t id,
            _In_ const std::vector<StatType> &ids
    ): rid(id), counter_ids(ids) {}
    void setStatsMode(sai_stats_mode_t statsMode) {}
    sai_stats_mode_t getStatsMode() const
    {
        SWSS_LOG_ENTER();
        SWSS_LOG_THROW("This counter type has no stats mode field");
        // GCC 8.3 requires a return value here
        return SAI_STATS_MODE_READ_AND_CLEAR;
    }
    sai_object_id_t rid;
    std::vector<StatType> counter_ids;
};

// CounterIds structure contains stats mode, now buffer pool is the only one
// has member stats_mode.
template <typename StatType>
struct CounterIds<StatType, typename std::enable_if_t<std::is_same<StatType, sai_buffer_pool_stat_t>::value> >
{
    CounterIds(
            _In_ sai_object_id_t id,
            _In_ const std::vector<StatType> &ids
    ): rid(id), counter_ids(ids)
    {
        SWSS_LOG_ENTER();
    }

    void setStatsMode(sai_stats_mode_t statsMode)
    {
        SWSS_LOG_ENTER();
        stats_mode = statsMode;
    }

    sai_stats_mode_t getStatsMode() const
    {
        SWSS_LOG_ENTER();
        return stats_mode;
    }
    sai_object_id_t rid;
    std::vector<StatType> counter_ids;
    sai_stats_mode_t stats_mode;
};

template <typename T>
struct HasStatsMode
{
    template <typename U>
    static void check(decltype(&U::stats_mode));
    template <typename U>
    static int check(...);

    enum { value = std::is_void<decltype(check<T>(0))>::value };
};

// BulkStatsContext is used to store bulk counter related
// data and avoid construct them each time calling SAI bulk API
template <typename StatType>
struct BulkStatsContext
{
    std::vector<sai_object_id_t> object_vids;
    std::vector<sai_object_key_t> object_keys;
    std::vector<StatType> counter_ids;
    std::vector<sai_status_t> object_statuses;
    std::vector<uint64_t> counters;
    std::string name;
    uint32_t default_bulk_chunk_size;
};

// TODO: use if const expression when cpp17 is supported
template <typename StatType>
std::string serializeStat(
        _In_ const StatType stat)
{
    SWSS_LOG_ENTER();
    SWSS_LOG_THROW("serializeStat for default type parameter is not implemented");
    // GCC 8.3 requires a return value here
    return "";
}

template <>
std::string serializeStat(
        _In_ const sai_port_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_port_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_policer_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_policer_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_queue_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_queue_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_ingress_priority_group_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_ingress_priority_group_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_router_interface_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_router_interface_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_switch_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_switch_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_macsec_flow_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_macsec_flow_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_macsec_sa_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_macsec_sa_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_counter_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_counter_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_tunnel_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_tunnel_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_buffer_pool_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_buffer_pool_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_eni_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_eni_stat(stat);
}

template <>
std::string serializeStat(
        _In_ const sai_meter_bucket_entry_stat_t stat)
{
    SWSS_LOG_ENTER();
    return sai_serialize_meter_bucket_entry_stat(stat);
}

template <typename StatType>
void deserializeStat(
        _In_ const char* name,
        _Out_ StatType *stat)
{
    SWSS_LOG_ENTER();
    SWSS_LOG_THROW("deserializeStat for default type parameter is not implemented");
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_port_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_port_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_policer_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_policer_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_queue_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_queue_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_ingress_priority_group_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_ingress_priority_group_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_router_interface_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_router_interface_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_switch_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_switch_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_macsec_flow_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_macsec_flow_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_macsec_sa_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_macsec_sa_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_counter_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_counter_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_tunnel_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_tunnel_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_buffer_pool_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_buffer_pool_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_eni_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_eni_stat(name, stat);
}

template <>
void deserializeStat(
        _In_ const char* name,
        _Out_ sai_meter_bucket_entry_stat_t *stat)
{
    SWSS_LOG_ENTER();
    sai_deserialize_meter_bucket_entry_stat(name, stat);
}

template <typename AttrType>
void deserializeAttr(
        _In_ const std::string& name,
        _Out_ AttrType &attr)
{
    SWSS_LOG_ENTER();
    SWSS_LOG_THROW("deserializeAttr for default type parameter is not implemented");
}

template <>
void deserializeAttr(
        _In_ const std::string& name,
        _Out_ sai_queue_attr_t &attr)
{
    SWSS_LOG_ENTER();
    sai_deserialize_queue_attr(name, attr);
}

template <>
void deserializeAttr(
        _In_ const std::string& name,
        _Out_ sai_ingress_priority_group_attr_t &attr)
{
    SWSS_LOG_ENTER();
    sai_deserialize_ingress_priority_group_attr(name, attr);
}

template <>
void deserializeAttr(
        _In_ const std::string& name,
        _Out_ sai_macsec_sa_attr_t &attr)
{
    SWSS_LOG_ENTER();
    sai_deserialize_macsec_sa_attr(name, attr);
}

template <>
void deserializeAttr(
        _In_ const std::string& name,
        _Out_ sai_acl_counter_attr_t &attr)
{
    SWSS_LOG_ENTER();
    sai_deserialize_acl_counter_attr(name, attr);
}

template <typename StatType>
class CounterContext : public BaseCounterContext
{
    std::map<std::string, uint32_t> m_counterChunkSizeMapFromPrefix;

protected:
    sai_object_id_t m_switchId = SAI_NULL_OBJECT_ID;

public:
    typedef CounterIds<StatType> CounterIdsType;
    typedef BulkStatsContext<StatType> BulkContextType;

    CounterContext(
            _In_ const std::string &name,
            _In_ const std::string &instance,
            _In_ sai_object_type_t object_type,
            _In_ sairedis::SaiInterface *vendor_sai,
            _In_ sai_stats_mode_t &stats_mode):
    BaseCounterContext(name, instance), m_objectType(object_type), m_vendorSai(vendor_sai), m_groupStatsMode(stats_mode)
    {
        SWSS_LOG_ENTER();
    }

    // For those object type who support per object stats mode, e.g. buffer pool.
    virtual void addObject(
            _In_ sai_object_id_t vid,
            _In_ sai_object_id_t rid,
            _In_ const std::vector<std::string> &idStrings,
            _In_ const std::string &per_object_stats_mode) override
    {
        SWSS_LOG_ENTER();
        sai_stats_mode_t instance_stats_mode = SAI_STATS_MODE_READ_AND_CLEAR;
        sai_stats_mode_t effective_stats_mode;
        // TODO: use if const expression when c++17 is supported
        if (HasStatsMode<CounterIdsType>::value)
        {
            if (per_object_stats_mode == STATS_MODE_READ_AND_CLEAR)
            {
                instance_stats_mode = SAI_STATS_MODE_READ_AND_CLEAR;
            }
            else if (per_object_stats_mode == STATS_MODE_READ)
            {
                instance_stats_mode = SAI_STATS_MODE_READ;
            }
            else
            {
                SWSS_LOG_WARN("Stats mode %s not supported for flex counter. Using STATS_MODE_READ_AND_CLEAR", per_object_stats_mode.c_str());
            }

            effective_stats_mode = (m_groupStatsMode == SAI_STATS_MODE_READ_AND_CLEAR ||
                                    instance_stats_mode == SAI_STATS_MODE_READ_AND_CLEAR) ? SAI_STATS_MODE_READ_AND_CLEAR : SAI_STATS_MODE_READ;
        }
        else
        {
            effective_stats_mode = m_groupStatsMode;
        }

        std::vector<StatType> counter_ids;
        for (const auto &str : idStrings)
        {
            StatType stat;
            deserializeStat(str.c_str(), &stat);
            counter_ids.push_back(stat);
        }

        updateSupportedCounters(rid, counter_ids, effective_stats_mode);

        std::vector<StatType> supportedIds;
        for (auto &counter : counter_ids)
        {
            if (isCounterSupported(counter))
            {
                supportedIds.push_back(counter);
            }
        }

        if (supportedIds.empty())
        {
            SWSS_LOG_NOTICE("%s %s does not has supported counters", m_name.c_str(), sai_serialize_object_id(rid).c_str());
            return;
        }

        if (double_confirm_supported_counters)
        {
            std::vector<uint64_t> stats(supportedIds.size());
            if (!collectData(rid, supportedIds, effective_stats_mode, false, stats))
            {
                SWSS_LOG_ERROR("%s RID %s can't provide the statistic",  m_name.c_str(), sai_serialize_object_id(rid).c_str());
                return;
            }
        }

         // Perform a remove and re-add to simplify the logic here
        removeObject(vid, false);

        bool supportBulk;
        // TODO: use if const expression when cpp17 is supported
        if (HasStatsMode<CounterIdsType>::value)
        {
            supportBulk = false;
        }
        else
        {
            supportBulk = no_double_check_bulk_capability || checkBulkCapability(vid, rid, supportedIds);
        }

        if (!supportBulk)
        {
            auto counter_data = std::make_shared<CounterIds<StatType>>(rid, supportedIds);
            // TODO: use if const expression when cpp17 is supported
            if (HasStatsMode<CounterIdsType>::value)
            {
                counter_data->setStatsMode(instance_stats_mode);
            }
            m_objectIdsMap.emplace(vid, counter_data);
        }
        else if (m_counterChunkSizeMapFromPrefix.empty())
        {
            std::sort(supportedIds.begin(), supportedIds.end());
            auto bulkContext = getBulkStatsContext(supportedIds, "default", default_bulk_chunk_size);
            addBulkStatsContext(vid, rid, supportedIds, *bulkContext.get());
        }
        else
        {
            std::map<std::string, vector<StatType>> counter_prefix_map;
            std::vector<StatType> default_partition;
            createCounterBulkChunkSizePerPrefixPartition(supportedIds, counter_prefix_map, default_partition);

            for (auto &counterPrefix : counter_prefix_map)
            {
                std::sort(counterPrefix.second.begin(), counterPrefix.second.end());
                auto bulkContext = getBulkStatsContext(counterPrefix.second, counterPrefix.first, m_counterChunkSizeMapFromPrefix[counterPrefix.first]);
                addBulkStatsContext(vid, rid, counterPrefix.second, *bulkContext.get());
            }

            std::sort(default_partition.begin(), default_partition.end());
            auto bulkContext = getBulkStatsContext(default_partition, "default", default_bulk_chunk_size);
            addBulkStatsContext(vid, rid, default_partition, *bulkContext.get());
        }
    }

    bool parseBulkChunkSizePerPrefixConfigString(
        _In_ const std::string& prefixConfigString)
    {
        SWSS_LOG_ENTER();

        m_counterChunkSizeMapFromPrefix.clear();

        if (!prefixConfigString.empty() && prefixConfigString != "NULL")
        {
            try
            {
                auto tokens = swss::tokenize(prefixConfigString, ';');

                for (auto &token: tokens)
                {
                    auto counter_name_bulk_size = swss::tokenize(token, ':');
                    SWSS_LOG_INFO("New partition %s bulk chunk size %s", counter_name_bulk_size[0].c_str(), counter_name_bulk_size[1].c_str());
                    m_counterChunkSizeMapFromPrefix[counter_name_bulk_size[0]] = stoi(counter_name_bulk_size[1]);
                }
            }
            catch (...)
            {
                SWSS_LOG_ERROR("Invalid bulk chunk size per counter ID field %s", prefixConfigString.c_str());
                m_counterChunkSizeMapFromPrefix.clear();
                return false;
            }
        }

        return true;
    }

    void createCounterBulkChunkSizePerPrefixPartition(
        _In_ const std::vector<StatType>& supportedIds,
        _Out_ std::map<std::string, std::vector<StatType>> &counter_prefix_map,
        _Out_ std::vector<StatType> &default_partition,
        _In_ bool log=false)
    {
        SWSS_LOG_ENTER();

        default_partition.clear();
        for (auto &counter : supportedIds)
        {
            std::string counterStr = serializeStat(counter);
            bool found = false;
            for (auto searchRef: m_counterChunkSizeMapFromPrefix)
            {
                if (!searchRef.first.empty() && counterStr.find(searchRef.first) != std::string::npos)
                {
                    found = true;
                    counter_prefix_map[searchRef.first].push_back(counter);
                    if (log)
                    {
                        SWSS_LOG_INFO("Put counter %s to partition %s", counterStr.c_str(), searchRef.first.c_str());
                    }
                    break;
                }
            }
            if (!found)
            {
                default_partition.push_back(counter);

                if (log)
                {
                    SWSS_LOG_INFO("Put counter %s to the default partition", counterStr.c_str());
                }
            }
        }
    }

    void setBulkChunkSize(
        _In_ uint32_t bulkChunkSize) override
    {
        SWSS_LOG_ENTER();
        default_bulk_chunk_size = bulkChunkSize;
        SWSS_LOG_INFO("Bulk chunk size updated to %u", bulkChunkSize);

        for (auto &bulkStatsContext : m_bulkContexts)
        {
            auto const &name = (*bulkStatsContext.second.get()).name;
            if (name == "default")
            {
                SWSS_LOG_INFO("Bulk chunk size of default updated to %u", bulkChunkSize);
                (*bulkStatsContext.second.get()).default_bulk_chunk_size = default_bulk_chunk_size;
                break;
            }
        }
    }

    void setBulkChunkSizePerPrefix(
        _In_ const std::string& bulkChunkSizePerPrefix) override
    {
        SWSS_LOG_ENTER();

        m_bulkChunkSizePerPrefix = bulkChunkSizePerPrefix;

        // No operation if the input string is invalid or no bulk context has been created
        if (!parseBulkChunkSizePerPrefixConfigString(bulkChunkSizePerPrefix) || m_bulkContexts.empty())
        {
            return;
        }

        if (m_bulkContexts.size() == 1)
        {
            // Only one bulk context exists which means
            // it is the first time per counter chunk size is configured and a unified counter ID set is polled for all objects
            auto it = m_bulkContexts.begin();
            std::shared_ptr<BulkContextType> singleBulkContext = it->second;
            const std::vector<StatType> &allCounterIds = singleBulkContext.get()->counter_ids;
            std::map<std::string, vector<StatType>> counterChunkSizePerPrefix;
            std::vector<StatType> defaultPartition;

            if (m_counterChunkSizeMapFromPrefix.empty())
            {
                // There is still no per counter prefix chunk size configured as the chunk size map is still empty.
                singleBulkContext.get()->default_bulk_chunk_size = default_bulk_chunk_size;
            }
            else
            {
                // Split the counter IDs according to the counter ID prefix mapping and store them into m_bulkContexts
                SWSS_LOG_NOTICE("Split counter IDs set by prefix for the first time %s", bulkChunkSizePerPrefix.c_str());
                createCounterBulkChunkSizePerPrefixPartition(allCounterIds, counterChunkSizePerPrefix, defaultPartition, true);

                for (auto &counterPrefix : counterChunkSizePerPrefix)
                {
                    std::sort(counterPrefix.second.begin(), counterPrefix.second.end());
                    auto bulkContext = getBulkStatsContext(counterPrefix.second, counterPrefix.first, m_counterChunkSizeMapFromPrefix[counterPrefix.first]);

                    bulkContext.get()->counter_ids = move(counterPrefix.second);
                    bulkContext.get()->object_statuses.resize(singleBulkContext.get()->object_statuses.size());
                    bulkContext.get()->object_vids = singleBulkContext.get()->object_vids;
                    bulkContext.get()->object_keys = singleBulkContext.get()->object_keys;
                    bulkContext.get()->counters.resize(bulkContext.get()->counter_ids.size() * bulkContext.get()->object_vids.size());

                    SWSS_LOG_INFO("Re-initializing counter partition %s", counterPrefix.first.c_str());
                }

                std::sort(defaultPartition.begin(), defaultPartition.end());
                setBulkStatsContext(defaultPartition, singleBulkContext);
                singleBulkContext.get()->counters.resize(singleBulkContext.get()->counter_ids.size() * singleBulkContext.get()->object_vids.size());
                m_bulkContexts.erase(it);
                SWSS_LOG_INFO("Removed the previous default counter partition");
            }
        }
        else if (m_counterChunkSizeMapFromPrefix.empty())
        {
            // There have been multiple bulk contexts which can result from
            // 1. per counter prefix chunk size configuration
            // 2. different objects support different counter ID set
            // And there is no per counter prefix chunk size configured any more
            // Multiple bulk contexts will be merged into one if they share the same object IDs set, which means case (1).
            std::set<sai_object_id_t> oid_set;
            std::vector<StatType> counter_ids;
            std::shared_ptr<BulkContextType> defaultBulkContext;
            for (auto &context : m_bulkContexts)
            {
                if (oid_set.empty())
                {
                    oid_set.insert(context.second.get()->object_vids.begin(), context.second.get()->object_vids.end());
                }
                else
                {
                    std::set<sai_object_id_t> tmp_oid_set(context.second.get()->object_vids.begin(), context.second.get()->object_vids.end());
                    if (tmp_oid_set != oid_set)
                    {
                        SWSS_LOG_ERROR("Can not merge partition because they contains different objects");
                        return;
                    }
                }
                if (context.second.get()->name == "default")
                {
                    defaultBulkContext = context.second;
                }
                counter_ids.insert(counter_ids.end(), context.second.get()->counter_ids.begin(), context.second.get()->counter_ids.end());
            }

            m_bulkContexts.clear();

            std::sort(counter_ids.begin(), counter_ids.end());
            setBulkStatsContext(counter_ids, defaultBulkContext);
            defaultBulkContext.get()->counters.resize(defaultBulkContext.get()->counter_ids.size() * defaultBulkContext.get()->object_vids.size());
        }
        else
        {
            // Multiple bulk contexts and per counter prefix chunk size
            // Update the chunk size only in this case.
            SWSS_LOG_NOTICE("Update bulk chunk size only %s", bulkChunkSizePerPrefix.c_str());

            auto counterChunkSizeMapFromPrefix = m_counterChunkSizeMapFromPrefix;
            for (auto &bulkStatsContext : m_bulkContexts)
            {
                auto const &name = (*bulkStatsContext.second.get()).name;

                if (name == "default")
                {
                    continue;
                }

                auto const &searchRef = counterChunkSizeMapFromPrefix.find(name);
                if (searchRef != counterChunkSizeMapFromPrefix.end())
                {
                    auto const &chunkSize = searchRef->second;

                    SWSS_LOG_INFO("Reset counter prefix %s chunk size %d", name.c_str(), chunkSize);
                    (*bulkStatsContext.second.get()).default_bulk_chunk_size = chunkSize;
                    counterChunkSizeMapFromPrefix.erase(searchRef);
                }
                else
                {
                    SWSS_LOG_WARN("Update bulk chunk size: bulk chunk size for prefix %s is not provided", name.c_str());
                }
            }

            for (auto &it : counterChunkSizeMapFromPrefix)
            {
                SWSS_LOG_WARN("Update bulk chunk size: prefix %s does not exist", it.first.c_str());
            }
        }

        for (auto &it : m_bulkContexts)
        {
            auto &context = *it.second.get();
            SWSS_LOG_INFO("%s %s partition %s number of OIDs %d number of counter IDs %d number of counters %d",
                          m_name.c_str(),
                          m_instanceId.c_str(),
                          context.name.c_str(),
                          context.object_keys.size(),
                          context.counter_ids.size(),
                          context.counters.size());
        }
    }

    virtual void bulkAddObject(
            _In_ const std::vector<sai_object_id_t>& vids,
                _In_ const std::vector<sai_object_id_t>& rids,
                _In_ const std::vector<std::string>& idStrings,
                _In_ const std::string &per_object_stats_mode)
    {
        SWSS_LOG_ENTER();
        sai_stats_mode_t effective_stats_mode;
        // TODO: use if const expression when c++17 is supported
        if (HasStatsMode<CounterIdsType>::value)
        {
            // Bulk operation is not supported by the counter group.
            SWSS_LOG_INFO("Counter group %s %s does not support bulk. Fallback to single call", m_name.c_str(), m_instanceId.c_str());

            // Fall back to old way
            for (size_t i = 0; i < vids.size(); i++)
            {
                auto rid = rids[i];
                auto vid = vids[i];
                addObject(vid, rid, idStrings, per_object_stats_mode);
            }

            return;
        }
        else
        {
            effective_stats_mode = m_groupStatsMode;
        }

        std::vector<StatType> allCounterIds, supportedIds;
        for (const auto &str : idStrings)
        {
            StatType stat;
            deserializeStat(str.c_str(), &stat);
            {
                allCounterIds.push_back(stat);
            }
        }

        updateSupportedCounters(rids[0]/*it is not really used*/, allCounterIds, effective_stats_mode);
        for (auto stat : allCounterIds)
        {
            if (isCounterSupported(stat))
            {
                supportedIds.push_back(stat);
            }
        }

        if (supportedIds.empty())
        {
            SWSS_LOG_NOTICE("%s %s does not have supported counters", m_name.c_str(), m_instanceId.c_str());
            return;
        }

        std::map<std::vector<StatType>, std::tuple<const std::string, uint32_t>> bulkUnsupportedCounters;
        auto statsMode = m_groupStatsMode == SAI_STATS_MODE_READ ? SAI_STATS_MODE_BULK_READ : SAI_STATS_MODE_BULK_READ_AND_CLEAR;
        auto checkAndUpdateBulkCapability = [&](const std::vector<StatType> &counter_ids, const std::string &prefix, uint32_t bulk_chunk_size)
        {
            BulkContextType ctx;
            // Check bulk capabilities again
            std::vector<StatType> supportedBulkIds;
            sai_status_t status = SAI_STATUS_SUCCESS;
            if (m_supportedBulkCounters.empty())
            {
                status = querySupportedCounters(rids[0], statsMode, m_supportedBulkCounters);
            }
            if (status == SAI_STATUS_SUCCESS && !m_supportedBulkCounters.empty())
            {
                for (auto stat : counter_ids)
                {
                    if (m_supportedBulkCounters.count(stat) != 0)
                    {
                        supportedBulkIds.push_back(stat);
                    }
                }
            }

            if (supportedBulkIds.size() < counter_ids.size())
            {
                // Bulk polling is unsupported for the whole group but single polling is supported
                // Add all objects to m_objectIdsMap so that they will be polled using single API
                for (size_t i = 0; i < vids.size(); i++)
                {
                    auto rid = rids[i];
                    auto vid = vids[i];
                    std::vector<uint64_t> stats(counter_ids.size());
                    if (collectData(rid, counter_ids, effective_stats_mode, false, stats)) {

                        auto it_vid = m_objectIdsMap.find(vid);
                        if (it_vid != m_objectIdsMap.end())
                        {
                            // Remove and re-add if vid already exists
                            m_objectIdsMap.erase(it_vid);
                        }

                        auto counter_data = std::make_shared<CounterIds<StatType>>(rid, counter_ids);
                        m_objectIdsMap.emplace(vid, counter_data);

                        SWSS_LOG_INFO("Fallback to single call for object 0x%" PRIx64, vid);
                    } else {
                        SWSS_LOG_WARN("%s RID %s can't provide the statistic",  m_name.c_str(), sai_serialize_object_id(rid).c_str());
                    }
                }

                return;
            }

            ctx.counter_ids = counter_ids;
            addBulkStatsContext(vids, rids, counter_ids, ctx);
            status = m_vendorSai->bulkGetStats(
                SAI_NULL_OBJECT_ID,
                m_objectType,
                static_cast<uint32_t>(ctx.object_keys.size()),
                ctx.object_keys.data(),
                static_cast<uint32_t>(ctx.counter_ids.size()),
                reinterpret_cast<const sai_stat_id_t *>(ctx.counter_ids.data()),
                statsMode,
                ctx.object_statuses.data(),
                ctx.counters.data());
            if (status == SAI_STATUS_SUCCESS)
            {
                auto bulkContext = getBulkStatsContext(counter_ids, prefix, bulk_chunk_size);
                addBulkStatsContext(vids, rids, counter_ids, *bulkContext.get());
            }
            else
            {
                // Bulk is not supported for this counter prefix
                // Append it to bulkUnsupportedCounters
                std::tuple<const std::string, uint32_t> value(prefix, bulk_chunk_size);
                bulkUnsupportedCounters.emplace(counter_ids, value);
                SWSS_LOG_INFO("Counters starting with %s do not support bulk. Fallback to single call for these counters", prefix.c_str());
            }
        };

        if (m_counterChunkSizeMapFromPrefix.empty())
        {
            std::sort(supportedIds.begin(), supportedIds.end());
            checkAndUpdateBulkCapability(supportedIds, "default", default_bulk_chunk_size);
        }
        else
        {
            std::map<std::string, vector<StatType>> counter_prefix_map;
            std::vector<StatType> default_partition;
            createCounterBulkChunkSizePerPrefixPartition(supportedIds, counter_prefix_map, default_partition);

            for (auto &counterPrefix : counter_prefix_map)
            {
                std::sort(counterPrefix.second.begin(), counterPrefix.second.end());
            }

            std::sort(default_partition.begin(), default_partition.end());

            for (auto &counterPrefix : counter_prefix_map)
            {
                checkAndUpdateBulkCapability(counterPrefix.second, counterPrefix.first, m_counterChunkSizeMapFromPrefix[counterPrefix.first]);
            }

            checkAndUpdateBulkCapability(default_partition, "default", default_bulk_chunk_size);
        }

        if (!bulkUnsupportedCounters.empty())
        {
            SWSS_LOG_NOTICE("Partial counters do not support bulk. Re-check bulk capability for each object");

            for (auto &it : bulkUnsupportedCounters)
            {
                std::vector<sai_object_id_t> bulkSupportedRIDs;
                std::vector<sai_object_id_t> bulkSupportedVIDs;
                for (size_t i = 0; i < vids.size(); i++)
                {
                    auto rid = rids[i];
                    auto vid = vids[i];
                    std::vector<uint64_t> stats(it.first.size());
                    if (checkBulkCapability(vid, rid, it.first))
                    {
                        bulkSupportedVIDs.push_back(vid);
                        bulkSupportedRIDs.push_back(rid);
                    }
                    else if (!double_confirm_supported_counters || collectData(rid, it.first, effective_stats_mode, false, stats))
                    {
                        SWSS_LOG_INFO("Fallback to single call for object 0x%" PRIx64, vid);

                        auto it_vid = m_objectIdsMap.find(vid);
                        if (it_vid != m_objectIdsMap.end())
                        {
                            // Remove and re-add if vid already exists
                            m_objectIdsMap.erase(it_vid);
                        }

                        auto counter_data = std::make_shared<CounterIds<StatType>>(rid, supportedIds);
                        m_objectIdsMap.emplace(vid, counter_data);
                    }
                    else
                    {
                        SWSS_LOG_WARN("%s RID %s can't provide the statistic",  m_name.c_str(), sai_serialize_object_id(rid).c_str());
                    }
                }

                if (!bulkSupportedVIDs.empty() && !bulkSupportedRIDs.empty())
                {
                    auto bulkContext = getBulkStatsContext(it.first, get<0>(it.second), get<1>(it.second));
                    addBulkStatsContext(bulkSupportedVIDs, bulkSupportedRIDs, it.first, *bulkContext.get());
                }
            }
        }
    }

    void removeObject(
            _In_ sai_object_id_t vid) override
    {
        SWSS_LOG_ENTER();

        removeObject(vid, true);
    }

    void removeObject(
            _In_ sai_object_id_t vid,
            _In_ bool log)
    {
        SWSS_LOG_ENTER();

        auto iter = m_objectIdsMap.find(vid);
        if (iter != m_objectIdsMap.end())
        {
            m_objectIdsMap.erase(iter);
        }

        // An object can be in both m_objectIdsMap and the bulk context
        // when bulk polling is supported by some counter prefixes but unsupported by some others
        if (!removeBulkStatsContext(vid) && log)
        {
            SWSS_LOG_NOTICE("Trying to remove nonexisting %s %s",
                            sai_serialize_object_type(m_objectType).c_str(),
                            sai_serialize_object_id(vid).c_str());
        }
    }

    virtual void collectData(
            _In_ swss::Table &countersTable) override
    {
        SWSS_LOG_ENTER();
        sai_stats_mode_t effective_stats_mode = m_groupStatsMode;
        for (const auto &kv : m_objectIdsMap)
        {
            const auto &vid = kv.first;
            const auto &rid = kv.second->rid;
            const auto &statIds = kv.second->counter_ids;

            // TODO: use if const expression when cpp17 is supported
            if (HasStatsMode<CounterIdsType>::value)
            {
                effective_stats_mode = (m_groupStatsMode == SAI_STATS_MODE_READ_AND_CLEAR ||
                                        kv.second->getStatsMode() == SAI_STATS_MODE_READ_AND_CLEAR) ? SAI_STATS_MODE_READ_AND_CLEAR : SAI_STATS_MODE_READ;
            }

            std::vector<uint64_t> stats(statIds.size());
            if (!collectData(rid, statIds, effective_stats_mode, true, stats))
            {
                continue;
            }

            std::vector<swss::FieldValueTuple> values;
            for (size_t i = 0; i != statIds.size(); i++)
            {
                values.emplace_back(serializeStat(statIds[i]), std::to_string(stats[i]));
            }
            countersTable.set(sai_serialize_object_id(vid), values, "");
        }

        for (const auto &kv : m_bulkContexts)
        {
            bulkCollectData(countersTable, *kv.second.get());
        }
    }

    void runPlugin(
            _In_ swss::DBConnector& counters_db,
            _In_ const std::vector<std::string>& argv) override
    {
        SWSS_LOG_ENTER();

        if (!hasObject())
        {
            return;
        }

        SWSS_LOG_DEBUG("Before running plugin %s %s", m_instanceId.c_str(), m_name.c_str());

        std::vector<std::string> idStrings;
        idStrings.reserve(m_objectIdsMap.size());
        std::transform(m_objectIdsMap.begin(),
                       m_objectIdsMap.end(),
                       std::back_inserter(idStrings),
                       [] (auto &kv) { return sai_serialize_object_id(kv.first); });

        for (auto &kv : m_bulkContexts)
        {
            std::transform(kv.second->object_vids.begin(),
                           kv.second->object_vids.end(),
                           std::back_inserter(idStrings),
                           [] (auto &vid) { return sai_serialize_object_id(vid); });
        }

        std::for_each(m_plugins.begin(),
                      m_plugins.end(),
                      [&] (auto &sha) { runRedisScript(counters_db, sha, idStrings, argv); });

        SWSS_LOG_DEBUG("After running plugin %s %s", m_instanceId.c_str(), m_name.c_str());
    }

    bool hasObject() const override
    {
        SWSS_LOG_ENTER();
        return !m_objectIdsMap.empty() || !m_bulkContexts.empty();
    }

private:
    bool isCounterSupported(
            _In_ StatType counter) const
    {
        SWSS_LOG_ENTER();
        return m_supportedCounters.count(counter) != 0;
    }

    bool collectData(
            _In_ sai_object_id_t rid,
            _In_ const std::vector<StatType> &counter_ids,
            _In_ sai_stats_mode_t stats_mode,
            _In_ bool log_err,
            _Out_ std::vector<uint64_t> &stats)
    {
        SWSS_LOG_ENTER();
        sai_status_t status;
        if (!use_sai_stats_ext)
        {
            status = m_vendorSai->getStats(
                    m_objectType,
                    rid,
                    static_cast<uint32_t>(counter_ids.size()),
                    (const sai_stat_id_t *)counter_ids.data(),
                    stats.data());

            if (status != SAI_STATUS_SUCCESS)
            {
                if (log_err)
                    SWSS_LOG_ERROR("Failed to get stats of %s 0x%" PRIx64 ": %d", m_name.c_str(), rid, status);
                else
                    SWSS_LOG_INFO("Failed to get stats of %s 0x%" PRIx64 ": %d", m_name.c_str(), rid, status);
                return false;
            }

            if (stats_mode == SAI_STATS_MODE_READ_AND_CLEAR)
            {
                status = m_vendorSai->clearStats(
                        m_objectType,
                        rid,
                        static_cast<uint32_t>(counter_ids.size()),
                        reinterpret_cast<const sai_stat_id_t *>(counter_ids.data()));
                if (status != SAI_STATUS_SUCCESS)
                {
                    if (log_err)
                    {
                        SWSS_LOG_ERROR("%s: failed to clear stats %s, rv: %s",
                                m_name.c_str(),
                                sai_serialize_object_id(rid).c_str(),
                                sai_serialize_status(status).c_str());
                    }
                    else
                    {
                            SWSS_LOG_INFO("%s: failed to clear stats %s, rv: %s",
                                m_name.c_str(),
                                sai_serialize_object_id(rid).c_str(),
                                sai_serialize_status(status).c_str());
                    }
                    return false;
                }
            }
        }
        else
        {
            status = m_vendorSai->getStatsExt(
                    m_objectType,
                    rid,
                    static_cast<uint32_t>(counter_ids.size()),
                    reinterpret_cast<const sai_stat_id_t *>(counter_ids.data()),
                    stats_mode,
                    stats.data());
            if (status != SAI_STATUS_SUCCESS)
            {
                if (log_err)
                {
                    SWSS_LOG_ERROR("Failed to get stats of %s 0x%" PRIx64 ": %d", m_name.c_str(), rid, status);
                }
                else
                {
                    SWSS_LOG_INFO("Failed to get stats of %s 0x%" PRIx64 ": %d", m_name.c_str(), rid, status);
                }
                return false;
            }
        }

        return true;
    }

    void bulkCollectData(
        _In_ swss::Table &countersTable,
        _Inout_ BulkContextType &ctx)
    {
        SWSS_LOG_ENTER();
        auto statsMode = m_groupStatsMode == SAI_STATS_MODE_READ ? SAI_STATS_MODE_BULK_READ : SAI_STATS_MODE_BULK_READ_AND_CLEAR;
        uint32_t bulk_chunk_size = ctx.default_bulk_chunk_size;
        uint32_t size = static_cast<uint32_t>(ctx.object_keys.size());
        if (bulk_chunk_size > size || bulk_chunk_size == 0)
        {
            bulk_chunk_size = size;
        }
        uint32_t current = 0;

        SWSS_LOG_INFO("Before getting bulk %s %s %s size %u bulk chunk size %u current %u", m_instanceId.c_str(), m_name.c_str(), ctx.name.c_str(), size, bulk_chunk_size, current);

        while (current < size)
        {
            sai_status_t status = m_vendorSai->bulkGetStats(
                SAI_NULL_OBJECT_ID,
                m_objectType,
                bulk_chunk_size,
                ctx.object_keys.data() + current,
                static_cast<uint32_t>(ctx.counter_ids.size()),
                reinterpret_cast<const sai_stat_id_t *>(ctx.counter_ids.data()),
                statsMode,
                ctx.object_statuses.data() + current,
                ctx.counters.data() + current * ctx.counter_ids.size());
            if (SAI_STATUS_SUCCESS != status)
            {
                SWSS_LOG_WARN("Failed to bulk get stats for %s %s %s %s starting object %u bulk chunk size %u: %d",
                              m_instanceId.c_str(), m_name.c_str(), ctx.name.c_str(), sai_serialize_object_type(m_objectType).c_str(), current, bulk_chunk_size, status);
            }
            current += bulk_chunk_size;

            SWSS_LOG_DEBUG("After getting bulk %s %s %s index %u(advanced to %u) bulk chunk size %u", m_instanceId.c_str(), m_name.c_str(), ctx.name.c_str(), current - bulk_chunk_size, current, bulk_chunk_size);

            if (size - current < bulk_chunk_size)
            {
                bulk_chunk_size = size - current;
            }
        }

        SWSS_LOG_INFO("After getting bulk %s %s %s total %u objects", m_instanceId.c_str(), m_name.c_str(), ctx.name.c_str(), size);

        auto time_stamp = std::chrono::steady_clock::now().time_since_epoch().count();

        std::vector<swss::FieldValueTuple> values;
        for (size_t i = 0; i < ctx.object_keys.size(); i++)
        {
            if (SAI_STATUS_SUCCESS != ctx.object_statuses[i])
            {
                SWSS_LOG_ERROR("Failed to get stats of %s 0x%" PRIx64 " 0x%" PRIx64 ": %d", m_name.c_str(), ctx.object_vids[i], ctx.object_keys[i].key.object_id, ctx.object_statuses[i]);
                continue;
            }
            const auto &vid = ctx.object_vids[i];

            for (size_t j = 0; j < ctx.counter_ids.size(); j++)
            {
                values.emplace_back(serializeStat(ctx.counter_ids[j]), std::to_string(ctx.counters[i * ctx.counter_ids.size() + j]));
            }

            countersTable.set(sai_serialize_object_id(vid), values, "");
            values.clear();
        }

        // First generate the key, then replace spaces with underscores to avoid issues when Lua plugins handle the timestamp
        std::string timestamp_key = m_instanceId + "_" + m_name + "_time_stamp";
        std::replace(timestamp_key.begin(), timestamp_key.end(), ' ', '_');

        values.emplace_back(timestamp_key, std::to_string(time_stamp));
        countersTable.set("TIME_STAMP", values, "");

        SWSS_LOG_DEBUG("After pushing db %s %s %s", m_instanceId.c_str(), m_name.c_str(), ctx.name.c_str());
    }

    auto getBulkStatsContext(
        _In_ const std::vector<StatType>& counterIds,
        _In_ const std::string& name,
        _In_ uint32_t bulk_chunk_size=0)
    {
        SWSS_LOG_ENTER();
        auto iter = m_bulkContexts.find(counterIds);
        if (iter != m_bulkContexts.end())
        {
            return iter->second;
        }

        SWSS_LOG_NOTICE("Create bulk stat context %s %s %s", m_instanceId.c_str(), m_name.c_str(), name.c_str());
        auto ret = m_bulkContexts.emplace(counterIds, std::make_shared<BulkContextType>());
        ret.first->second.get()->name = name;
        ret.first->second.get()->default_bulk_chunk_size = bulk_chunk_size;
        ret.first->second.get()->counter_ids = counterIds;
        return ret.first->second;
    }

    void setBulkStatsContext(
        _In_ const std::vector<StatType>& counterIds,
        _In_ const std::shared_ptr<BulkContextType> ptr)
    {
        SWSS_LOG_ENTER();
        m_bulkContexts.emplace(counterIds, ptr);
        ptr.get()->counter_ids = counterIds;
    }

    void addBulkStatsContext(
            _In_    sai_object_id_t vid,
            _In_    sai_object_id_t rid,
            _In_    const std::vector<StatType>& counterIds,
            _Inout_ BulkContextType &ctx)
    {
        SWSS_LOG_ENTER();
        ctx.object_vids.push_back(vid);
        sai_object_key_t object_key;
        object_key.key.object_id = rid;
        ctx.object_keys.push_back(object_key);
        ctx.object_statuses.push_back(SAI_STATUS_SUCCESS);
        ctx.counters.resize(counterIds.size() * ctx.object_keys.size());
    }

    void addBulkStatsContext(
            _In_    const std::vector<sai_object_id_t> &vids,
            _In_    const std::vector<sai_object_id_t> &rids,
            _In_    const std::vector<StatType>& counterIds,
            _Inout_ BulkContextType &ctx)
    {
        SWSS_LOG_ENTER();
        ctx.object_vids.insert(ctx.object_vids.end(), vids.begin(), vids.end());
        transform(rids.begin(), rids.end(), back_inserter(ctx.object_keys), [](sai_object_id_t rid) {
            sai_object_key_t key;
            key.key.object_id = rid;
            return key;
        });
        ctx.object_statuses.insert(ctx.object_statuses.end(), vids.size(), SAI_STATUS_SUCCESS);
        ctx.counters.resize(counterIds.size() * ctx.object_keys.size());
    }

    bool removeBulkStatsContext(
        _In_  sai_object_id_t vid)
    {
        SWSS_LOG_ENTER();
        std::set<std::vector<StatType>> bulkContextsToBeRemoved;
        bool found = false;
        for (auto iter = m_bulkContexts.begin(); iter != m_bulkContexts.end(); iter++)
        {
            auto &ctx = *iter->second.get();
            auto vid_iter = std::find(ctx.object_vids.begin(), ctx.object_vids.end(), vid);
            if (vid_iter == ctx.object_vids.end())
            {
                continue;
            }
            found = true;
            auto index = std::distance(ctx.object_vids.begin(), vid_iter);
            ctx.object_vids.erase(vid_iter);
            if (ctx.object_vids.empty())
            {
                // It can change the order of the map to erase an element in a loop iterating the map
                // which can cause some elements to be skipped or iterated for multiple times
                bulkContextsToBeRemoved.insert(iter->first);
            }
            else
            {
                auto key_iter = ctx.object_keys.begin();
                std::advance(key_iter, index);
                ctx.object_keys.erase(key_iter);
                ctx.counters.resize(ctx.counter_ids.size() * ctx.object_keys.size());
                ctx.object_statuses.pop_back();
            }
            if (m_counterChunkSizeMapFromPrefix.empty())
            {
                break;
            }
            else
            {
                // There can be more than one bulk context containing the VID when the per counter ID bulk chunk size is configured
                continue;
            }
        }

        for (auto iter : bulkContextsToBeRemoved)
        {
            m_bulkContexts.erase(iter);
        }

        return found;
    }

    bool checkBulkCapability(
            _In_ sai_object_id_t vid,
            _In_ sai_object_id_t rid,
            _In_ const std::vector<StatType>& counter_ids)
    {
        SWSS_LOG_ENTER();
        BulkContextType ctx;
        ctx.counter_ids = counter_ids;
        addBulkStatsContext(vid, rid, counter_ids, ctx);
        auto statsMode = m_groupStatsMode == SAI_STATS_MODE_READ ? SAI_STATS_MODE_BULK_READ : SAI_STATS_MODE_BULK_READ_AND_CLEAR;
        sai_status_t status = m_vendorSai->bulkGetStats(
                            SAI_NULL_OBJECT_ID,
                            m_objectType,
                            static_cast<uint32_t>(ctx.object_keys.size()),
                            ctx.object_keys.data(),
                            static_cast<uint32_t>(ctx.counter_ids.size()),
                            reinterpret_cast<const sai_stat_id_t *>(ctx.counter_ids.data()),
                            statsMode,
                            ctx.object_statuses.data(),
                            ctx.counters.data());
        return status == SAI_STATUS_SUCCESS;
    }

    void updateSupportedCounters(
            _In_ sai_object_id_t rid,
            _In_ const std::vector<StatType>& counter_ids,
            _In_ sai_stats_mode_t stats_mode)
    {
        SWSS_LOG_ENTER();
        if (!m_supportedCounters.empty() && !always_check_supported_counters)
        {
            SWSS_LOG_NOTICE("Ignore checking of supported counters");
            return;
        }

        if (always_check_supported_counters && !dont_clear_support_counter)
        {
            m_supportedCounters.clear();
        }

        if (!use_sai_stats_capa_query || querySupportedCounters(rid, stats_mode, m_supportedCounters) != SAI_STATUS_SUCCESS)
        {
            /* Fallback to legacy approach */
            getSupportedCounters(rid, counter_ids, stats_mode);
        }
    }

    sai_status_t querySupportedCounters(
            _In_ sai_object_id_t rid,
            _In_ sai_stats_mode_t stats_mode,
            _Out_ std::set<StatType> &supportedCounters)
    {
        SWSS_LOG_ENTER();
        sai_stat_capability_list_t stats_capability;
        stats_capability.count = 0;
        stats_capability.list = nullptr;

        if (m_switchId == SAI_NULL_OBJECT_ID)
        {
            m_switchId = m_vendorSai->switchIdQuery(rid);
        }

        /* First call is to check the size needed to allocate */
        sai_status_t status = m_vendorSai->queryStatsCapability(
            m_switchId,
            m_objectType,
            &stats_capability);

        /* Second call is for query statistics capability */
        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            std::vector<sai_stat_capability_t> statCapabilityList(stats_capability.count);
            stats_capability.list = statCapabilityList.data();
            status = m_vendorSai->queryStatsCapability(
                m_switchId,
                m_objectType,
                &stats_capability);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_INFO("Unable to get %s supported counters for %s",
                    m_name.c_str(),
                    sai_serialize_object_id(rid).c_str());
            }
            else
            {
                for (auto statCapability: statCapabilityList)
                {
                    auto currentStatModes = statCapability.stat_modes;
                    if (!(currentStatModes & stats_mode))
                    {
                        continue;
                    }

                    StatType counter = static_cast<StatType>(statCapability.stat_enum);
                    supportedCounters.insert(counter);
                }
            }
        }
        return status;
    }

    void getSupportedCounters(
            _In_ sai_object_id_t rid,
            _In_ const std::vector<StatType>& counter_ids,
            _In_ sai_stats_mode_t stats_mode)
    {
        SWSS_LOG_ENTER();
        std::vector<uint64_t> values(1);

        for (const auto &counter : counter_ids)
        {
            if (isCounterSupported(counter))
            {
                continue;
            }

            std::vector<StatType> tmp_counter_ids {counter};
            if (!collectData(rid, tmp_counter_ids, stats_mode, false, values))
            {
                continue;
            }

            m_supportedCounters.insert(counter);
        }
    }

protected:
    sai_object_type_t m_objectType;
    sairedis::SaiInterface *m_vendorSai;
    sai_stats_mode_t& m_groupStatsMode;
    std::set<StatType> m_supportedCounters;
    std::set<StatType> m_supportedBulkCounters;
    std::map<sai_object_id_t, std::shared_ptr<CounterIdsType>> m_objectIdsMap;
    std::map<std::vector<StatType>, std::shared_ptr<BulkContextType>> m_bulkContexts;
};

template <typename AttrType>
class AttrContext : public CounterContext<AttrType>
{
public:
    typedef CounterIds<AttrType> AttrIdsType;
    typedef CounterContext<AttrType> Base;
    AttrContext(
            _In_ const std::string &name,
            _In_ const std::string &instance,
            _In_ sai_object_type_t object_type,
            _In_ sairedis::SaiInterface *vendor_sai,
            _In_ sai_stats_mode_t &stats_mode):
    CounterContext<AttrType>(name, instance, object_type, vendor_sai, stats_mode)
    {
        SWSS_LOG_ENTER();
    }

    void addObject(
            _In_ sai_object_id_t vid,
            _In_ sai_object_id_t rid,
            _In_ const std::vector<std::string> &idStrings,
            _In_ const std::string &per_object_stats_mode) override
    {
        SWSS_LOG_ENTER();

        std::vector<AttrType> attrIds;

        for (const auto &str : idStrings)
        {
            AttrType attr;
            deserializeAttr(str, attr);
            attrIds.push_back(attr);
        }

        auto it = Base::m_objectIdsMap.find(vid);
        if (it != Base::m_objectIdsMap.end())
        {
            it->second->counter_ids = attrIds;
            return;
        }

        auto attr_ids = std::make_shared<AttrIdsType>(rid, attrIds);
        Base::m_objectIdsMap.emplace(vid, attr_ids);
    }

    void bulkAddObject(
            _In_ const std::vector<sai_object_id_t>& vids,
            _In_ const std::vector<sai_object_id_t>& rids,
            _In_ const std::vector<std::string>& idStrings,
            _In_ const std::string &per_object_stats_mode) override
    {
        SWSS_LOG_ENTER();

        for (auto i = 0uL; i < vids.size(); i++)
        {
            addObject(vids[i], rids[i], idStrings, per_object_stats_mode);
        }
    }

    void collectData(
            _In_ swss::Table &countersTable) override
    {
        SWSS_LOG_ENTER();

        for (const auto &kv : Base::m_objectIdsMap)
        {
            const auto &vid = kv.first;
            const auto &rid = kv.second->rid;
            const auto &attrIds = kv.second->counter_ids;

            std::vector<sai_attribute_t> attrs(attrIds.size());
            for (size_t i = 0; i < attrIds.size(); i++)
            {
                attrs[i].id = attrIds[i];
            }

            // Get attr
            sai_status_t status = Base::m_vendorSai->get(
                    Base::m_objectType,
                    rid,
                    static_cast<uint32_t>(attrIds.size()),
                    attrs.data());

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("Failed to get attr of %s 0x%" PRIx64 ": %d",
                        sai_serialize_object_type(Base::m_objectType).c_str(), vid, status);
                continue;
            }

            std::vector<swss::FieldValueTuple> values;
            for (size_t i = 0; i != attrIds.size(); i++)
            {
                auto meta = sai_metadata_get_attr_metadata(Base::m_objectType, attrs[i].id);
                if (!meta)
                {
                    SWSS_LOG_THROW("Failed to get metadata for %s", sai_serialize_object_type(Base::m_objectType).c_str());
                }
                values.emplace_back(meta->attridname, sai_serialize_attr_value(*meta, attrs[i]));
            }
            countersTable.set(sai_serialize_object_id(vid), values, "");
        }
    }
};

class DashMeterCounterContext : public BaseCounterContext
{
public:
    DashMeterCounterContext(
            _In_ const std::string &name,
            _In_ const std::string &instance,
            _In_ sairedis::SaiInterface *vendor_sai,
            _In_ std::string dbCounters):
    BaseCounterContext(name, instance), m_dbCounters(dbCounters), m_vendorSai(vendor_sai)
    {
        SWSS_LOG_ENTER();
    }

    void addObject(
            _In_ sai_object_id_t vid,
            _In_ sai_object_id_t rid,
            _In_ const std::vector<std::string> &idStrings,
            _In_ const std::string &per_object_stats_mode) override
    {
        SWSS_LOG_ENTER();

        if (m_switchId == 0UL)
        {
            m_switchId = m_vendorSai->switchIdQuery(rid);
        }

        if (m_meterBucketsPerEni == 0)
        {
            if (m_initalized) {
                // need not repeat these global checks for each object
                return;
            }
            sai_attribute_t attr;
            attr.id = SAI_SWITCH_ATTR_DASH_CAPS_MAX_METER_BUCKET_COUNT_PER_ENI;
            auto status = m_vendorSai->get(SAI_OBJECT_TYPE_SWITCH, m_switchId,
                                           1, &attr);
            if ((status != SAI_STATUS_SUCCESS) ||
                (attr.value.u32 == 0))
            {
                m_initalized = true;
                SWSS_LOG_NOTICE("No meter buckets supported per ENI for %s %s",
                                m_name.c_str(), sai_serialize_object_id(rid).c_str());
                return;
            }
            m_meterBucketsPerEni = attr.value.u32;
        }

        if (m_supportedMeterCounters.empty())
        {
            if (m_initalized) {
                // need not repeat these global checks for each object
                return;
            }
            getSupportedMeterCounters(rid, idStrings);
            if (m_supportedMeterCounters.empty())
            {
                m_initalized = true;
                SWSS_LOG_NOTICE("%s %s does not have supported counters",
                                m_name.c_str(), sai_serialize_object_id(rid).c_str());
                return;
            }
            if (!checkBulkCapability(vid, rid, m_supportedMeterCounters, m_groupStatsMode)) {
                m_initalized = true;
                SWSS_LOG_NOTICE("%s %s does not have bulk get support for Dash meter counters",
                                m_name.c_str(), sai_serialize_object_id(rid).c_str());
                return;
            }
        }
        // add object to flex counter poll
        addBulkMeterContext(vid, rid);
        m_initalized = true;
    }

    void removeObject(_In_ sai_object_id_t vid) override
    {
        SWSS_LOG_ENTER();
        auto it = m_bulkMeterContexts.find(vid);
        if (it == m_bulkMeterContexts.end()) {
            return;
        }
        // delete all meter bucket stats for this object from counters DB
        swss::DBConnector db(m_dbCounters, 0);
        swss::RedisPipeline pipeline(&db);
        swss::Table countersTable(&pipeline, COUNTERS_TABLE, true);
        for (const auto& object_key: it->second.object_keys) {
           countersTable.del(sai_serialize_meter_bucket_entry(object_key.key.meter_bucket_entry));
        }
        // remove from flex counter poll
        m_bulkMeterContexts.erase(it);
    }

    void collectData(_In_ swss::Table &countersTable) override
    {
        SWSS_LOG_ENTER();
        for (auto &kv : m_bulkMeterContexts)
        {
            bulkCollectData(countersTable, kv.second);
        }
    }

    void runPlugin(
            _In_ swss::DBConnector& counters_db,
            _In_ const std::vector<std::string>& argv) override
    {
        SWSS_LOG_ENTER();

        if (!hasObject())
        {
            return;
        }

        for (auto &kv : m_bulkMeterContexts)
        {
            auto& ctx = kv.second;
            std::vector<std::string> idStrings;
            idStrings.reserve(m_meterBucketsPerEni);

            for (uint32_t i = 0; i < m_meterBucketsPerEni; ++i) {
                idStrings.push_back(sai_serialize_meter_bucket_entry(ctx.object_keys[i].key.meter_bucket_entry));
            }
            std::for_each(m_plugins.begin(),
                          m_plugins.end(),
                          [&] (auto &sha) { runRedisScript(counters_db, sha, idStrings, argv); });
        }
    }

    void bulkAddObject(
            _In_ const std::vector<sai_object_id_t>& vids,
            _In_ const std::vector<sai_object_id_t>& rids,
            _In_ const std::vector<std::string>& idStrings,
            _In_ const std::string &per_object_stats_mode) override
    {
        SWSS_LOG_ENTER();

        for (auto i = 0uL; i < vids.size(); i++)
        {
            addObject(vids[i], rids[i], idStrings, per_object_stats_mode);
        }
    }

    bool hasObject() const override
    {
        SWSS_LOG_ENTER();
        return !m_bulkMeterContexts.empty();
    }

private:
    struct BulkMeterStatsContext
    {
        sai_object_id_t eni_vid;
        std::vector<sai_object_key_t> object_keys;
        std::vector<sai_status_t> object_statuses;
        std::vector<sai_meter_bucket_entry_stat_t> counter_ids;
        std::vector<uint64_t> counters;
    };
    bool bulkCollectData(
        _In_ swss::Table &countersTable,
        _Inout_ BulkMeterStatsContext &ctx)
    {
        SWSS_LOG_ENTER();

        if (ctx.object_keys.size() == 0)
        {
            return false;
        }

        auto statsMode = m_groupStatsMode == SAI_STATS_MODE_READ ? SAI_STATS_MODE_BULK_READ : SAI_STATS_MODE_BULK_READ_AND_CLEAR;

        sai_status_t status = m_vendorSai->bulkGetStats(
            SAI_NULL_OBJECT_ID,
            m_objectType,
            static_cast<uint32_t>(ctx.object_keys.size()),
            ctx.object_keys.data(),
            static_cast<uint32_t>(ctx.counter_ids.size()),
            reinterpret_cast<const sai_stat_id_t *>(ctx.counter_ids.data()),
            statsMode,
            ctx.object_statuses.data(),
            ctx.counters.data());

        if (SAI_STATUS_SUCCESS != status)
        {
            SWSS_LOG_WARN("Failed to get meter bulk stats for %s: ENI 0x% " PRIx64 ": %u", m_name.c_str(), ctx.eni_vid, status);
            return false;
        }

        bool meter_class_hit = false;
        std::vector<swss::FieldValueTuple> values;
        for (size_t i = 0; i < ctx.object_keys.size(); i++)
        {
            if (SAI_STATUS_SUCCESS != ctx.object_statuses[i])
            {
                SWSS_LOG_ERROR("Failed to get meter bulk stats of %s for ENI 0x%" PRIx64 " meter-class %d : %d",
                               m_name.c_str(), ctx.object_keys[i].key.meter_bucket_entry.eni_id,
                               ctx.object_keys[i].key.meter_bucket_entry.meter_class,
                               ctx.object_statuses[i]);
                continue;
            }
            for (size_t j = 0; j < ctx.counter_ids.size(); ++j) {
                if (ctx.counters[i * ctx.counter_ids.size() + j] != 0UL) {
                    meter_class_hit = true;
                    break;
                }
            }
            // write only non-zero meter classes to COUNTERS_DB
            if (!meter_class_hit) {
                continue;
            }
            meter_class_hit = false;
            for (size_t j = 0; j < ctx.counter_ids.size(); j++)
            {
                values.emplace_back(serializeStat(ctx.counter_ids[j]), std::to_string(ctx.counters[i * ctx.counter_ids.size() + j]));
            }
            countersTable.set(sai_serialize_meter_bucket_entry(ctx.object_keys[i].key.meter_bucket_entry), values, "");
            values.clear();
        }
        return true;
    }

    bool checkBulkCapability(
            _In_ sai_object_id_t vid,
            _In_ sai_object_id_t rid,
            _In_ const std::vector<sai_meter_bucket_entry_stat_t>& counter_ids,
            _In_ sai_stats_mode_t stats_mode)
    {
        SWSS_LOG_ENTER();

        auto ctx = makeBulkMeterContext(vid, rid);
        auto statsMode = stats_mode == SAI_STATS_MODE_READ ? SAI_STATS_MODE_BULK_READ : SAI_STATS_MODE_BULK_READ_AND_CLEAR;
        sai_status_t status = m_vendorSai->bulkGetStats(
                            SAI_NULL_OBJECT_ID,
                            m_objectType,
                            static_cast<uint32_t>(ctx.object_keys.size()),
                            ctx.object_keys.data(),
                            static_cast<uint32_t>(ctx.counter_ids.size()),
                            reinterpret_cast<const sai_stat_id_t *>(ctx.counter_ids.data()),
                            statsMode,
                            ctx.object_statuses.data(),
                            ctx.counters.data());
        return status == SAI_STATUS_SUCCESS;
    }

    void updateSupportedMeterCounters(
            _In_ sai_object_id_t rid,
            _In_ const std::vector<sai_meter_bucket_entry_stat_t>& counter_ids,
            _In_ sai_stats_mode_t stats_mode)
    {
        SWSS_LOG_ENTER();

        if (!m_supportedMeterCounters.empty() && !always_check_supported_counters)
        {
            SWSS_LOG_NOTICE("Ignore checking of supported counters");
            return;
        }
        querySupportedMeterCounters(rid, counter_ids, stats_mode);
    }

    sai_status_t querySupportedMeterCounters(
            _In_ sai_object_id_t rid,
            _In_ const std::vector<sai_meter_bucket_entry_stat_t>& counter_ids,
            _In_ sai_stats_mode_t stats_mode)
    {
        SWSS_LOG_ENTER();
        sai_stat_capability_list_t stats_capability;
        stats_capability.count = 0;
        stats_capability.list = nullptr;

        /* First call is to check the size needed to allocate */
        sai_status_t status = m_vendorSai->queryStatsCapability(m_switchId,
                                                                m_objectType,
                                                                &stats_capability);

        /* Second call is for query statistics capability */
        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            std::vector<sai_stat_capability_t> statCapabilityList(stats_capability.count);
            stats_capability.list = statCapabilityList.data();
            status = m_vendorSai->queryStatsCapability(m_switchId,
                                                       m_objectType,
                                                       &stats_capability);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_INFO("Unable to get %s supported meter counters for %s",
                    m_name.c_str(),
                    sai_serialize_object_id(rid).c_str());
            }
            else
            {
                for (auto counter: counter_ids) {
                    for (auto statCapability: statCapabilityList)
                    {
                        auto currentStatModes = statCapability.stat_modes;
                        if (!(currentStatModes & stats_mode))
                        {
                            continue;
                        }

                        if (counter == static_cast<sai_meter_bucket_entry_stat_t>(statCapability.stat_enum)) {
                            m_supportedMeterCounters.push_back(counter);
                        }
                    }
                }
            }
        }
        return status;
    }

    void getSupportedMeterCounters(sai_object_id_t rid,
                                   const std::vector<std::string> &idStrings)
    {
        SWSS_LOG_ENTER();
        std::vector<sai_meter_bucket_entry_stat_t> counter_ids;

        for (const auto &str : idStrings)
        {
            SWSS_LOG_INFO("id string %s", str.c_str());
            sai_meter_bucket_entry_stat_t stat;
            deserializeStat(str.c_str(), &stat);
            counter_ids.push_back(stat);
        }
        updateSupportedMeterCounters(rid, counter_ids, m_groupStatsMode);
    }

    BulkMeterStatsContext makeBulkMeterContext(sai_object_id_t vid, sai_object_id_t rid)
    {
        SWSS_LOG_ENTER();
        BulkMeterStatsContext ctx;

        ctx.eni_vid = vid;
        sai_object_key_t object_key;
        object_key.key.meter_bucket_entry.eni_id = rid;
        object_key.key.meter_bucket_entry.switch_id = m_switchId;
        // Populate object keys for ENI meter classes: 1 - max-capable.
        // 0 is not a valid meter class since its used to denote traffic that is not metered
        for (uint32_t i = 1; i <= m_meterBucketsPerEni; ++i) {
            object_key.key.meter_bucket_entry.meter_class = i;
            ctx.object_keys.push_back(object_key);
        }
        ctx.object_statuses.resize(ctx.object_keys.size());
        ctx.counter_ids = m_supportedMeterCounters;
        ctx.counters.resize(m_supportedMeterCounters.size() * ctx.object_keys.size());

        return ctx;
    }
    void addBulkMeterContext(sai_object_id_t vid, sai_object_id_t rid)
    {
        SWSS_LOG_ENTER();
        auto it = m_bulkMeterContexts.find(vid);
        if (it != m_bulkMeterContexts.end()) {
            return;
        }
        m_bulkMeterContexts.emplace(vid, makeBulkMeterContext(vid, rid));
    }

    std::map<sai_object_id_t, BulkMeterStatsContext> m_bulkMeterContexts;
    std::vector<sai_meter_bucket_entry_stat_t> m_supportedMeterCounters;
    sai_object_type_t m_objectType = (sai_object_type_t) SAI_OBJECT_TYPE_METER_BUCKET_ENTRY;
    std::string m_dbCounters;
    sairedis::SaiInterface *m_vendorSai;
    sai_stats_mode_t m_groupStatsMode = SAI_STATS_MODE_READ;
    sai_object_id_t m_switchId = 0UL;
    uint32_t m_meterBucketsPerEni = 0;
    bool m_initalized = false;
};

FlexCounter::FlexCounter(
        _In_ const std::string& instanceId,
        _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
        _In_ const std::string& dbCounters,
        _In_ const bool noDoubleCheckBulkCapability):
    m_readyToPoll(false),
    m_pollInterval(0),
    m_instanceId(instanceId),
    m_vendorSai(vendorSai),
    m_dbCounters(dbCounters),
    m_noDoubleCheckBulkCapability(noDoubleCheckBulkCapability)
{
    SWSS_LOG_ENTER();

    m_enable = false;
    m_isDiscarded = false;

    startFlexCounterThread();
}

FlexCounter::~FlexCounter(void)
{
    SWSS_LOG_ENTER();

    endFlexCounterThread();
}

void FlexCounter::setPollInterval(
        _In_ uint32_t pollInterval)
{
    SWSS_LOG_ENTER();

    if (m_pollInterval != pollInterval)
    {
        m_pollInterval = pollInterval;
        m_cvSleep.notify_all();

        SWSS_LOG_INFO("Set POLL INTERVAL %d for FC %s", pollInterval, m_instanceId.c_str());
    }
}

void FlexCounter::setStatus(
        _In_ const std::string& status)
{
    SWSS_LOG_ENTER();

    const auto &cit = statusMap.find(status);
    if (cit == statusMap.cend())
    {
        SWSS_LOG_WARN("Input value %s is not supported for Flex counter status, enter enable or disable", status.c_str());
        return;
    }

    if (m_enable != cit->second)
    {
        m_enable = cit->second;
        m_cvSleep.notify_all();

        SWSS_LOG_INFO("Set STATUS %s for FC %s", status.c_str(), m_instanceId.c_str());
    }
}

void FlexCounter::setStatsMode(
        _In_ const std::string& mode)
{
    SWSS_LOG_ENTER();

    const auto &cit = statsModeMap.find(mode);
    if (cit == statsModeMap.cend())
    {
        SWSS_LOG_WARN("Input value %s is not supported for Flex counter stats mode, enter STATS_MODE_READ or STATS_MODE_READ_AND_CLEAR", mode.c_str());
        return;
    }

    if (m_statsMode != cit->second)
    {
        m_statsMode = cit->second;
        m_cvSleep.notify_all();

        SWSS_LOG_INFO("Set STATS MODE %s for FC %s", mode.c_str(), m_instanceId.c_str());
    }
}

void FlexCounter::removeDataFromCountersDB(
        _In_ sai_object_id_t vid,
        _In_ const std::string &ratePrefix)
{
    SWSS_LOG_ENTER();
    swss::DBConnector db(m_dbCounters, 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    std::string vidStr = sai_serialize_object_id(vid);
    countersTable.del(vidStr);
    if (!ratePrefix.empty())
    {
        swss::Table ratesTable(&pipeline, RATES_TABLE, false);
        ratesTable.del(vidStr);
        ratesTable.del(vidStr + ratePrefix);
    }
}

void FlexCounter::removeCounterPlugins()
{
    MUTEX;

    SWSS_LOG_ENTER();

    for (const auto &kv : m_counterContext)
    {
        kv.second->removePlugins();
    }

    m_isDiscarded = true;
}

void FlexCounter::addCounterPlugin(
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    MUTEX;

    SWSS_LOG_ENTER();

    m_isDiscarded = false;
    uint32_t bulkChunkSize = 0;
    std::string bulkChunkSizePerPrefix;

    for (auto& fvt: values)
    {
        auto& field = fvField(fvt);
        auto& value = fvValue(fvt);

        auto shaStrings = swss::tokenize(value, ',');

        if (field == POLL_INTERVAL_FIELD)
        {
            setPollInterval(stoi(value));
        }
        else if (field == BULK_CHUNK_SIZE_FIELD)
        {
            if (value != "NULL")
            {
                try
                {
                    bulkChunkSize = stoi(value);
                }
                catch (...)
                {
                    SWSS_LOG_ERROR("Invalid bulk chunk size %s", value.c_str());
                }
            }
            for (auto &context : m_counterContext)
            {
                SWSS_LOG_NOTICE("Set counter context %s %s bulk size %u", m_instanceId.c_str(), COUNTER_TYPE_PORT.c_str(), bulkChunkSize);
                context.second->setBulkChunkSize(bulkChunkSize);
            }
        }
        else if (field == BULK_CHUNK_SIZE_PER_PREFIX_FIELD)
        {
            bulkChunkSizePerPrefix = value;
            for (auto &context : m_counterContext)
            {
                SWSS_LOG_NOTICE("Set counter context %s %s bulk chunk prefix map %s", m_instanceId.c_str(), COUNTER_TYPE_PORT.c_str(), bulkChunkSizePerPrefix.c_str());
                context.second->setBulkChunkSizePerPrefix(bulkChunkSizePerPrefix);
            }
        }
        else if (field == FLEX_COUNTER_STATUS_FIELD)
        {
            setStatus(value);
        }
        else if (field == STATS_MODE_FIELD)
        {
            setStatsMode(value);
        }
        else
        {
            auto counterTypeRef = m_plugIn2CounterType.find(field);

            if (counterTypeRef != m_plugIn2CounterType.end())
            {
                getCounterContext(counterTypeRef->second)->addPlugins(shaStrings);

                if (m_noDoubleCheckBulkCapability)
                {
                    getCounterContext(counterTypeRef->second)->setNoDoubleCheckBulkCapability(true);

                    SWSS_LOG_NOTICE("Do not double check bulk capability counter context %s %s", m_instanceId.c_str(), counterTypeRef->second.c_str());
                }

                if (bulkChunkSize > 0)
                {
                    getCounterContext(counterTypeRef->second)->setBulkChunkSize(bulkChunkSize);
                    SWSS_LOG_NOTICE("Create counter context %s %s with bulk size %u", m_instanceId.c_str(), counterTypeRef->second.c_str(), bulkChunkSize);
                }

                if (!bulkChunkSizePerPrefix.empty())
                {
                    getCounterContext(counterTypeRef->second)->setBulkChunkSizePerPrefix(bulkChunkSizePerPrefix);
                    SWSS_LOG_NOTICE("Create counter context %s %s with bulk prefix map %s", m_instanceId.c_str(), counterTypeRef->second.c_str(), bulkChunkSizePerPrefix.c_str());
                }
            }
            else
            {
                SWSS_LOG_ERROR("Field is not supported %s", field.c_str());
            }
        }
    }

    // notify thread to start polling
    notifyPoll();
}

bool FlexCounter::isEmpty()
{
    MUTEX;

    SWSS_LOG_ENTER();

    return allIdsEmpty() && allPluginsEmpty();
}

bool FlexCounter::isDiscarded()
{
    SWSS_LOG_ENTER();

    return isEmpty() && m_isDiscarded;
}

bool FlexCounter::allIdsEmpty() const
{
    SWSS_LOG_ENTER();

    for (auto &kv : m_counterContext)
    {
        if (kv.second->hasObject())
        {
            return false;
        }
    }

    return true;
}

bool FlexCounter::allPluginsEmpty() const
{
    SWSS_LOG_ENTER();

    for (auto &kv : m_counterContext)
    {
        if (kv.second->hasPlugin())
        {
            return false;
        }
    }

    return true;
}

std::shared_ptr<BaseCounterContext> FlexCounter::createCounterContext(
        _In_ const std::string& context_name,
        _In_ const std::string& instance)
{
    SWSS_LOG_ENTER();
    if (context_name == COUNTER_TYPE_PORT)
    {
        auto context = std::make_shared<CounterContext<sai_port_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_PORT, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_WRED_ECN_PORT)
    {
        auto context = std::make_shared<CounterContext<sai_port_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_PORT, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_PORT_DEBUG)
    {
        auto context = std::make_shared<CounterContext<sai_port_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_PORT, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->use_sai_stats_capa_query = false;
        context->use_sai_stats_ext = true;

        return context;
    }
    else if (context_name == COUNTER_TYPE_QUEUE)
    {
        auto context = std::make_shared<CounterContext<sai_queue_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_QUEUE, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->double_confirm_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_WRED_ECN_QUEUE)
    {
        auto context = std::make_shared<CounterContext<sai_queue_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_QUEUE, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->double_confirm_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_PG)
    {
        auto context = std::make_shared<CounterContext<sai_ingress_priority_group_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->double_confirm_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_RIF)
    {
        return std::make_shared<CounterContext<sai_router_interface_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == COUNTER_TYPE_SWITCH_DEBUG)
    {
        auto context = std::make_shared<CounterContext<sai_switch_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_SWITCH, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->use_sai_stats_capa_query = false;
        context->use_sai_stats_ext = true;

        return context;
    }
    else if (context_name == COUNTER_TYPE_MACSEC_FLOW)
    {
        auto context = std::make_shared<CounterContext<sai_macsec_flow_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_MACSEC_FLOW, m_vendorSai.get(), m_statsMode);
        context->use_sai_stats_capa_query = false;
        return context;
    }
    else if (context_name == COUNTER_TYPE_MACSEC_SA)
    {
        auto context = std::make_shared<CounterContext<sai_macsec_sa_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_MACSEC_SA, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->use_sai_stats_capa_query = false;
        context->dont_clear_support_counter = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_FLOW)
    {
        auto context = std::make_shared<CounterContext<sai_counter_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_COUNTER, m_vendorSai.get(), m_statsMode);
        context->use_sai_stats_capa_query = false;
        context->use_sai_stats_ext = true;

        return context;
    }
    else if (context_name == COUNTER_TYPE_TUNNEL)
    {
        auto context = std::make_shared<CounterContext<sai_tunnel_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_TUNNEL, m_vendorSai.get(), m_statsMode);
        context->use_sai_stats_capa_query = false;
        return context;
    }
    else if (context_name == COUNTER_TYPE_BUFFER_POOL)
    {
        auto context = std::make_shared<CounterContext<sai_buffer_pool_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_BUFFER_POOL, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_ENI)
    {
        auto context = std::make_shared<CounterContext<sai_eni_stat_t>>(context_name, instance, (sai_object_type_t)SAI_OBJECT_TYPE_ENI, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_METER_BUCKET)
    {
        return std::make_shared<DashMeterCounterContext>(context_name, instance, m_vendorSai.get(), m_dbCounters);
    }
    else if (context_name == ATTR_TYPE_QUEUE)
    {
        return std::make_shared<AttrContext<sai_queue_attr_t>>(context_name, instance, SAI_OBJECT_TYPE_QUEUE, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == ATTR_TYPE_PG)
    {
        return std::make_shared<AttrContext<sai_ingress_priority_group_attr_t>>(context_name, instance, SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == ATTR_TYPE_MACSEC_SA)
    {
        return std::make_shared<AttrContext<sai_macsec_sa_attr_t>>(context_name, instance, SAI_OBJECT_TYPE_MACSEC_SA, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == ATTR_TYPE_ACL_COUNTER)
    {
        return std::make_shared<AttrContext<sai_acl_counter_attr_t>>(context_name, instance, SAI_OBJECT_TYPE_ACL_COUNTER, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == COUNTER_TYPE_POLICER)
    {
        return std::make_shared<CounterContext<sai_policer_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_POLICER, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == COUNTER_TYPE_SRV6)
    {
        return std::make_shared<CounterContext<sai_counter_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_COUNTER, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == COUNTER_TYPE_SWITCH)
    {
        auto context = std::make_shared<CounterContext<sai_switch_stat_t>>(context_name, instance, SAI_OBJECT_TYPE_SWITCH, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->use_sai_stats_capa_query = true;
        return context;
    }

    SWSS_LOG_THROW("Invalid counter type %s", context_name.c_str());
    // GCC 8.3 requires a return value here
    return nullptr;
}

std::shared_ptr<BaseCounterContext> FlexCounter::getCounterContext(
        _In_ const std::string &name)
{
    SWSS_LOG_ENTER();

    auto iter = m_counterContext.find(name);
    if (iter != m_counterContext.end())
    {
        return iter->second;
    }

    auto counterContext = createCounterContext(name, m_instanceId);

    if (m_noDoubleCheckBulkCapability)
    {
        counterContext->setNoDoubleCheckBulkCapability(true);
        SWSS_LOG_NOTICE("Do not double check bulk capability counter context %s %s", m_instanceId.c_str(), name.c_str());
    }

    auto ret = m_counterContext.emplace(name, counterContext);
    return ret.first->second;
}

void FlexCounter::removeCounterContext(
        _In_ const std::string &name)
{
    SWSS_LOG_ENTER();

    auto iter = m_counterContext.find(name);
    if (iter != m_counterContext.end())
    {
        m_counterContext.erase(iter);
    }
    else
    {
        SWSS_LOG_ERROR("Try to remove non-exist counter context %s", name.c_str());
    }
}

bool FlexCounter::hasCounterContext(
    _In_ const std::string &name) const
{
    SWSS_LOG_ENTER();
    return m_counterContext.find(name) != m_counterContext.end();
}

void FlexCounter::collectCounters(
        _In_ swss::Table &countersTable)
{
    SWSS_LOG_ENTER();

    for (const auto &it : m_counterContext)
    {
        it.second->collectData(countersTable);
    }

    countersTable.flush();
}

void FlexCounter::runPlugins(
        _In_ swss::DBConnector& counters_db)
{
    SWSS_LOG_ENTER();

    const std::vector<std::string> argv =
    {
        std::to_string(counters_db.getDbId()),
        COUNTERS_TABLE,
        std::to_string(m_pollInterval)
    };

    for (const auto &it : m_counterContext)
    {
        it.second->runPlugin(counters_db, argv);
    }
}

void FlexCounter::flexCounterThreadRunFunction()
{
    SWSS_LOG_ENTER();

    swss::DBConnector db(m_dbCounters, 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, true);

    while (m_runFlexCounterThread)
    {
        MUTEX;

        if (m_enable && !allIdsEmpty() && (m_pollInterval > 0))
        {
            auto start = std::chrono::steady_clock::now();

            collectCounters(countersTable);

            runPlugins(db);

            auto finish = std::chrono::steady_clock::now();

            uint32_t delay = static_cast<uint32_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count());

            uint32_t correction = delay % m_pollInterval;
            correction = m_pollInterval - correction;
            MUTEX_UNLOCK; // explicit unlock

            SWSS_LOG_DEBUG("End of flex counter thread FC %s, took %d ms", m_instanceId.c_str(), delay);

            std::unique_lock<std::mutex> lk(m_mtxSleep);

            m_cvSleep.wait_for(lk, std::chrono::milliseconds(correction));

            continue;
        }

        MUTEX_UNLOCK; // explicit unlock

        // nothing to collect, wait until notified
        waitPoll();
    }
}

void FlexCounter::startFlexCounterThread()
{
    SWSS_LOG_ENTER();

    m_runFlexCounterThread = true;

    m_flexCounterThread = std::make_shared<std::thread>(&FlexCounter::flexCounterThreadRunFunction, this);

    SWSS_LOG_INFO("Flex Counter thread started");
}

void FlexCounter::endFlexCounterThread(void)
{
    SWSS_LOG_ENTER();

    MUTEX;

    if (!m_runFlexCounterThread)
    {
        return;
    }

    m_runFlexCounterThread = false;

    notifyPoll();

    m_cvSleep.notify_all();

    if (m_flexCounterThread != nullptr)
    {
        auto fcThread = std::move(m_flexCounterThread);

        MUTEX_UNLOCK; // NOTE: explicit unlock before join to not cause deadlock

        SWSS_LOG_INFO("Wait for Flex Counter thread to end");

        fcThread->join();
    }

    SWSS_LOG_INFO("Flex Counter thread ended");
}

void FlexCounter::removeCounter(
        _In_ sai_object_id_t vid)
{
    MUTEX;

    SWSS_LOG_ENTER();

    auto objectType = VidManager::objectTypeQuery(vid);

    if (objectType == SAI_OBJECT_TYPE_PORT)
    {
        if (hasCounterContext(COUNTER_TYPE_PORT))
        {
            getCounterContext(COUNTER_TYPE_PORT)->removeObject(vid);
        }
        if (hasCounterContext(COUNTER_TYPE_PORT_DEBUG))
        {
            getCounterContext(COUNTER_TYPE_PORT_DEBUG)->removeObject(vid);
        }
        if (hasCounterContext(COUNTER_TYPE_WRED_ECN_PORT))
        {
            getCounterContext(COUNTER_TYPE_WRED_ECN_PORT)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_QUEUE)
    {
        if (hasCounterContext(COUNTER_TYPE_QUEUE))
        {
            getCounterContext(COUNTER_TYPE_QUEUE)->removeObject(vid);
        }
        if (hasCounterContext(COUNTER_TYPE_WRED_ECN_QUEUE))
        {
            getCounterContext(COUNTER_TYPE_WRED_ECN_QUEUE)->removeObject(vid);
        }
        if (hasCounterContext(ATTR_TYPE_QUEUE))
        {
            getCounterContext(ATTR_TYPE_QUEUE)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP)
    {
        if (hasCounterContext(COUNTER_TYPE_PG))
        {
            getCounterContext(COUNTER_TYPE_PG)->removeObject(vid);
        }
        if (hasCounterContext(ATTR_TYPE_PG))
        {
            getCounterContext(ATTR_TYPE_PG)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_ROUTER_INTERFACE)
    {
        if (hasCounterContext(COUNTER_TYPE_RIF))
        {
            removeDataFromCountersDB(vid, ":RIF");
            getCounterContext(COUNTER_TYPE_RIF)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_BUFFER_POOL)
    {
        if (hasCounterContext(COUNTER_TYPE_BUFFER_POOL))
        {
            getCounterContext(COUNTER_TYPE_BUFFER_POOL)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        if (hasCounterContext(COUNTER_TYPE_SWITCH_DEBUG))
        {
            getCounterContext(COUNTER_TYPE_SWITCH_DEBUG)->removeObject(vid);
        }
        if (hasCounterContext(COUNTER_TYPE_SWITCH))
        {
            getCounterContext(COUNTER_TYPE_SWITCH)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_MACSEC_FLOW)
    {
        if (hasCounterContext(COUNTER_TYPE_MACSEC_FLOW))
        {
            getCounterContext(COUNTER_TYPE_MACSEC_FLOW)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        if (hasCounterContext(COUNTER_TYPE_MACSEC_SA))
        {
            getCounterContext(COUNTER_TYPE_MACSEC_SA)->removeObject(vid);
        }

        if (hasCounterContext(ATTR_TYPE_MACSEC_SA))
        {
            getCounterContext(ATTR_TYPE_MACSEC_SA)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_ACL_COUNTER)
    {
        if (hasCounterContext(ATTR_TYPE_ACL_COUNTER))
        {
            getCounterContext(ATTR_TYPE_ACL_COUNTER)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_TUNNEL)
    {
        if (hasCounterContext(COUNTER_TYPE_TUNNEL))
        {
            getCounterContext(COUNTER_TYPE_TUNNEL)->removeObject(vid);
        }
    }
    else if (objectType == (sai_object_type_t)SAI_OBJECT_TYPE_ENI)
    {
        if (hasCounterContext(COUNTER_TYPE_ENI))
        {
            getCounterContext(COUNTER_TYPE_ENI)->removeObject(vid);
        }
        if (hasCounterContext(COUNTER_TYPE_METER_BUCKET))
        {
            getCounterContext(COUNTER_TYPE_METER_BUCKET)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_COUNTER)
    {
        if (hasCounterContext(COUNTER_TYPE_FLOW))
        {
            getCounterContext(COUNTER_TYPE_FLOW)->removeObject(vid);
            removeDataFromCountersDB(vid, ":TRAP");
        }

        if (hasCounterContext(COUNTER_TYPE_SRV6))
        {
            getCounterContext(COUNTER_TYPE_SRV6)->removeObject(vid);
        }
    }
    else if (objectType == SAI_OBJECT_TYPE_POLICER)
    {
        if (hasCounterContext(COUNTER_TYPE_POLICER))
        {
            getCounterContext(COUNTER_TYPE_POLICER)->removeObject(vid);
        }
    }
    else
    {
        SWSS_LOG_ERROR("Object type for removal not supported, %s",
                sai_serialize_object_type(objectType).c_str());
    }
}

void FlexCounter::addCounter(
        _In_ sai_object_id_t vid,
        _In_ sai_object_id_t rid,
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    MUTEX;

    SWSS_LOG_ENTER();

    sai_object_type_t objectType = VidManager::objectTypeQuery(vid); // VID and RID will have the same object type

    std::vector<std::string> counterIds;

    std::string statsMode;

    for (const auto& valuePair: values)
    {
        const auto field = fvField(valuePair);
        const auto value = fvValue(valuePair);

        auto idStrings = swss::tokenize(value, ',');

        const auto &counterGroupRef = m_objectTypeField2CounterType.find({objectType, field});
        if (counterGroupRef != m_objectTypeField2CounterType.end())
        {
            getCounterContext(counterGroupRef->second)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_BUFFER_POOL && field == BUFFER_POOL_COUNTER_ID_LIST)
        {
            counterIds = idStrings;
        }
        else if (objectType == SAI_OBJECT_TYPE_BUFFER_POOL && field == STATS_MODE_FIELD)
        {
            statsMode = value;
        }
        else
        {
            SWSS_LOG_ERROR("Object type and field combination is not supported, object type %s, field %s",
                    sai_serialize_object_type(objectType).c_str(),
                    field.c_str());
        }
    }

    // outside loop since required 2 fields BUFFER_POOL_COUNTER_ID_LIST and STATS_MODE_FIELD

    if (objectType == SAI_OBJECT_TYPE_BUFFER_POOL && counterIds.size())
    {
        getCounterContext(COUNTER_TYPE_BUFFER_POOL)->addObject(
                vid,
                rid,
                counterIds,
                statsMode);
    }

    // notify thread to start polling
    notifyPoll();
}

void FlexCounter::bulkAddCounter(
        _In_ sai_object_type_t objectType,
        _In_ const std::vector<sai_object_id_t>& vids,
        _In_ const std::vector<sai_object_id_t>& rids,
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    MUTEX;

    SWSS_LOG_ENTER();

    std::vector<std::string> counterIds;

    std::string statsMode;

    for (const auto& valuePair: values)
    {
        const auto field = fvField(valuePair);
        const auto value = fvValue(valuePair);

        auto idStrings = swss::tokenize(value, ',');

        const auto &counterGroupRef = m_objectTypeField2CounterType.find({objectType, field});
        if (counterGroupRef != m_objectTypeField2CounterType.end())
        {
            getCounterContext(counterGroupRef->second)->bulkAddObject(
                    vids,
                    rids,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_BUFFER_POOL && field == BUFFER_POOL_COUNTER_ID_LIST)
        {
            counterIds = idStrings;
        }
        else if (objectType == SAI_OBJECT_TYPE_BUFFER_POOL && field == STATS_MODE_FIELD)
        {
            statsMode = value;
        }
        else
        {
            SWSS_LOG_ERROR("Object type and field combination is not supported, object type %s, field %s",
                    sai_serialize_object_type(objectType).c_str(),
                    field.c_str());
        }
    }

    // outside loop since required 2 fields BUFFER_POOL_COUNTER_ID_LIST and STATS_MODE_FIELD

    if (objectType == SAI_OBJECT_TYPE_BUFFER_POOL && counterIds.size())
    {
        getCounterContext(COUNTER_TYPE_BUFFER_POOL)->bulkAddObject(
                vids,
                rids,
                counterIds,
                statsMode);
    }

    // notify thread to start polling
    notifyPoll();
}

void FlexCounter::waitPoll()
{
    SWSS_LOG_ENTER();
    std::unique_lock<std::mutex> lk(m_mtxSleep);
    m_pollCond.wait(lk, [&](){return m_readyToPoll;});
    m_readyToPoll = false;
}

void FlexCounter::notifyPoll()
{
    SWSS_LOG_ENTER();
    std::unique_lock<std::mutex> lk(m_mtxSleep);
    m_readyToPoll = true;
    m_pollCond.notify_all();
}
