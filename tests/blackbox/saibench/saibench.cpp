/*
 * Load client of the syncd black-box benchmark, an orchagent stand-in.
 *
 * It talks to syncd through libsairedis, as orchagent does, and executes the
 * commands it reads on stdin, one per line, answering each with one line of
 * JSON on stdout. Times are CLOCK_MONOTONIC nanoseconds, the clock the mock
 * SAI uses too. See README.md for the commands.
 */

extern "C" {
#include "sai.h"
}

#include "sairedis.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>

static std::map<std::string, std::string> g_profile;

static sai_switch_api_t* g_switchApi = nullptr;
static sai_route_api_t* g_routeApi = nullptr;
static sai_next_hop_api_t* g_nextHopApi = nullptr;
static sai_next_hop_group_api_t* g_nextHopGroupApi = nullptr;
static sai_neighbor_api_t* g_neighborApi = nullptr;
static sai_router_interface_api_t* g_rifApi = nullptr;
static sai_fdb_api_t* g_fdbApi = nullptr;
static sai_port_api_t* g_portApi = nullptr;
static sai_bridge_api_t* g_bridgeApi = nullptr;

static sai_redis_communication_mode_t g_commMode = SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;

static sai_object_id_t g_switchId = SAI_NULL_OBJECT_ID;
static sai_object_id_t g_vr = SAI_NULL_OBJECT_ID;
static sai_object_id_t g_vlan = SAI_NULL_OBJECT_ID;

static std::vector<sai_object_id_t> g_ports;
static std::vector<sai_object_id_t> g_bridgePorts;

static std::vector<sai_object_id_t> g_rifs;
static std::vector<sai_object_id_t> g_nextHops;
static sai_object_id_t g_nextHopGroup = SAI_NULL_OBJECT_ID;

static std::vector<sai_object_id_t> g_extraNextHops;

static std::atomic<uint64_t> g_fdbEvents(0);
static std::atomic<uint64_t> g_fdbFirstNs(0);
static std::atomic<uint64_t> g_fdbLastNs(0);
static std::atomic<uint64_t> g_portEvents(0);
static std::atomic<uint64_t> g_portFirstNs(0);
static std::atomic<uint64_t> g_portLastNs(0);

static const sai_mac_t g_mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };

static uint64_t nowNs()
{
    SWSS_LOG_ENTER();

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static const char* profileGetValue(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == nullptr)
    {
        return nullptr;
    }

    auto it = g_profile.find(variable);

    return it == g_profile.end() ? nullptr : it->second.c_str();
}

static int profileGetNextValue(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    static auto it = g_profile.begin();

    if (value == nullptr)
    {
        it = g_profile.begin();

        return 0;
    }

    if (it == g_profile.end())
    {
        return -1;
    }

    *variable = it->first.c_str();
    *value = it->second.c_str();

    it++;

    return 0;
}

static const sai_service_method_table_t g_services = {
    profileGetValue,
    profileGetNextValue
};

static void noteEvent(
        _In_ std::atomic<uint64_t>& counter,
        _In_ std::atomic<uint64_t>& first,
        _In_ std::atomic<uint64_t>& last,
        _In_ uint32_t count)
{
    SWSS_LOG_ENTER();

    uint64_t now = nowNs();

    uint64_t zero = 0;

    first.compare_exchange_strong(zero, now);

    last = now;

    counter += count;
}

static void onFdbEvent(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    noteEvent(g_fdbEvents, g_fdbFirstNs, g_fdbLastNs, count);
}

static void onPortStateChange(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    noteEvent(g_portEvents, g_portFirstNs, g_portLastNs, count);
}

static void onShutdownRequest(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_WARN("syncd requested shutdown of switch %s", sai_serialize_object_id(switch_id).c_str());
}

/*
 * Arguments of a command: words after the command name, key=value or bare.
 */

class Args
{
    public:

        Args(
                _In_ const std::vector<std::string>& tokens)
        {
            SWSS_LOG_ENTER();

            for (size_t idx = 1; idx < tokens.size(); idx++)
            {
                auto pos = tokens[idx].find('=');

                if (pos == std::string::npos)
                {
                    m_values[tokens[idx]] = "1";
                }
                else
                {
                    m_values[tokens[idx].substr(0, pos)] = tokens[idx].substr(pos + 1);
                }
            }
        }

        std::string str(
                _In_ const std::string& key,
                _In_ const std::string& def) const
        {
            SWSS_LOG_ENTER();

            auto it = m_values.find(key);

            return it == m_values.end() ? def : it->second;
        }

        uint64_t num(
                _In_ const std::string& key,
                _In_ uint64_t def) const
        {
            SWSS_LOG_ENTER();

            auto it = m_values.find(key);

            return it == m_values.end() ? def : std::stoull(it->second);
        }

    private:

        std::map<std::string, std::string> m_values;
};

/*
 * A JSON object, built one field at a time.
 */

class Json
{
    public:

        Json& add(
                _In_ const std::string& key,
                _In_ const std::string& value)
        {
            SWSS_LOG_ENTER();

            std::string escaped;

            for (char c: value)
            {
                if (c == '"' || c == '\\')
                {
                    escaped += '\\';
                }

                escaped += ((unsigned char)c < 0x20) ? ' ' : c;
            }

            return raw(key, "\"" + escaped + "\"");
        }

        Json& add(
                _In_ const std::string& key,
                _In_ const char* value)
        {
            SWSS_LOG_ENTER();

            return add(key, std::string(value));
        }

