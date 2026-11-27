#include "MockSai.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <sstream>

#include <errno.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MUTEX() std::lock_guard<std::mutex> _lock(m_mutex)

using namespace saimock;

static std::vector<std::string> tokenize(
        _In_ const std::string& line,
        _In_ char separator)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> tokens;

    std::string token;

    std::istringstream stream(line);

    while (std::getline(stream, token, separator))
    {
        if (!token.empty())
        {
            tokens.push_back(token);
        }
    }

    return tokens;
}

static bool parseBool(
        _In_ const std::string& value)
{
    SWSS_LOG_ENTER();

    return value == "1" || value == "true" || value == "yes" || value == "on";
}

static std::string jsonString(
        _In_ const std::string& value)
{
    SWSS_LOG_ENTER();

    std::string out = "\"";

    for (char c: value)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
        }
        else if ((unsigned char)c < 0x20)
        {
            out += ' ';
        }
        else
        {
            out += c;
        }
    }

    return out + "\"";
}

static std::string jsonError(
        _In_ const std::string& message)
{
    SWSS_LOG_ENTER();

    return "{\"ok\":false,\"error\":" + jsonString(message) + "}";
}

static bool parseObjectType(
        _In_ const std::string& name,
        _Out_ int32_t& objectType)
{
    SWSS_LOG_ENTER();

    if (name == "*")
    {
        objectType = -1;

        return true;
    }

    std::string full = name.rfind("SAI_OBJECT_TYPE_", 0) == 0 ? name : "SAI_OBJECT_TYPE_" + name;

    for (size_t idx = 0; idx < sai_metadata_enum_sai_object_type_t.valuescount; idx++)
    {
        if (full == sai_metadata_enum_sai_object_type_t.valuesnames[idx])
        {
            objectType = sai_metadata_enum_sai_object_type_t.values[idx];

            return true;
        }
    }

    return false;
}

/*
 * Key=value arguments of a control command, after its fixed arguments.
 */

static std::map<std::string, std::string> keyValues(
        _In_ const std::vector<std::string>& args,
        _In_ size_t start)
{
    SWSS_LOG_ENTER();

    std::map<std::string, std::string> values;

    for (size_t idx = start; idx < args.size(); idx++)
    {
        auto pos = args[idx].find('=');

        if (pos == std::string::npos)
        {
            values[args[idx]] = "";
        }
        else
        {
            values[args[idx].substr(0, pos)] = args[idx].substr(pos + 1);
        }
    }

    return values;
}

