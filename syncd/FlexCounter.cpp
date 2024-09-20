#include "FlexCounter.h"
#include "VidManager.h"

#include "meta/sai_serialize.h"

#include "swss/redisapi.h"
#include "swss/tokenize.h"

#include <inttypes.h>
#include <vector>

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
static const std::string ATTR_TYPE_QUEUE = "Queue Attribute";
static const std::string ATTR_TYPE_PG = "Priority Group Attribute";
static const std::string ATTR_TYPE_MACSEC_SA = "MACSEC SA Attribute";
static const std::string ATTR_TYPE_ACL_COUNTER = "ACL Counter Attribute";

BaseCounterContext::BaseCounterContext(const std::string &name):
m_name(name)
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
public:
    typedef CounterIds<StatType> CounterIdsType;
    typedef BulkStatsContext<StatType> BulkContextType;

    CounterContext(
            _In_ const std::string &name,
            _In_ sai_object_type_t object_type,
            _In_ sairedis::SaiInterface *vendor_sai,
            _In_ sai_stats_mode_t &stats_mode):
    BaseCounterContext(name), m_objectType(object_type), m_vendorSai(vendor_sai), m_groupStatsMode(stats_mode)
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
            supportBulk = checkBulkCapability(vid, rid, supportedIds);
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
        else
        {
            std::sort(supportedIds.begin(), supportedIds.end());
            auto bulkContext = getBulkStatsContext(supportedIds);
            addBulkStatsContext(vid, rid, supportedIds, *bulkContext.get());
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
        else
        {
            if (!removeBulkStatsContext(vid) && log)
            {
                SWSS_LOG_NOTICE("Trying to remove nonexisting %s %s",
                    sai_serialize_object_type(m_objectType).c_str(),
                    sai_serialize_object_id(vid).c_str());
            }
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
            SWSS_LOG_WARN("Failed to bulk get stats for %s: %u", m_name.c_str(), status);
        }

        std::vector<swss::FieldValueTuple> values;
        for (size_t i = 0; i < ctx.object_keys.size(); i++)
        {
            if (SAI_STATUS_SUCCESS != ctx.object_statuses[i])
            {
                SWSS_LOG_ERROR("Failed to get stats of %s 0x%" PRIx64 ": %d", m_name.c_str(), ctx.object_keys[i].key.object_id, ctx.object_statuses[i]);
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
    }

    auto getBulkStatsContext(
        _In_ const std::vector<StatType>& counterIds)
    {
        SWSS_LOG_ENTER();
        auto iter = m_bulkContexts.find(counterIds);
        if (iter != m_bulkContexts.end())
        {
            return iter->second;
        }

        auto ret = m_bulkContexts.emplace(counterIds, std::make_shared<BulkContextType>());
        return ret.first->second;
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
        if (ctx.counter_ids.empty())
        {
            ctx.counter_ids = counterIds;
        }
        ctx.object_statuses.push_back(SAI_STATUS_SUCCESS);
        ctx.counters.resize(counterIds.size() * ctx.object_keys.size());
    }

    bool removeBulkStatsContext(
        _In_  sai_object_id_t vid)
    {
        SWSS_LOG_ENTER();
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
                m_bulkContexts.erase(iter);
            }
            else
            {
                auto key_iter = ctx.object_keys.begin();
                std::advance(key_iter, index);
                ctx.object_keys.erase(key_iter);
                ctx.counters.resize(ctx.counter_ids.size() * ctx.object_keys.size());
                ctx.object_statuses.pop_back();
            }
            break;
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

        if (always_check_supported_counters)
        {
            m_supportedCounters.clear();
        }

        if (!use_sai_stats_capa_query || querySupportedCounters(rid, stats_mode) != SAI_STATUS_SUCCESS)
        {
            /* Fallback to legacy approach */
            getSupportedCounters(rid, counter_ids, stats_mode);
        }
    }

    sai_status_t querySupportedCounters(
            _In_ sai_object_id_t rid,
            _In_ sai_stats_mode_t stats_mode)
    {
        SWSS_LOG_ENTER();
        sai_stat_capability_list_t stats_capability;
        stats_capability.count = 0;
        stats_capability.list = nullptr;

        /* First call is to check the size needed to allocate */
        sai_status_t status = m_vendorSai->queryStatsCapability(
            rid,
            m_objectType,
            &stats_capability);

        /* Second call is for query statistics capability */
        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            std::vector<sai_stat_capability_t> statCapabilityList(stats_capability.count);
            stats_capability.list = statCapabilityList.data();
            status = m_vendorSai->queryStatsCapability(
                rid,
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
                    m_supportedCounters.insert(counter);
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
            _In_ sai_object_type_t object_type,
            _In_ sairedis::SaiInterface *vendor_sai,
            _In_ sai_stats_mode_t &stats_mode):
    CounterContext<AttrType>(name, object_type, vendor_sai, stats_mode)
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

FlexCounter::FlexCounter(
        _In_ const std::string& instanceId,
        _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
        _In_ const std::string& dbCounters):
    m_readyToPoll(false),
    m_pollInterval(0),
    m_instanceId(instanceId),
    m_vendorSai(vendorSai),
    m_dbCounters(dbCounters)
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

    m_pollInterval = pollInterval;
}

void FlexCounter::setStatus(
        _In_ const std::string& status)
{
    SWSS_LOG_ENTER();

    if (status == "enable")
    {
        m_enable = true;
    }
    else if (status == "disable")
    {
        m_enable = false;
    }
    else
    {
        SWSS_LOG_WARN("Input value %s is not supported for Flex counter status, enter enable or disable", status.c_str());
    }
}

void FlexCounter::setStatsMode(
        _In_ const std::string& mode)
{
    SWSS_LOG_ENTER();

    if (mode == STATS_MODE_READ)
    {
        m_statsMode = SAI_STATS_MODE_READ;

        SWSS_LOG_DEBUG("Set STATS MODE %s for FC %s", mode.c_str(), m_instanceId.c_str());
    }
    else if (mode == STATS_MODE_READ_AND_CLEAR)
    {
        m_statsMode = SAI_STATS_MODE_READ_AND_CLEAR;

        SWSS_LOG_DEBUG("Set STATS MODE %s for FC %s", mode.c_str(), m_instanceId.c_str());
    }
    else
    {
        SWSS_LOG_WARN("Input value %s is not supported for Flex counter stats mode, enter STATS_MODE_READ or STATS_MODE_READ_AND_CLEAR", mode.c_str());
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

    for (auto& fvt: values)
    {
        auto& field = fvField(fvt);
        auto& value = fvValue(fvt);

        auto shaStrings = swss::tokenize(value, ',');

        if (field == POLL_INTERVAL_FIELD)
        {
            setPollInterval(stoi(value));
        }
        else if (field == FLEX_COUNTER_STATUS_FIELD)
        {
            setStatus(value);
        }
        else if (field == STATS_MODE_FIELD)
        {
            setStatsMode(value);
        }
        else if (field == QUEUE_PLUGIN_FIELD)
        {
            getCounterContext(COUNTER_TYPE_QUEUE)->addPlugins(shaStrings);
        }
        else if (field == PG_PLUGIN_FIELD)
        {
            getCounterContext(COUNTER_TYPE_PG)->addPlugins(shaStrings);
        }
        else if (field == PORT_PLUGIN_FIELD)
        {
            getCounterContext(COUNTER_TYPE_PORT)->addPlugins(shaStrings);
        }
        else if (field == RIF_PLUGIN_FIELD)
        {
            getCounterContext(COUNTER_TYPE_RIF)->addPlugins(shaStrings);
        }
        else if (field == BUFFER_POOL_PLUGIN_FIELD)
        {
            getCounterContext(COUNTER_TYPE_BUFFER_POOL)->addPlugins(shaStrings);
        }
        else if (field == TUNNEL_PLUGIN_FIELD)
        {
            getCounterContext(COUNTER_TYPE_TUNNEL)->addPlugins(shaStrings);
        }
        else if (field == FLOW_COUNTER_PLUGIN_FIELD)
        {
            getCounterContext(COUNTER_TYPE_FLOW)->addPlugins(shaStrings);
        }
        else
        {
            SWSS_LOG_ERROR("Field is not supported %s", field.c_str());
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
        _In_ const std::string& context_name)
{
    SWSS_LOG_ENTER();
    if (context_name == COUNTER_TYPE_PORT)
    {
        auto context = std::make_shared<CounterContext<sai_port_stat_t>>(context_name, SAI_OBJECT_TYPE_PORT, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_PORT_DEBUG)
    {
        auto context = std::make_shared<CounterContext<sai_port_stat_t>>(context_name, SAI_OBJECT_TYPE_PORT, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->use_sai_stats_capa_query = false;
        context->use_sai_stats_ext = true;

        return context;
    }
    else if (context_name == COUNTER_TYPE_QUEUE)
    {
        auto context = std::make_shared<CounterContext<sai_queue_stat_t>>(context_name, SAI_OBJECT_TYPE_QUEUE, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->double_confirm_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_PG)
    {
        auto context = std::make_shared<CounterContext<sai_ingress_priority_group_stat_t>>(context_name, SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->double_confirm_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_RIF)
    {
        return std::make_shared<CounterContext<sai_router_interface_stat_t>>(context_name, SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == COUNTER_TYPE_SWITCH_DEBUG)
    {
        auto context = std::make_shared<CounterContext<sai_switch_stat_t>>(context_name, SAI_OBJECT_TYPE_SWITCH, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        context->use_sai_stats_capa_query = false;
        context->use_sai_stats_ext = true;

        return context;
    }
    else if (context_name == COUNTER_TYPE_MACSEC_FLOW)
    {
        auto context = std::make_shared<CounterContext<sai_macsec_flow_stat_t>>(context_name, SAI_OBJECT_TYPE_MACSEC_FLOW, m_vendorSai.get(), m_statsMode);
        context->use_sai_stats_capa_query = false;
        return context;
    }
    else if (context_name == COUNTER_TYPE_MACSEC_SA)
    {
        auto context = std::make_shared<CounterContext<sai_macsec_sa_stat_t>>(context_name, SAI_OBJECT_TYPE_MACSEC_SA, m_vendorSai.get(), m_statsMode);
        context->use_sai_stats_capa_query = false;
        return context;
    }
    else if (context_name == COUNTER_TYPE_FLOW)
    {
        auto context = std::make_shared<CounterContext<sai_counter_stat_t>>(context_name, SAI_OBJECT_TYPE_COUNTER, m_vendorSai.get(), m_statsMode);
        context->use_sai_stats_capa_query = false;
        context->use_sai_stats_ext = true;

        return context;
    }
    else if (context_name == COUNTER_TYPE_TUNNEL)
    {
        auto context = std::make_shared<CounterContext<sai_tunnel_stat_t>>(context_name, SAI_OBJECT_TYPE_TUNNEL, m_vendorSai.get(), m_statsMode);
        context->use_sai_stats_capa_query = false;
        return context;
    }
    else if (context_name == COUNTER_TYPE_BUFFER_POOL)
    {
        auto context = std::make_shared<CounterContext<sai_buffer_pool_stat_t>>(context_name, SAI_OBJECT_TYPE_BUFFER_POOL, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        return context;
    }
    else if (context_name == COUNTER_TYPE_ENI)
    {
        auto context = std::make_shared<CounterContext<sai_eni_stat_t>>(context_name, (sai_object_type_t)SAI_OBJECT_TYPE_ENI, m_vendorSai.get(), m_statsMode);
        context->always_check_supported_counters = true;
        return context;
    }
    else if (context_name == ATTR_TYPE_QUEUE)
    {
        return std::make_shared<AttrContext<sai_queue_attr_t>>(context_name, SAI_OBJECT_TYPE_QUEUE, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == ATTR_TYPE_PG)
    {
        return std::make_shared<AttrContext<sai_ingress_priority_group_attr_t>>(context_name, SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == ATTR_TYPE_MACSEC_SA)
    {
        return std::make_shared<AttrContext<sai_macsec_sa_attr_t>>(context_name, SAI_OBJECT_TYPE_MACSEC_SA, m_vendorSai.get(), m_statsMode);
    }
    else if (context_name == ATTR_TYPE_ACL_COUNTER)
    {
        return std::make_shared<AttrContext<sai_acl_counter_attr_t>>(context_name, SAI_OBJECT_TYPE_ACL_COUNTER, m_vendorSai.get(), m_statsMode);
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

    auto ret = m_counterContext.emplace(name, createCounterContext(name));
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
    }
    else if (objectType == SAI_OBJECT_TYPE_QUEUE)
    {
        if (hasCounterContext(COUNTER_TYPE_QUEUE))
        {
            getCounterContext(COUNTER_TYPE_QUEUE)->removeObject(vid);
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
    }
    else if (objectType == SAI_OBJECT_TYPE_COUNTER)
    {
        if (hasCounterContext(COUNTER_TYPE_FLOW))
        {
            getCounterContext(COUNTER_TYPE_FLOW)->removeObject(vid);
            removeDataFromCountersDB(vid, ":TRAP");
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

        if (objectType == SAI_OBJECT_TYPE_PORT && field == PORT_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_PORT)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_PORT && field == PORT_DEBUG_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_PORT_DEBUG)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_QUEUE && field == QUEUE_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_QUEUE)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");

        }
        else if (objectType == SAI_OBJECT_TYPE_QUEUE && field == QUEUE_ATTR_ID_LIST)
        {
            getCounterContext(ATTR_TYPE_QUEUE)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP && field == PG_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_PG)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP && field == PG_ATTR_ID_LIST)
        {
            getCounterContext(ATTR_TYPE_PG)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_ROUTER_INTERFACE && field == RIF_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_RIF)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_SWITCH && field == SWITCH_DEBUG_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_SWITCH_DEBUG)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_MACSEC_FLOW && field == MACSEC_FLOW_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_MACSEC_FLOW)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_MACSEC_SA && field == MACSEC_SA_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_MACSEC_SA)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_MACSEC_SA && field == MACSEC_SA_ATTR_ID_LIST)
        {
            getCounterContext(ATTR_TYPE_MACSEC_SA)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_ACL_COUNTER && field == ACL_COUNTER_ATTR_ID_LIST)
        {
            getCounterContext(ATTR_TYPE_ACL_COUNTER)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == SAI_OBJECT_TYPE_COUNTER && field == FLOW_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_FLOW)->addObject(
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
        else if (objectType == SAI_OBJECT_TYPE_TUNNEL && field == TUNNEL_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_TUNNEL)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
        }
        else if (objectType == (sai_object_type_t)SAI_OBJECT_TYPE_ENI && field == ENI_COUNTER_ID_LIST)
        {
            getCounterContext(COUNTER_TYPE_ENI)->addObject(
                    vid,
                    rid,
                    idStrings,
                    "");
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