        Json& add(
                _In_ const std::string& key,
                _In_ uint64_t value)
        {
            SWSS_LOG_ENTER();

            return raw(key, std::to_string(value));
        }

        Json& add(
                _In_ const std::string& key,
                _In_ double value)
        {
            SWSS_LOG_ENTER();

            std::ostringstream out;

            out.precision(9);

            out << value;

            return raw(key, out.str());
        }

        Json& add(
                _In_ const std::string& key,
                _In_ bool value)
        {
            SWSS_LOG_ENTER();

            return raw(key, value ? "true" : "false");
        }

        Json& raw(
                _In_ const std::string& key,
                _In_ const std::string& value)
        {
            SWSS_LOG_ENTER();

            m_body += (m_body.empty() ? "" : ",") + std::string("\"") + key + "\":" + value;

            return *this;
        }

        std::string str() const
        {
            SWSS_LOG_ENTER();

            return "{" + m_body + "}";
        }

    private:

        std::string m_body;
};

static std::string failure(
        _In_ const std::string& cmd,
        _In_ const std::string& message)
{
    SWSS_LOG_ENTER();

    return Json().add("ok", false).add("cmd", cmd).add("error", message).str();
}

/*
 * Per call latencies of a phase.
 */

class Latencies
{
    public:

        void add(
                _In_ uint64_t ns)
        {
            SWSS_LOG_ENTER();

            m_samples.push_back(ns);
        }

        std::string json()
        {
            SWSS_LOG_ENTER();

            if (m_samples.empty())
            {
                return "{}";
            }

            std::sort(m_samples.begin(), m_samples.end());

            uint64_t sum = 0;

            for (auto s: m_samples)
            {
                sum += s;
            }

            Json json;

            json.add("calls", (uint64_t)m_samples.size());
            json.add("mean_us", (double)sum / (double)m_samples.size() / 1000.0);
            json.add("p50_us", (double)at(0.50) / 1000.0);
            json.add("p90_us", (double)at(0.90) / 1000.0);
            json.add("p99_us", (double)at(0.99) / 1000.0);
            json.add("max_us", (double)m_samples.back() / 1000.0);

            return json.str();
        }

    private:

        uint64_t at(
                _In_ double quantile) const
        {
            SWSS_LOG_ENTER();

            size_t index = (size_t)(quantile * (double)(m_samples.size() - 1));

            return m_samples[index];
        }

        std::vector<uint64_t> m_samples;
};

/*
 * Result of a phase: what the client did and how long it took.
 */

struct Phase
{
    std::string cmd;

    std::string op;

    uint64_t objects = 0;

    uint64_t bulk = 0;

    uint64_t errors = 0;

    uint64_t startNs = 0;

    uint64_t endNs = 0;

    Latencies latencies;

    std::string json()
    {
        SWSS_LOG_ENTER();

        double wall = (double)(endNs - startNs) / 1e9;

        Json json;

        json.add("ok", errors == 0);
        json.add("cmd", cmd);
        json.add("op", op);
        json.add("objects", objects);
        json.add("bulk", bulk);
        json.add("errors", errors);
        json.add("start_ns", startNs);
        json.add("end_ns", endNs);
        json.add("wall_s", wall);
        json.add("objects_per_s", wall > 0 ? (double)objects / wall : 0.0);
        json.raw("latency", latencies.json());

        return json.str();
    }
};

static sai_status_t setRedisAttr(
        _In_ sai_attr_id_t id,
        _In_ int32_t value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = id;
    attr.value.s32 = value;

    return g_switchApi->set_switch_attribute(SAI_NULL_OBJECT_ID, &attr);
}

static sai_status_t setRedisBool(
        _In_ sai_attr_id_t id,
        _In_ bool value)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = id;
    attr.value.booldata = value;

    return g_switchApi->set_switch_attribute(SAI_NULL_OBJECT_ID, &attr);
}

/*
 * Waits for syncd to finish everything sent so far. In asynchronous mode the
 * calls did not wait; in synchronous modes syncd answers a call before it
 * records it in ASIC_DB. syncd executes requests in order, so the answer of a
 * get comes after all that.
 */

static sai_status_t syncPoint()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    return g_switchApi->get_switch_attribute(g_switchId, 1, &attr);
}

static sai_status_t getOidList(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t oid,
        _In_ sai_attr_id_t attrId,
        _Out_ std::vector<sai_object_id_t>& list)
{
    SWSS_LOG_ENTER();

    list.resize(4096);

    sai_attribute_t attr;

    attr.id = attrId;

    sai_status_t status = SAI_STATUS_FAILURE;

    // a longer list answers BUFFER_OVERFLOW with its count: get it again
    // with that much room

    for (int attempt = 0; attempt < 2; attempt++)
    {
        attr.value.objlist.count = (uint32_t)list.size();
        attr.value.objlist.list = list.data();

        switch (objectType)
        {
            case SAI_OBJECT_TYPE_SWITCH:
                status = g_switchApi->get_switch_attribute(oid, 1, &attr);
                break;

            case SAI_OBJECT_TYPE_BRIDGE:
                status = g_bridgeApi->get_bridge_attribute(oid, 1, &attr);
                break;

            case SAI_OBJECT_TYPE_PORT:
                status = g_portApi->get_port_attribute(oid, 1, &attr);
                break;

            default:
                status = SAI_STATUS_NOT_SUPPORTED;
                break;
        }

        if (status != SAI_STATUS_BUFFER_OVERFLOW)
        {
            break;
        }

        list.resize(attr.value.objlist.count);
    }

    list.resize(status == SAI_STATUS_SUCCESS ? attr.value.objlist.count : 0);

    return status;
}