void MockSai::loadConfig(
        _In_ const sai_service_method_table_t *smt)
{
    SWSS_LOG_ENTER();

    // environment wins over the profile syncd was given

    auto value = [smt](const char* key) -> std::string {
        const char* env = getenv(key);

        if (env)
        {
            return env;
        }

        if (smt && smt->profile_get_value)
        {
            const char* profile = smt->profile_get_value(0, key);

            if (profile)
            {
                return profile;
            }
        }

        return "";
    };

    auto u32 = [&value](const char* key, uint32_t& field) {
        auto v = value(key);

        if (!v.empty())
        {
            field = (uint32_t)std::stoul(v);
        }
    };

    auto boolean = [&value](const char* key, bool& field) {
        auto v = value(key);

        if (!v.empty())
        {
            field = parseBool(v);
        }
    };

    auto str = [&value](const char* key, std::string& field) {
        auto v = value(key);

        if (!v.empty())
        {
            field = v;
        }
    };

    try
    {
        str("SAI_MOCK_CONTROL_SOCKET", m_config.controlSocket);
        u32("SAI_MOCK_PORT_COUNT", m_config.portCount);
        u32("SAI_MOCK_LANES_PER_PORT", m_config.lanesPerPort);
        u32("SAI_MOCK_PORT_SPEED", m_config.portSpeed);
        u32("SAI_MOCK_QUEUES_PER_PORT", m_config.queuesPerPort);
        u32("SAI_MOCK_CPU_QUEUES", m_config.cpuQueues);
        u32("SAI_MOCK_PGS_PER_PORT", m_config.pgsPerPort);
        boolean("SAI_MOCK_DEFAULT_BRIDGE_PORTS", m_config.defaultBridgePorts);
        boolean("SAI_MOCK_DEFAULT_VLAN_MEMBERS", m_config.defaultVlanMembers);
        boolean("SAI_MOCK_PORTS_UP", m_config.portsUp);
        boolean("SAI_MOCK_STORE_ATTRIBUTES", m_config.storeAttributes);
        boolean("SAI_MOCK_BULK", m_config.bulkSupported);
        boolean("SAI_MOCK_BULK_STATS", m_config.bulkStatsSupported);
        str("SAI_MOCK_SRC_MAC", m_config.srcMac);
        str("SAI_MOCK_DELAYS", m_config.delays);
    }
    catch (const std::exception& e)
    {
        SWSS_LOG_ERROR("invalid mock configuration: %s", e.what());
    }

    m_config.lanesPerPort = std::max<uint32_t>(1, m_config.lanesPerPort);
    m_config.queuesPerPort = std::min<uint32_t>(std::max<uint32_t>(2, m_config.queuesPerPort), 254);
    m_config.cpuQueues = std::min<uint32_t>(std::max<uint32_t>(1, m_config.cpuQueues), 255);
    m_config.pgsPerPort = std::min<uint32_t>(m_config.pgsPerPort, 255);

    // delays: "op:type:per_call_ns[:per_object_ns[:sleep]]" separated by ';'

    for (const auto& spec: tokenize(m_config.delays, ';'))
    {
        auto parts = tokenize(spec, ':');

        parts.insert(parts.begin(), "delay");

        auto result = setDelay(parts);

        if (result.find("\"ok\":true") == std::string::npos)
        {
            SWSS_LOG_ERROR("invalid delay %s: %s", spec.c_str(), result.c_str());
        }
    }

    SWSS_LOG_NOTICE("saimock: %u ports x %u lanes, %u queues, %u priority groups, store attributes %d, bulk %d, bulk stats %d, control %s",
            m_config.portCount,
            m_config.lanesPerPort,
            m_config.queuesPerPort,
            m_config.pgsPerPort,
            m_config.storeAttributes,
            m_config.bulkSupported,
            m_config.bulkStatsSupported,
            m_config.controlSocket.c_str());
}

std::string MockSai::setDelay(
        _In_ const std::vector<std::string>& args)
{
    SWSS_LOG_ENTER();

    // delay <op|*> <type|*> <per_call_ns> [per_object_ns] [sleep|spin]
    // delay clear

    if (args.size() == 2 && args[1] == "clear")
    {
        m_delays.clear();
        m_hasDelays = false;

        return "{\"ok\":true}";
    }

    if (args.size() < 4)
    {
        return jsonError("usage: delay <op|*> <object type|*> <per call ns> [per object ns] [sleep|spin]");
    }

    Op op;

    if (!opFromName(args[1], op))
    {
        return jsonError("unknown op " + args[1]);
    }

    int32_t objectType;

    if (!parseObjectType(args[2], objectType))
    {
        return jsonError("unknown object type " + args[2]);
    }

    Delay delay;

    try
    {
        delay.perCallNs = std::stoull(args[3]);
        delay.perObjectNs = args.size() > 4 ? std::stoull(args[4]) : 0;
    }
    catch (const std::exception& e)
    {
        return jsonError(std::string("invalid delay: ") + e.what());
    }

    delay.sleep = args.size() > 5 && args[5] == "sleep";

    auto key = std::make_pair(op, objectType);

    if (delay.perCallNs == 0 && delay.perObjectNs == 0)
    {
        m_delays.erase(key);
    }
    else
    {
        m_delays[key] = delay;
    }

    m_hasDelays = !m_delays.empty();

    return "{\"ok\":true}";
}