static sai_status_t getSwitchOid(
        _In_ sai_attr_id_t attrId,
        _Out_ sai_object_id_t& oid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = attrId;

    sai_status_t status = g_switchApi->get_switch_attribute(g_switchId, 1, &attr);

    oid = status == SAI_STATUS_SUCCESS ? attr.value.oid : SAI_NULL_OBJECT_ID;

    return status;
}

static std::string cmdInit(
        _In_ const Args& args)
{
    SWSS_LOG_ENTER();

    if (g_switchId != SAI_NULL_OBJECT_ID)
    {
        return failure("init", "switch already created");
    }

    sai_deserialize_redis_communication_mode(args.str("comm", "redis_sync"), g_commMode);

    bool tempView = args.num("temp_view", 1) != 0;

    sai_status_t status = setRedisAttr(SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE, g_commMode);

    if (status == SAI_STATUS_SUCCESS)
    {
        status = setRedisBool(SAI_REDIS_SWITCH_ATTR_RECORD, args.num("record", 0) != 0);
    }

    if (status == SAI_STATUS_SUCCESS && g_commMode == SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC)
    {
        status = setRedisBool(SAI_REDIS_SWITCH_ATTR_USE_PIPELINE, true);
    }

    if (status == SAI_STATUS_SUCCESS && tempView)
    {
        status = setRedisAttr(SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD, SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW);
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        return failure("init", "setting sairedis attributes failed: " + sai_serialize_status(status));
    }

    std::vector<sai_attribute_t> attrs(5);

    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = true;
    attrs[1].id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attrs[1].value.ptr = (void*)&onFdbEvent;
    attrs[2].id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attrs[2].value.ptr = (void*)&onPortStateChange;
    attrs[3].id = SAI_SWITCH_ATTR_SWITCH_SHUTDOWN_REQUEST_NOTIFY;
    attrs[3].value.ptr = (void*)&onShutdownRequest;
    attrs[4].id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
    memcpy(attrs[4].value.mac, g_mac, sizeof(sai_mac_t));

    uint64_t t0 = nowNs();

    status = g_switchApi->create_switch(&g_switchId, (uint32_t)attrs.size(), attrs.data());

    uint64_t t1 = nowNs();

    if (status != SAI_STATUS_SUCCESS)
    {
        return failure("init", "create_switch failed: " + sai_serialize_status(status));
    }

    uint64_t t2 = t1;

    if (tempView && args.num("apply_view", 1))
    {
        status = setRedisAttr(SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD, SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW);

        t2 = nowNs();

        if (status != SAI_STATUS_SUCCESS)
        {
            return failure("init", "apply view failed: " + sai_serialize_status(status));
        }
    }

    status = getOidList(SAI_OBJECT_TYPE_SWITCH, g_switchId, SAI_SWITCH_ATTR_PORT_LIST, g_ports);

    sai_object_id_t bridge = SAI_NULL_OBJECT_ID;

    if (status == SAI_STATUS_SUCCESS)
    {
        status = getSwitchOid(SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID, g_vr);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        status = getSwitchOid(SAI_SWITCH_ATTR_DEFAULT_VLAN_ID, g_vlan);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        status = getSwitchOid(SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID, bridge);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        status = getOidList(SAI_OBJECT_TYPE_BRIDGE, bridge, SAI_BRIDGE_ATTR_PORT_LIST, g_bridgePorts);
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        return failure("init", "reading the switch failed: " + sai_serialize_status(status));
    }

    return Json()
        .add("ok", true)
        .add("cmd", "init")
        .add("comm", sai_serialize_redis_communication_mode(g_commMode))
        .add("switch", sai_serialize_object_id(g_switchId))
        .add("ports", (uint64_t)g_ports.size())
        .add("bridge_ports", (uint64_t)g_bridgePorts.size())
        .add("start_ns", t0)
        .add("create_switch_s", (double)(t1 - t0) / 1e9)
        .add("apply_view_s", (double)(t2 - t1) / 1e9)
        .str();
}

static sai_ip_address_t ip4(
        _In_ uint32_t hostOrder)
{
    SWSS_LOG_ENTER();

    sai_ip_address_t ip;

    memset(&ip, 0, sizeof(ip));

    ip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    ip.addr.ip4 = htonl(hostOrder);

    return ip;
}

/*
 * Destination of route number index: 100.0.0.0/24 onwards for IPv4,
 * fc00::/64 onwards for IPv6.
 */

static sai_route_entry_t routeEntry(
        _In_ uint64_t index,
        _In_ bool v6)
{
    SWSS_LOG_ENTER();

    sai_route_entry_t entry;

    memset(&entry, 0, sizeof(entry));

    entry.switch_id = g_switchId;
    entry.vr_id = g_vr;

    if (v6)
    {
        entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

        entry.destination.addr.ip6[0] = 0xfc;

        for (int byte = 0; byte < 6; byte++)
        {
            entry.destination.addr.ip6[7 - byte] = (uint8_t)(index >> (8 * byte));
        }

        memset(entry.destination.mask.ip6, 0xff, 8);
    }
    else
    {
        entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        entry.destination.addr.ip4 = htonl((uint32_t)((100ULL << 24) + (index << 8)));
        entry.destination.mask.ip4 = htonl(0xffffff00);
    }

    return entry;
}

static sai_object_id_t routeTarget(
        _In_ const std::string& target,
        _In_ uint64_t index)
{
    SWSS_LOG_ENTER();

    if (target == "nhg")
    {
        return g_nextHopGroup;
    }

    return g_nextHops[index % g_nextHops.size()];
}

static std::string cmdSetupL3(
        _In_ const Args& args)
{
    SWSS_LOG_ENTER();

    if (g_switchId == SAI_NULL_OBJECT_ID)
    {
        return failure("setup_l3", "no switch");
    }

    if (!g_rifs.empty())
    {
        return failure("setup_l3", "already set up");
    }

    size_t count = std::min<size_t>(args.num("ports", 4), g_ports.size());

    if (count == 0)
    {
        return failure("setup_l3", "no port");
    }

    uint64_t t0 = nowNs();

    for (size_t idx = 0; idx < count; idx++)
    {
        sai_attribute_t attrs[5];

        attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
        attrs[0].value.oid = g_vr;
        attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
        attrs[1].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;
        attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
        attrs[2].value.oid = g_ports[idx];
        attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
        memcpy(attrs[3].value.mac, g_mac, sizeof(sai_mac_t));
        attrs[4].id = SAI_ROUTER_INTERFACE_ATTR_MTU;
        attrs[4].value.u32 = 9100;

        sai_object_id_t rif;

        sai_status_t status = g_rifApi->create_router_interface(&rif, g_switchId, 5, attrs);

        if (status != SAI_STATUS_SUCCESS)
        {
            return failure("setup_l3", "create_router_interface failed: " + sai_serialize_status(status));
        }

        g_rifs.push_back(rif);

        sai_neighbor_entry_t neighbor;

        memset(&neighbor, 0, sizeof(neighbor));

        neighbor.switch_id = g_switchId;
        neighbor.rif_id = rif;
        neighbor.ip_address = ip4((10U << 24) | ((uint32_t)idx << 8) | 1);

        sai_attribute_t mac;

        mac.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
        memcpy(mac.value.mac, g_mac, sizeof(sai_mac_t));
        mac.value.mac[5] = (uint8_t)(0x80 + idx);

        status = g_neighborApi->create_neighbor_entry(&neighbor, 1, &mac);

        if (status != SAI_STATUS_SUCCESS)
        {
            return failure("setup_l3", "create_neighbor_entry failed: " + sai_serialize_status(status));
        }

        sai_attribute_t nh[3];

        nh[0].id = SAI_NEXT_HOP_ATTR_TYPE;
        nh[0].value.s32 = SAI_NEXT_HOP_TYPE_IP;
        nh[1].id = SAI_NEXT_HOP_ATTR_IP;
        nh[1].value.ipaddr = neighbor.ip_address;
        nh[2].id = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID;
        nh[2].value.oid = rif;

        sai_object_id_t nextHop;

        status = g_nextHopApi->create_next_hop(&nextHop, g_switchId, 3, nh);

        if (status != SAI_STATUS_SUCCESS)
        {
            return failure("setup_l3", "create_next_hop failed: " + sai_serialize_status(status));
        }

        g_nextHops.push_back(nextHop);
    }

    sai_attribute_t type;

    type.id = SAI_NEXT_HOP_GROUP_ATTR_TYPE;
    type.value.s32 = SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP;

    sai_status_t status = g_nextHopGroupApi->create_next_hop_group(&g_nextHopGroup, g_switchId, 1, &type);

    if (status != SAI_STATUS_SUCCESS)
    {
        return failure("setup_l3", "create_next_hop_group failed: " + sai_serialize_status(status));
    }

    for (auto nextHop: g_nextHops)
    {
        sai_attribute_t member[2];

        member[0].id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID;
        member[0].value.oid = g_nextHopGroup;
        member[1].id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID;
        member[1].value.oid = nextHop;

        sai_object_id_t memberId;

        status = g_nextHopGroupApi->create_next_hop_group_member(&memberId, g_switchId, 2, member);

        if (status != SAI_STATUS_SUCCESS)
        {
            return failure("setup_l3", "create_next_hop_group_member failed: " + sai_serialize_status(status));
        }
    }

    status = syncPoint();

    uint64_t t1 = nowNs();

    if (status != SAI_STATUS_SUCCESS)
    {
        return failure("setup_l3", "sync failed: " + sai_serialize_status(status));
    }

    return Json()
        .add("ok", true)
        .add("cmd", "setup_l3")
        .add("ports", (uint64_t)count)
        .add("start_ns", t0)
        .add("end_ns", t1)
        .add("wall_s", (double)(t1 - t0) / 1e9)
        .str();
}

/*
 * Executes count operations, single or in bulks of bulk objects: call(first,
 * n, statuses) does n of them from first on and returns the call's status.
 */

template <typename F>
static void runPhase(
        _Inout_ Phase& phase,
        _In_ uint64_t count,
        _In_ uint64_t bulk,
        _In_ F call)
{
    SWSS_LOG_ENTER();

    phase.objects = count;
    phase.bulk = bulk;

    std::vector<sai_status_t> statuses((size_t)std::max<uint64_t>(bulk, 1));

    phase.startNs = nowNs();

    for (uint64_t first = 0; first < count; first += std::max<uint64_t>(bulk, 1))
    {
        uint32_t n = (uint32_t)std::min<uint64_t>(std::max<uint64_t>(bulk, 1), count - first);

        uint64_t t0 = nowNs();

        sai_status_t status = call(first, n, statuses.data());

        phase.latencies.add(nowNs() - t0);

        if (bulk <= 1)
        {
            phase.errors += (status != SAI_STATUS_SUCCESS);

            continue;
        }

        for (uint32_t idx = 0; idx < n; idx++)
        {
            phase.errors += (statuses[idx] != SAI_STATUS_SUCCESS);
        }
    }

    if (syncPoint() != SAI_STATUS_SUCCESS)
    {
        phase.errors++;
    }

    phase.endNs = nowNs();
}