std::string MockSai::statsJson(
        _In_ bool reset)
{
    SWSS_LOG_ENTER();

    std::ostringstream out;

    out << "{\"ok\":true,\"now_ns\":" << nowNs() << ",\"since_ns\":" << m_statsSinceNs << ",\"stats\":[";

    bool first = true;

    for (const auto& it: m_stats)
    {
        const auto& stat = it.second;

        std::string type = it.first.second < 0 ? "*" :
            sai_serialize_object_type((sai_object_type_t)it.first.second);

        out << (first ? "" : ",")
            << "{\"op\":\"" << opName(it.first.first) << "\""
            << ",\"object_type\":" << jsonString(type)
            << ",\"calls\":" << stat.calls
            << ",\"objects\":" << stat.objects
            << ",\"errors\":" << stat.errors
            << ",\"unsupported\":" << stat.unsupported
            << ",\"sdk_ns\":" << stat.sdkNs
            << ",\"delay_ns\":" << stat.delayNs
            << ",\"first_ns\":" << stat.firstNs
            << ",\"last_ns\":" << stat.lastNs
            << ",\"max_objects\":" << stat.maxObjects
            << "}";

        first = false;
    }

    out << "],\"objects\":" << objectsJson() << "}";

    if (reset)
    {
        m_stats.clear();
        m_statsSinceNs = nowNs();
    }

    return out.str();
}

std::string MockSai::objectsJson()
{
    SWSS_LOG_ENTER();

    std::ostringstream out;

    out << "{";

    bool first = true;

    for (const auto& it: m_counts)
    {
        if (it.second == 0)
        {
            continue;
        }

        out << (first ? "" : ",")
            << jsonString(sai_serialize_object_type((sai_object_type_t)it.first))
            << ":" << it.second;

        first = false;
    }

    out << "}";

    return out.str();
}

std::string MockSai::configJson()
{
    SWSS_LOG_ENTER();

    std::ostringstream out;

    out << "{\"ok\":true"
        << ",\"pid\":" << getpid()
        << ",\"control_socket\":" << jsonString(m_config.controlSocket)
        << ",\"port_count\":" << m_config.portCount
        << ",\"lanes_per_port\":" << m_config.lanesPerPort
        << ",\"port_speed\":" << m_config.portSpeed
        << ",\"queues_per_port\":" << m_config.queuesPerPort
        << ",\"cpu_queues\":" << m_config.cpuQueues
        << ",\"pgs_per_port\":" << m_config.pgsPerPort
        << ",\"default_bridge_ports\":" << (m_config.defaultBridgePorts ? "true" : "false")
        << ",\"default_vlan_members\":" << (m_config.defaultVlanMembers ? "true" : "false")
        << ",\"ports_up\":" << (m_config.portsUp ? "true" : "false")
        << ",\"store_attributes\":" << (m_config.storeAttributes ? "true" : "false")
        << ",\"bulk\":" << (m_config.bulkSupported ? "true" : "false")
        << ",\"bulk_stats\":" << (m_config.bulkStatsSupported ? "true" : "false")
        << ",\"switches\":" << m_switches.size()
        << ",\"delays\":[";

    bool first = true;

    for (const auto& it: m_delays)
    {
        std::string type = it.first.second < 0 ? "*" :
            sai_serialize_object_type((sai_object_type_t)it.first.second);

        out << (first ? "" : ",")
            << "{\"op\":\"" << opName(it.first.first) << "\""
            << ",\"object_type\":" << jsonString(type)
            << ",\"per_call_ns\":" << it.second.perCallNs
            << ",\"per_object_ns\":" << it.second.perObjectNs
            << ",\"sleep\":" << (it.second.sleep ? "true" : "false")
            << "}";

        first = false;
    }

    out << "]}";

    return out.str();
}