static std::string cmdRoutes(
        _In_ const Args& args)
{
    SWSS_LOG_ENTER();

    if (g_nextHops.empty())
    {
        return failure("routes", "run setup_l3 first");
    }

    std::string op = args.str("op", "create");
    std::string target = args.str("target", "nhg");

    uint64_t count = args.num("count", 1000);
    uint64_t bulk = args.num("bulk", 1);
    uint64_t start = args.num("start", 0);

    bool v6 = args.str("family", "v4") == "v6";

    Phase phase;

    phase.cmd = "routes";
    phase.op = op;

    std::vector<sai_route_entry_t> entries((size_t)std::max<uint64_t>(bulk, 1));
    std::vector<sai_attribute_t> attrs((size_t)std::max<uint64_t>(bulk, 1));
    std::vector<uint32_t> attrCounts((size_t)std::max<uint64_t>(bulk, 1), 1);
    std::vector<const sai_attribute_t*> attrLists((size_t)std::max<uint64_t>(bulk, 1));

    // set moves routes to the other kind of target

    std::string setTarget = target == "nhg" ? "nh" : "nhg";

    auto fill = [&](uint64_t first, uint32_t n) {
        for (uint32_t idx = 0; idx < n; idx++)
        {
            uint64_t index = start + first + idx;

            entries[idx] = routeEntry(index, v6);

            if (target == "drop" && op == "create")
            {
                attrs[idx].id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
                attrs[idx].value.s32 = SAI_PACKET_ACTION_DROP;
            }
            else
            {
                attrs[idx].id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
                attrs[idx].value.oid = routeTarget(op == "set" ? setTarget : target, index);
            }

            attrLists[idx] = &attrs[idx];
        }
    };

    if (op == "create")
    {
        runPhase(phase, count, bulk, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
            fill(first, n);

            if (bulk <= 1)
            {
                return g_routeApi->create_route_entry(&entries[0], 1, &attrs[0]);
            }

            return g_routeApi->create_route_entries(n, entries.data(), attrCounts.data(), attrLists.data(),
                    SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses);
        });
    }
    else if (op == "set")
    {
        runPhase(phase, count, bulk, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
            fill(first, n);

            if (bulk <= 1)
            {
                return g_routeApi->set_route_entry_attribute(&entries[0], &attrs[0]);
            }

            return g_routeApi->set_route_entries_attribute(n, entries.data(), attrs.data(),
                    SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses);
        });
    }
    else if (op == "remove")
    {
        runPhase(phase, count, bulk, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
            fill(first, n);

            if (bulk <= 1)
            {
                return g_routeApi->remove_route_entry(&entries[0]);
            }

            return g_routeApi->remove_route_entries(n, entries.data(),
                    SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses);
        });
    }
    else
    {
        return failure("routes", "unknown op " + op);
    }

    return phase.json();
}

static std::string cmdNeighbors(
        _In_ const Args& args)
{
    SWSS_LOG_ENTER();

    if (g_rifs.empty())
    {
        return failure("neighbors", "run setup_l3 first");
    }

    std::string op = args.str("op", "create");

    uint64_t count = args.num("count", 1000);
    uint64_t bulk = args.num("bulk", 1);

    Phase phase;

    phase.cmd = "neighbors";
    phase.op = op;

    size_t size = (size_t)std::max<uint64_t>(bulk, 1);

    std::vector<sai_neighbor_entry_t> entries(size);
    std::vector<sai_attribute_t> attrs(size);
    std::vector<uint32_t> attrCounts(size, 1);
    std::vector<const sai_attribute_t*> attrLists(size);

    auto fill = [&](uint64_t first, uint32_t n) {
        for (uint32_t idx = 0; idx < n; idx++)
        {
            uint64_t index = first + idx;

            memset(&entries[idx], 0, sizeof(entries[idx]));

            entries[idx].switch_id = g_switchId;
            entries[idx].rif_id = g_rifs[index % g_rifs.size()];
            entries[idx].ip_address = ip4((uint32_t)((11ULL << 24) + index + 1));

            attrs[idx].id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
            memcpy(attrs[idx].value.mac, g_mac, sizeof(sai_mac_t));
            attrs[idx].value.mac[3] = (uint8_t)(index >> 16);
            attrs[idx].value.mac[4] = (uint8_t)(index >> 8);
            attrs[idx].value.mac[5] = (uint8_t)index;

            attrLists[idx] = &attrs[idx];
        }
    };

    if (op == "create")
    {
        runPhase(phase, count, bulk, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
            fill(first, n);

            if (bulk <= 1)
            {
                return g_neighborApi->create_neighbor_entry(&entries[0], 1, &attrs[0]);
            }

            return g_neighborApi->create_neighbor_entries(n, entries.data(), attrCounts.data(), attrLists.data(),
                    SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses);
        });
    }
    else if (op == "remove")
    {
        runPhase(phase, count, bulk, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
            fill(first, n);

            if (bulk <= 1)
            {
                return g_neighborApi->remove_neighbor_entry(&entries[0]);
            }

            return g_neighborApi->remove_neighbor_entries(n, entries.data(),
                    SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses);
        });
    }
    else
    {
        return failure("neighbors", "unknown op " + op);
    }

    return phase.json();
}

static std::string cmdNextHops(
        _In_ const Args& args)
{
    SWSS_LOG_ENTER();

    if (g_rifs.empty())
    {
        return failure("nexthops", "run setup_l3 first");
    }

    std::string op = args.str("op", "create");

    uint64_t count = args.num("count", 1000);
    uint64_t bulk = args.num("bulk", 1);

    Phase phase;

    phase.cmd = "nexthops";
    phase.op = op;

    size_t size = (size_t)std::max<uint64_t>(bulk, 1);

    std::vector<sai_attribute_t> attrs(size * 3);
    std::vector<uint32_t> attrCounts(size, 3);
    std::vector<const sai_attribute_t*> attrLists(size);

    if (op == "create")
    {
        if (!g_extraNextHops.empty())
        {
            return failure("nexthops", "remove the next hops first");
        }

        g_extraNextHops.resize((size_t)count);

        runPhase(phase, count, bulk, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
            for (uint32_t idx = 0; idx < n; idx++)
            {
                uint64_t index = first + idx;

                sai_attribute_t* a = &attrs[(size_t)idx * 3];

                a[0].id = SAI_NEXT_HOP_ATTR_TYPE;
                a[0].value.s32 = SAI_NEXT_HOP_TYPE_IP;
                a[1].id = SAI_NEXT_HOP_ATTR_IP;
                a[1].value.ipaddr = ip4((uint32_t)((12ULL << 24) + index + 1));
                a[2].id = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID;
                a[2].value.oid = g_rifs[index % g_rifs.size()];

                attrLists[idx] = a;
            }

            if (bulk <= 1)
            {
                return g_nextHopApi->create_next_hop(&g_extraNextHops[first], g_switchId, 3, attrLists[0]);
            }

            return g_nextHopApi->create_next_hops(g_switchId, n, attrCounts.data(), attrLists.data(),
                    SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, &g_extraNextHops[first], statuses);
        });
    }
    else if (op == "remove")
    {
        count = std::min<uint64_t>(count, g_extraNextHops.size());

        runPhase(phase, count, bulk, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
            if (bulk <= 1)
            {
                return g_nextHopApi->remove_next_hop(g_extraNextHops[first]);
            }

            return g_nextHopApi->remove_next_hops(n, &g_extraNextHops[first],
                    SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses);
        });

        g_extraNextHops.erase(g_extraNextHops.begin(), g_extraNextHops.begin() + (ptrdiff_t)count);
    }
    else
    {
        return failure("nexthops", "unknown op " + op);
    }

    return phase.json();
}