std::string MockSai::control(
        _In_ const std::string& line)
{
    SWSS_LOG_ENTER();

    auto args = tokenize(line, ' ');

    if (args.empty())
    {
        return jsonError("empty command");
    }

    const auto& cmd = args[0];

    if (cmd == "ping")
    {
        return "{\"ok\":true,\"pid\":" + std::to_string(getpid()) + "}";
    }

    if (cmd == "notify")
    {
        return notify(args);
    }

    MUTEX();

    if (cmd == "stats")
    {
        return statsJson(args.size() > 1 && args[1] == "reset");
    }

    if (cmd == "reset")
    {
        m_stats.clear();
        m_statsSinceNs = nowNs();

        return "{\"ok\":true,\"since_ns\":" + std::to_string(m_statsSinceNs) + "}";
    }

    if (cmd == "objects")
    {
        return "{\"ok\":true,\"objects\":" + objectsJson() + "}";
    }

    if (cmd == "config")
    {
        return configJson();
    }

    if (cmd == "delay")
    {
        return setDelay(args);
    }

    return jsonError("unknown command " + cmd);
}

std::string MockSai::notify(
        _In_ const std::vector<std::string>& args)
{
    SWSS_LOG_ENTER();

    // notify fdb count=N [batch=B] [rate=R] [event=learned|aged|move] [start=S]
    // notify port_state count=N [batch=B] [rate=R]
    // notify status

    if (args.size() >= 2 && args[1] == "status")
    {
        return "{\"ok\":true,\"running\":" + std::to_string(m_notifiersRunning.load()) + "}";
    }

    if (args.size() < 2)
    {
        return jsonError("usage: notify <fdb|port_state|status> count=N [batch=B] [rate=per second]");
    }

    auto kv = keyValues(args, 2);

    uint64_t count;
    uint64_t batch;
    uint64_t rate;
    uint64_t start;

    try
    {
        count = kv.count("count") ? std::stoull(kv["count"]) : 1;
        batch = kv.count("batch") ? std::stoull(kv["batch"]) : 1;
        rate = kv.count("rate") ? std::stoull(kv["rate"]) : 0;
        start = kv.count("start") ? std::stoull(kv["start"]) : 0;
    }
    catch (const std::exception& e)
    {
        return jsonError(std::string("invalid argument: ") + e.what());
    }

    batch = std::min<uint64_t>(std::max<uint64_t>(batch, 1), 10000);

    {
        MUTEX();

        if (m_switches.empty())
        {
            return jsonError("no switch");
        }
    }

    std::function<void()> notifier;

    if (args[1] == "fdb")
    {
        sai_fdb_event_t event = SAI_FDB_EVENT_LEARNED;

        std::string name = kv.count("event") ? kv["event"] : "learned";

        if (name == "aged")
        {
            event = SAI_FDB_EVENT_AGED;
        }
        else if (name == "move")
        {
            event = SAI_FDB_EVENT_MOVE;
        }
        else if (name != "learned")
        {
            return jsonError("unknown fdb event " + name);
        }

        notifier = [this, count, batch, rate, event, start]() {
            fdbNotifier(count, (uint32_t)batch, rate, event, start);
        };
    }
    else if (args[1] == "port_state")
    {
        notifier = [this, count, batch, rate]() {
            portNotifier(count, (uint32_t)batch, rate);
        };
    }
    else
    {
        return jsonError("unknown notification " + args[1]);
    }

    // before the thread starts, which decrements it when it is done
    m_notifiersRunning++;

    std::shared_ptr<std::thread> thread;

    try
    {
        thread = std::make_shared<std::thread>(notifier);
    }
    catch (const std::exception& e)
    {
        m_notifiersRunning--;

        return jsonError(std::string("failed to start the notifier: ") + e.what());
    }

    std::lock_guard<std::mutex> lock(m_notifierMutex);

    m_notifiers.push_back(thread);

    return "{\"ok\":true,\"count\":" + std::to_string(count) + "}";
}

/*
 * Notifications go to syncd's callbacks from this thread, as an SDK event
 * thread would deliver them, without holding the API mutex.
 */