static std::string cmdFdb(
        _In_ const Args& args)
{
    SWSS_LOG_ENTER();

    if (g_bridgePorts.empty() || g_vlan == SAI_NULL_OBJECT_ID)
    {
        return failure("fdb", "no bridge port or no default vlan");
    }

    std::string op = args.str("op", "create");

    uint64_t count = args.num("count", 1000);
    uint64_t bulk = args.num("bulk", 1);

    Phase phase;

    phase.cmd = "fdb";
    phase.op = op;

    size_t size = (size_t)std::max<uint64_t>(bulk, 1);

    std::vector<sai_fdb_entry_t> entries(size);
    std::vector<sai_attribute_t> attrs(size * 3);
    std::vector<uint32_t> attrCounts(size, 3);
    std::vector<const sai_attribute_t*> attrLists(size);

    auto fill = [&](uint64_t first, uint32_t n) {
        for (uint32_t idx = 0; idx < n; idx++)
        {
            uint64_t index = first + idx;

            memset(&entries[idx], 0, sizeof(entries[idx]));

            entries[idx].switch_id = g_switchId;
            entries[idx].bv_id = g_vlan;
            entries[idx].mac_address[0] = 0x06;
            entries[idx].mac_address[2] = (uint8_t)(index >> 24);
            entries[idx].mac_address[3] = (uint8_t)(index >> 16);
            entries[idx].mac_address[4] = (uint8_t)(index >> 8);
            entries[idx].mac_address[5] = (uint8_t)index;

            sai_attribute_t* a = &attrs[(size_t)idx * 3];

            a[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
            a[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
            a[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
            a[1].value.oid = g_bridgePorts[index % g_bridgePorts.size()];
            a[2].id = SAI_FDB_ENTRY_ATTR_PACKET_ACTION;
            a[2].value.s32 = SAI_PACKET_ACTION_FORWARD;

            attrLists[idx] = a;
        }
    };

    if (op == "create")
    {
        runPhase(phase, count, bulk, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
            fill(first, n);

            if (bulk <= 1)
            {
                return g_fdbApi->create_fdb_entry(&entries[0], 3, attrLists[0]);
            }

            return g_fdbApi->create_fdb_entries(n, entries.data(), attrCounts.data(), attrLists.data(),
                    SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses);
        });
    }
    else if (op == "remove")
    {
        runPhase(phase, count, bulk, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
            fill(first, n);

            if (bulk <= 1)
            {
                return g_fdbApi->remove_fdb_entry(&entries[0]);
            }

            return g_fdbApi->remove_fdb_entries(n, entries.data(),
                    SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses);
        });
    }
    else
    {
        return failure("fdb", "unknown op " + op);
    }

    return phase.json();
}

static void s8list(
        _Out_ sai_s8_list_t& list,
        _In_ const std::string& value)
{
    SWSS_LOG_ENTER();

    list.count = (uint32_t)value.size();
    list.list = (int8_t*)const_cast<char*>(value.c_str());
}

static std::string cmdFlexGroup(
        _In_ const Args& args)
{
    SWSS_LOG_ENTER();

    if (g_switchId == SAI_NULL_OBJECT_ID)
    {
        return failure("flex_group", "no switch");
    }

    std::string name = args.str("name", "PORT_STAT_COUNTER");
    std::string interval = args.str("interval_ms", "1000");
    std::string operation = args.str("op", "enable");
    std::string statsMode = args.str("stats_mode", "STATS_MODE_READ");
    std::string chunk = args.str("bulk_chunk_size", "");
    std::string empty;

    sai_redis_flex_counter_group_parameter_t param;

    s8list(param.counter_group_name, name);
    s8list(param.poll_interval, interval);
    s8list(param.operation, operation);
    s8list(param.stats_mode, statsMode);
    s8list(param.plugin_name, empty);
    s8list(param.plugins, empty);
    s8list(param.bulk_chunk_size, chunk);
    s8list(param.bulk_chunk_size_per_prefix, empty);

    sai_attribute_t attr;

    attr.id = SAI_REDIS_SWITCH_ATTR_FLEX_COUNTER_GROUP;
    attr.value.ptr = &param;

    uint64_t t0 = nowNs();

    sai_status_t status = g_switchApi->set_switch_attribute(g_switchId, &attr);

    uint64_t t1 = nowNs();

    if (status != SAI_STATUS_SUCCESS)
    {
        return failure("flex_group", "set failed: " + sai_serialize_status(status));
    }

    return Json()
        .add("ok", true)
        .add("cmd", "flex_group")
        .add("name", name)
        .add("wall_s", (double)(t1 - t0) / 1e9)
        .str();
}

static std::string cmdFlexCounters(
        _In_ const Args& args)
{
    SWSS_LOG_ENTER();

    if (g_switchId == SAI_NULL_OBJECT_ID)
    {
        return failure("flex_counters", "no switch");
    }

    std::string type = args.str("type", "port");
    std::string op = args.str("op", "add");

    sai_object_type_t objectType;
    sai_attr_id_t listAttr = 0;

    std::string group;
    std::string field;

    if (type == "port")
    {
        objectType = SAI_OBJECT_TYPE_PORT;
        group = "PORT_STAT_COUNTER";
        field = "PORT_COUNTER_ID_LIST";
    }
    else if (type == "queue")
    {
        objectType = SAI_OBJECT_TYPE_QUEUE;
        listAttr = SAI_PORT_ATTR_QOS_QUEUE_LIST;
        group = "QUEUE_STAT_COUNTER";
        field = "QUEUE_COUNTER_ID_LIST";
    }
    else if (type == "pg")
    {
        objectType = SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP;
        listAttr = SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST;
        group = "PG_WATERMARK_STAT_COUNTER";
        field = "PG_COUNTER_ID_LIST";
    }
    else
    {
        return failure("flex_counters", "unknown type " + type);
    }

    group = args.str("group", group);
    field = args.str("field", field);

    size_t ports = std::min<size_t>(args.num("ports", g_ports.size()), g_ports.size());

    std::vector<sai_object_id_t> objects;

    for (size_t idx = 0; idx < ports; idx++)
    {
        if (objectType == SAI_OBJECT_TYPE_PORT)
        {
            objects.push_back(g_ports[idx]);
            continue;
        }

        std::vector<sai_object_id_t> children;

        sai_status_t status = getOidList(SAI_OBJECT_TYPE_PORT, g_ports[idx], listAttr, children);

        if (status != SAI_STATUS_SUCCESS)
        {
            return failure("flex_counters", "reading port children failed: " + sai_serialize_status(status));
        }

        objects.insert(objects.end(), children.begin(), children.end());
    }

    // the first counters of the object type's stat enum, all by default

    auto info = sai_metadata_get_object_type_info(objectType);

    size_t counters = std::min<size_t>(args.num("counters", info->statenum->valuescount), info->statenum->valuescount);

    std::string ids;

    for (size_t idx = 0; idx < counters; idx++)
    {
        ids += (idx ? "," : "") + std::string(info->statenum->valuesnames[idx]);
    }

    std::string empty;

    Phase phase;

    phase.cmd = "flex_counters";
    phase.op = op;

    runPhase(phase, objects.size(), 1, [&](uint64_t first, uint32_t n, sai_status_t* statuses) {
        std::string key = group + ":" + sai_serialize_object_id(objects[first]);

        sai_redis_flex_counter_parameter_t param;

        s8list(param.counter_key, key);
        s8list(param.counter_ids, op == "add" ? ids : empty);
        s8list(param.counter_field_name, op == "add" ? field : empty);
        s8list(param.stats_mode, empty);

        sai_attribute_t attr;

        attr.id = SAI_REDIS_SWITCH_ATTR_FLEX_COUNTER;
        attr.value.ptr = &param;

        return g_switchApi->set_switch_attribute(g_switchId, &attr);
    });

    return Json()
        .add("ok", phase.errors == 0)
        .add("cmd", "flex_counters")
        .add("op", op)
        .add("type", type)
        .add("objects", (uint64_t)objects.size())
        .add("counters", (uint64_t)counters)
        .add("errors", phase.errors)
        .add("wall_s", (double)(phase.endNs - phase.startNs) / 1e9)
        .str();
}

static std::string cmdWaitNotifications(
        _In_ const Args& args)
{
    SWSS_LOG_ENTER();

    std::string type = args.str("type", "fdb");

    uint64_t count = args.num("count", 1);
    uint64_t timeoutMs = args.num("timeout_ms", 60000);

    std::atomic<uint64_t>* counter;
    std::atomic<uint64_t>* first;
    std::atomic<uint64_t>* last;

    if (type == "fdb")
    {
        counter = &g_fdbEvents;
        first = &g_fdbFirstNs;
        last = &g_fdbLastNs;
    }
    else if (type == "port_state")
    {
        counter = &g_portEvents;
        first = &g_portFirstNs;
        last = &g_portLastNs;
    }
    else
    {
        return failure("wait_notifications", "unknown type " + type);
    }

    uint64_t deadline = nowNs() + timeoutMs * 1000000ULL;

    while (counter->load() < count && nowNs() < deadline)
    {
        usleep(1000);
    }

    uint64_t received = counter->load();

    return Json()
        .add("ok", received >= count)
        .add("cmd", "wait_notifications")
        .add("type", type)
        .add("received", received)
        .add("first_ns", first->load())
        .add("last_ns", last->load())
        .str();
}

static std::string cmdResetNotifications()
{
    SWSS_LOG_ENTER();

    g_fdbEvents = 0;
    g_fdbFirstNs = 0;
    g_fdbLastNs = 0;
    g_portEvents = 0;
    g_portFirstNs = 0;
    g_portLastNs = 0;

    return Json().add("ok", true).add("cmd", "reset_notifications").str();
}

static std::string execute(
        _In_ const std::string& line)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> tokens;

    std::istringstream stream(line);

    std::string token;

    while (stream >> token)
    {
        tokens.push_back(token);
    }

    if (tokens.empty())
    {
        return failure("", "empty command");
    }

    const std::string& cmd = tokens[0];

    Args args(tokens);

    try
    {
        if (cmd == "init")
            return cmdInit(args);

        if (cmd == "setup_l3")
            return cmdSetupL3(args);

        if (cmd == "routes")
            return cmdRoutes(args);

        if (cmd == "neighbors")
            return cmdNeighbors(args);

        if (cmd == "nexthops")
            return cmdNextHops(args);

        if (cmd == "fdb")
            return cmdFdb(args);

        if (cmd == "flex_group")
            return cmdFlexGroup(args);

        if (cmd == "flex_counters")
            return cmdFlexCounters(args);

        if (cmd == "wait_notifications")
            return cmdWaitNotifications(args);

        if (cmd == "reset_notifications")
            return cmdResetNotifications();

        if (cmd == "ping")
            return Json().add("ok", true).add("cmd", "ping").add("now_ns", nowNs()).str();
    }
    catch (const std::exception& e)
    {
        return failure(cmd, e.what());
    }

    return failure(cmd, "unknown command");
}

static void usage()
{
    SWSS_LOG_ENTER();

    std::cerr << "Usage: saibench [-p KEY=VALUE]..." << std::endl;
    std::cerr << "    Reads commands on stdin, answers each with a line of JSON on stdout." << std::endl;
    std::cerr << "    -p KEY=VALUE    profile value for libsairedis, as SAI_REDIS_CONTEXT_CONFIG" << std::endl;
}

int main(
        _In_ int argc,
        _In_ char **argv)
{
    SWSS_LOG_ENTER();

    int opt;

    while ((opt = getopt(argc, argv, "p:h")) != -1)
    {
        switch (opt)
        {
            case 'p':
                {
                    std::string kv = optarg;

                    auto pos = kv.find('=');

                    if (pos == std::string::npos)
                    {
                        usage();
                        return 1;
                    }

                    g_profile[kv.substr(0, pos)] = kv.substr(pos + 1);
                }
                break;

            default:
                usage();
                return opt == 'h' ? 0 : 1;
        }
    }

    sai_status_t status = sai_api_initialize(0, &g_services);

    if (status != SAI_STATUS_SUCCESS)
    {
        std::cerr << "sai_api_initialize failed: " << sai_serialize_status(status) << std::endl;
        return 1;
    }

    sai_api_query(SAI_API_SWITCH, (void**)&g_switchApi);
    sai_api_query(SAI_API_ROUTE, (void**)&g_routeApi);
    sai_api_query(SAI_API_NEXT_HOP, (void**)&g_nextHopApi);
    sai_api_query(SAI_API_NEXT_HOP_GROUP, (void**)&g_nextHopGroupApi);
    sai_api_query(SAI_API_NEIGHBOR, (void**)&g_neighborApi);
    sai_api_query(SAI_API_ROUTER_INTERFACE, (void**)&g_rifApi);
    sai_api_query(SAI_API_FDB, (void**)&g_fdbApi);
    sai_api_query(SAI_API_PORT, (void**)&g_portApi);
    sai_api_query(SAI_API_BRIDGE, (void**)&g_bridgeApi);

    std::string line;

    while (std::getline(std::cin, line))
    {
        if (line == "quit")
        {
            break;
        }

        std::cout << execute(line) << std::endl;
    }

    sai_api_uninitialize();

    return 0;
}