static void paceTo(
        _In_ uint64_t startNs,
        _In_ uint64_t sent,
        _In_ uint64_t rate,
        _In_ uint64_t nowNs)
{
    SWSS_LOG_ENTER();

    if (rate == 0)
    {
        return;
    }

    uint64_t due = startNs + sent * 1000000000ULL / rate;

    if (due > nowNs)
    {
        uint64_t ns = due - nowNs;

        struct timespec ts;

        ts.tv_sec = (time_t)(ns / 1000000000ULL);
        ts.tv_nsec = (long)(ns % 1000000000ULL);

        while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        {
            // a signal woke it up: ts has the time left
        }
    }
}

void MockSai::fdbNotifier(
        _In_ uint64_t count,
        _In_ uint32_t batch,
        _In_ uint64_t rate,
        _In_ sai_fdb_event_t event,
        _In_ uint64_t start)
{
    SWSS_LOG_ENTER();

    sai_fdb_event_notification_fn callback = nullptr;
    sai_object_id_t switchRid = SAI_NULL_OBJECT_ID;
    sai_object_id_t vlan = SAI_NULL_OBJECT_ID;

    std::vector<sai_object_id_t> bridgePorts;

    {
        MUTEX();

        for (const auto& it: m_switches)
        {
            auto ntf = it.second.notifications.find(SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY);

            if (ntf != it.second.notifications.end() && ntf->second)
            {
                callback = (sai_fdb_event_notification_fn)ntf->second;
                switchRid = it.second.switchRid;
                vlan = it.second.defaultVlan;
                bridgePorts = it.second.bridgePorts;
                break;
            }
        }
    }

    if (callback == nullptr || bridgePorts.empty())
    {
        SWSS_LOG_ERROR("no fdb event callback or no bridge port, no fdb events sent");

        m_notifiersRunning--;

        return;
    }

    std::vector<sai_fdb_event_notification_data_t> data(batch);
    std::vector<sai_attribute_t> attrs((size_t)batch * 3);

    uint64_t startNs = nowNs();
    uint64_t sent = 0;

    while (sent < count && m_runControl)
    {
        uint32_t n = (uint32_t)std::min<uint64_t>(batch, count - sent);

        for (uint32_t idx = 0; idx < n; idx++)
        {
            // MAC number start + seq: the caller picks new MACs or old ones

            uint64_t seq = start + sent + idx;

            auto& d = data[idx];

            memset(&d, 0, sizeof(d));

            d.event_type = event;
            d.fdb_entry.switch_id = switchRid;
            d.fdb_entry.bv_id = vlan;
            d.fdb_entry.mac_address[0] = 0x02;
            d.fdb_entry.mac_address[1] = (uint8_t)(seq >> 32);
            d.fdb_entry.mac_address[2] = (uint8_t)(seq >> 24);
            d.fdb_entry.mac_address[3] = (uint8_t)(seq >> 16);
            d.fdb_entry.mac_address[4] = (uint8_t)(seq >> 8);
            d.fdb_entry.mac_address[5] = (uint8_t)seq;

            sai_attribute_t* a = &attrs[(size_t)idx * 3];

            a[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
            a[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
            a[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
            a[1].value.oid = bridgePorts[seq % bridgePorts.size()];
            a[2].id = SAI_FDB_ENTRY_ATTR_PACKET_ACTION;
            a[2].value.s32 = SAI_PACKET_ACTION_FORWARD;

            d.attr_count = 3;
            d.attr = a;
        }

        {
            // the switch learns or ages the entries it reports

            MUTEX();

            for (uint32_t idx = 0; idx < n; idx++)
            {
                sai_object_meta_key_t mk;

                memset(&mk, 0, sizeof(mk));

                mk.objecttype = SAI_OBJECT_TYPE_FDB_ENTRY;
                mk.objectkey.key.fdb_entry = data[idx].fdb_entry;

                if (event == SAI_FDB_EVENT_AGED)
                {
                    doRemoveEntry(mk);
                }
                else
                {
                    doRemoveEntry(mk);
                    doCreateEntry(mk, data[idx].attr_count, data[idx].attr);
                }
            }
        }

        uint64_t t0 = nowNs();

        callback(n, data.data());

        uint64_t t1 = nowNs();

        {
            MUTEX();

            account(Op::NOTIFY, SAI_OBJECT_TYPE_FDB_ENTRY, n, 0, 0, t0, 0, t1);
        }

        sent += n;

        paceTo(startNs, sent, rate, nowNs());
    }

    SWSS_LOG_NOTICE("sent %" PRIu64 " fdb events in %.3f s", sent, (double)(nowNs() - startNs) / 1e9);

    m_notifiersRunning--;
}

void MockSai::portNotifier(
        _In_ uint64_t count,
        _In_ uint32_t batch,
        _In_ uint64_t rate)
{
    SWSS_LOG_ENTER();

    sai_port_state_change_notification_fn callback = nullptr;

    std::vector<sai_object_id_t> ports;

    {
        MUTEX();

        for (const auto& it: m_switches)
        {
            auto ntf = it.second.notifications.find(SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY);

            if (ntf != it.second.notifications.end() && ntf->second)
            {
                callback = (sai_port_state_change_notification_fn)ntf->second;
                ports = it.second.ports;
                break;
            }
        }
    }

    if (callback == nullptr || ports.empty())
    {
        SWSS_LOG_ERROR("no port state change callback or no port, no port events sent");

        m_notifiersRunning--;

        return;
    }

    std::vector<sai_port_oper_status_notification_t> data(batch);

    uint64_t startNs = nowNs();
    uint64_t sent = 0;

    while (sent < count && m_runControl)
    {
        uint32_t n = (uint32_t)std::min<uint64_t>(batch, count - sent);

        {
            // each event flips the port, and get returns the new state

            MUTEX();

            for (uint32_t idx = 0; idx < n; idx++)
            {
                sai_object_id_t port = ports[(sent + idx) % ports.size()];

                auto it = m_oids.find(port);

                int32_t state = SAI_PORT_OPER_STATUS_DOWN;

                if (it != m_oids.end())
                {
                    for (auto& stored: it->second.attrs)
                    {
                        if (stored.attr.id == SAI_PORT_ATTR_OPER_STATUS)
                        {
                            state = stored.attr.value.s32 == SAI_PORT_OPER_STATUS_UP ?
                                SAI_PORT_OPER_STATUS_DOWN : SAI_PORT_OPER_STATUS_UP;

                            stored.attr.value.s32 = state;
                        }
                    }
                }

                memset(&data[idx], 0, sizeof(data[idx]));

                data[idx].port_id = port;
                data[idx].port_state = (sai_port_oper_status_t)state;
            }
        }

        uint64_t t0 = nowNs();

        callback(n, data.data());

        uint64_t t1 = nowNs();

        {
            MUTEX();

            account(Op::NOTIFY, SAI_OBJECT_TYPE_PORT, n, 0, 0, t0, 0, t1);
        }

        sent += n;

        paceTo(startNs, sent, rate, nowNs());
    }

    SWSS_LOG_NOTICE("sent %" PRIu64 " port state events in %.3f s", sent, (double)(nowNs() - startNs) / 1e9);

    m_notifiersRunning--;
}

void MockSai::joinNotifiers()
{
    SWSS_LOG_ENTER();

    std::vector<std::shared_ptr<std::thread>> threads;

    {
        std::lock_guard<std::mutex> lock(m_notifierMutex);

        threads.swap(m_notifiers);
    }

    for (auto& thread: threads)
    {
        if (thread->joinable())
        {
            thread->join();
        }
    }
}

void MockSai::startControlServer()
{
    SWSS_LOG_ENTER();

    if (m_config.controlSocket.empty() || m_controlThread)
    {
        return;
    }

    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    if (m_config.controlSocket.size() >= sizeof(addr.sun_path))
    {
        SWSS_LOG_ERROR("control socket path too long: %s", m_config.controlSocket.c_str());

        return;
    }

    strncpy(addr.sun_path, m_config.controlSocket.c_str(), sizeof(addr.sun_path) - 1);

    unlink(m_config.controlSocket.c_str());

    m_controlFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (m_controlFd < 0 ||
            bind(m_controlFd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ||
            listen(m_controlFd, 16) != 0)
    {
        SWSS_LOG_ERROR("failed to listen on %s: %s", m_config.controlSocket.c_str(), strerror(errno));

        if (m_controlFd >= 0)
        {
            close(m_controlFd);
            m_controlFd = -1;
        }

        return;
    }

    chmod(m_config.controlSocket.c_str(), 0600);

    m_stopFd = eventfd(0, EFD_CLOEXEC);

    m_runControl = true;

    m_controlThread = std::make_shared<std::thread>(&MockSai::controlServerThread, this);

    SWSS_LOG_NOTICE("control socket %s", m_config.controlSocket.c_str());
}

void MockSai::stopControlServer()
{
    SWSS_LOG_ENTER();

    m_runControl = false;

    if (m_stopFd >= 0)
    {
        uint64_t one = 1;

        if (write(m_stopFd, &one, sizeof(one)) != sizeof(one))
        {
            SWSS_LOG_WARN("failed to wake the control thread");
        }
    }

    if (m_controlThread && m_controlThread->joinable())
    {
        m_controlThread->join();
    }

    m_controlThread = nullptr;

    if (m_controlFd >= 0)
    {
        close(m_controlFd);
        m_controlFd = -1;

        unlink(m_config.controlSocket.c_str());
    }

    if (m_stopFd >= 0)
    {
        close(m_stopFd);
        m_stopFd = -1;
    }
}

void MockSai::controlServerThread()
{
    SWSS_LOG_ENTER();

    while (m_runControl)
    {
        struct pollfd fds[2];

        fds[0].fd = m_controlFd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = m_stopFd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        int ready = poll(fds, 2, -1);

        if (ready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            SWSS_LOG_ERROR("control poll failed: %s", strerror(errno));
            break;
        }

        if (fds[1].revents)
        {
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            int fd = accept4(m_controlFd, nullptr, nullptr, SOCK_CLOEXEC);

            if (fd >= 0)
            {
                serveClient(fd);

                close(fd);
            }
        }
    }
}

void MockSai::serveClient(
        _In_ int fd)
{
    SWSS_LOG_ENTER();

    std::string buffer;

    char chunk[4096];

    while (m_runControl)
    {
        struct pollfd fds[2];

        fds[0].fd = fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = m_stopFd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        if (poll(fds, 2, -1) < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return;
        }

        if (fds[1].revents)
        {
            return;
        }

        ssize_t len = read(fd, chunk, sizeof(chunk));

        if (len <= 0)
        {
            return;
        }

        buffer.append(chunk, (size_t)len);

        size_t pos;

        while ((pos = buffer.find('\n')) != std::string::npos)
        {
            std::string line = buffer.substr(0, pos);

            buffer.erase(0, pos + 1);

            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            std::string response;

            try
            {
                response = control(line) + "\n";
            }
            catch (const std::exception& e)
            {
                response = jsonError(e.what()) + "\n";
            }

            size_t offset = 0;

            while (offset < response.size())
            {
                // MSG_NOSIGNAL: a client gone, say after a timeout, must not
                // kill syncd with SIGPIPE

                ssize_t written = send(fd, response.data() + offset, response.size() - offset, MSG_NOSIGNAL);

                if (written <= 0)
                {
                    if (written < 0 && errno == EINTR)
                    {
                        continue;
                    }

                    return;
                }

                offset += (size_t)written;
            }
        }
    }
}
