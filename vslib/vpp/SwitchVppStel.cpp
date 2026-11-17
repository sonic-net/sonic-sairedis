#include "SwitchVpp.h"

#include "meta/sai_serialize.h"
#include "swss/logger.h"
#include "vppxlate/SaiIntfStats.h"

#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "sonic_stel_uapi.h"

using namespace saivs;

namespace
{
    constexpr uint64_t STEL_MIN_INTERVAL_NS = 1000000;

    class GenlConnection
    {
        public:
            ~GenlConnection()
            {
                // SWSS_LOG_ENTER() omitted - trivial RAII cleanup.
                if (m_socket != nullptr)
                {
                    nl_socket_free(m_socket);
                }
            }

            bool connect()
            {
                SWSS_LOG_ENTER();
                m_socket = nl_socket_alloc();
                if (m_socket == nullptr)
                {
                    SWSS_LOG_ERROR("Failed to allocate the STEL netlink socket");
                    return false;
                }

                nl_socket_disable_seq_check(m_socket);

                int status = genl_connect(m_socket);
                if (status < 0)
                {
                    SWSS_LOG_ERROR("Failed to connect the STEL netlink socket: %s",
                            nl_geterror(status));
                    return false;
                }

                m_family = genl_ctrl_resolve(m_socket, SONIC_STEL_FAMILY_NAME);
                if (m_family < 0)
                {
                    SWSS_LOG_ERROR("Failed to resolve STEL netlink family %s: %s",
                            SONIC_STEL_FAMILY_NAME,
                            nl_geterror(m_family));
                    return false;
                }

                return true;
            }

            bool send(const std::vector<uint8_t>& payload)
            {
                // SWSS_LOG_ENTER() omitted - per-sample hot path.
                struct nl_msg *message = nlmsg_alloc();
                if (message == nullptr)
                {
                    return false;
                }

                if (genlmsg_put(
                        message,
                        NL_AUTO_PID,
                        NL_AUTO_SEQ,
                        m_family,
                        0,
                        0,
                        SONIC_STEL_CMD_SEND_IPFIX,
                        1) == nullptr)
                {
                    nlmsg_free(message);
                    return false;
                }

                int status = nla_put(
                        message,
                        SONIC_STEL_ATTR_IPFIX_DATA,
                        static_cast<int>(payload.size()),
                        payload.data());
                if (status >= 0)
                {
                    status = nl_send_auto(m_socket, message);
                }

                nlmsg_free(message);
                return status >= 0;
            }

        private:
            struct nl_sock *m_socket = nullptr;
            int m_family = -1;
    };

    void putU16Be(std::vector<uint8_t>& buffer, uint16_t value)
    {
        // SWSS_LOG_ENTER() omitted - serialization helper.
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
        buffer.push_back(static_cast<uint8_t>(value & 0xff));
    }

    void putU32Be(std::vector<uint8_t>& buffer, uint32_t value)
    {
        // SWSS_LOG_ENTER() omitted - serialization helper.
        buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
        buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
        buffer.push_back(static_cast<uint8_t>(value & 0xff));
    }

    void putU64Be(std::vector<uint8_t>& buffer, uint64_t value)
    {
        // SWSS_LOG_ENTER() omitted - serialization helper.
        for (int shift = 56; shift >= 0; shift -= 8)
        {
            buffer.push_back(static_cast<uint8_t>((value >> shift) & 0xff));
        }
    }

    std::vector<uint8_t> buildDataMessage(
            const TamTemplateKey& key,
            uint32_t sequence,
            uint64_t timestampNs,
            const std::vector<uint64_t>& counters)
    {
        // SWSS_LOG_ENTER() omitted - per-sample serialization helper.
        const size_t setLength = 12 + counters.size() * 8;
        const size_t messageLength = 16 + setLength;
        std::vector<uint8_t> message;
        message.reserve(messageLength);

        putU16Be(message, 10);
        putU16Be(message, static_cast<uint16_t>(messageLength));
        putU32Be(message, static_cast<uint32_t>(timestampNs / 1000000000));
        putU32Be(message, sequence);
        putU32Be(message, key.observationDomainId);

        putU16Be(message, key.templateId);
        putU16Be(message, static_cast<uint16_t>(setLength));
        putU64Be(message, timestampNs);
        for (uint64_t counter : counters)
        {
            putU64Be(message, counter);
        }

        return message;
    }

    bool getPortCounterValue(
            const vpp_interface_stats_t& stats,
            sai_stat_id_t statId,
            uint64_t& value)
    {
        // SWSS_LOG_ENTER() omitted - per-counter hot path.
        switch (static_cast<sai_port_stat_t>(statId))
        {
            case SAI_PORT_STAT_IF_IN_OCTETS:
                value = stats.rx_bytes;
                return true;
            case SAI_PORT_STAT_IF_IN_DISCARDS:
                value = stats.drops;
                return true;
            case SAI_PORT_STAT_IF_OUT_OCTETS:
                value = stats.tx_bytes;
                return true;
            case SAI_PORT_STAT_IF_IN_UCAST_PKTS:
                value = stats.rx_unicast;
                return true;
            case SAI_PORT_STAT_IF_OUT_ERRORS:
                value = stats.tx_error;
                return true;
            case SAI_PORT_STAT_IF_OUT_UCAST_PKTS:
                value = stats.tx_unicast;
                return true;
            default:
                return false;
        }
    }
}

sai_status_t SwitchVpp::getTamReportInterval(
        _In_ sai_object_id_t tam_tel_type_id,
        _Out_ std::chrono::nanoseconds& interval,
        _Out_ sai_object_id_t *report_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t telTypeAttr{};
    telTypeAttr.id = SAI_TAM_TEL_TYPE_ATTR_REPORT_ID;
    CHECK_STATUS(get(
            SAI_OBJECT_TYPE_TAM_TEL_TYPE,
            tam_tel_type_id,
            1,
            &telTypeAttr));

    if (report_id != nullptr)
    {
        *report_id = telTypeAttr.value.oid;
    }
    return getReportInterval(telTypeAttr.value.oid, interval);
}

sai_status_t SwitchVpp::getReportInterval(
        _In_ sai_object_id_t report_id,
        _Out_ std::chrono::nanoseconds& interval)
{
    SWSS_LOG_ENTER();

    sai_attribute_t reportAttrs[2]{};
    reportAttrs[0].id = SAI_TAM_REPORT_ATTR_REPORT_INTERVAL;
    reportAttrs[1].id = SAI_TAM_REPORT_ATTR_REPORT_INTERVAL_UNIT;
    CHECK_STATUS(get(
            SAI_OBJECT_TYPE_TAM_REPORT,
            report_id,
            2,
            reportAttrs));

    uint64_t multiplier;
    switch (reportAttrs[1].value.s32)
    {
        case SAI_TAM_REPORT_INTERVAL_UNIT_NANOSEC:
            multiplier = 1;
            break;
        case SAI_TAM_REPORT_INTERVAL_UNIT_USEC:
            multiplier = 1000;
            break;
        case SAI_TAM_REPORT_INTERVAL_UNIT_MSEC:
            multiplier = 1000000;
            break;
        default:
            return SAI_STATUS_NOT_SUPPORTED;
    }

    uint64_t value = reportAttrs[0].value.u32;
    if (value > std::numeric_limits<uint64_t>::max() / multiplier)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    uint64_t intervalNs = value * multiplier;
    if (intervalNs < STEL_MIN_INTERVAL_NS)
    {
        SWSS_LOG_ERROR("VPP STEL interval %lu ns is below the 1 ms minimum",
                static_cast<unsigned long>(intervalNs));
        return SAI_STATUS_NOT_SUPPORTED;
    }

    interval = std::chrono::nanoseconds(intervalNs);
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::updateStelReportInterval(
        _In_ sai_object_id_t report_id)
{
    SWSS_LOG_ENTER();

    std::chrono::nanoseconds interval;
    CHECK_STATUS(getReportInterval(report_id, interval));

    std::lock_guard<std::mutex> streamsLock(m_stelMutex);
    for (auto& item : m_stelStreams)
    {
        auto& stream = item.second;
        if (stream->reportId != report_id)
        {
            continue;
        }

        {
            std::lock_guard<std::mutex> wakeLock(stream->wakeMutex);
            stream->interval = interval;
            ++stream->intervalVersion;
        }
        stream->wake.notify_all();
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::startStelStream(
        _In_ sai_object_id_t tam_tel_type_id)
{
    SWSS_LOG_ENTER();

    stopStelStream(tam_tel_type_id);

    auto subscriptions = getTamCounterSubscriptions(tam_tel_type_id);
    if (subscriptions.empty())
    {
        SWSS_LOG_ERROR("VPP STEL telemetry type %s has no subscriptions",
                sai_serialize_object_id(tam_tel_type_id).c_str());
        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto stream = std::make_shared<StelStream>();
    CHECK_STATUS(getTamReportInterval(
            tam_tel_type_id,
            stream->interval,
            &stream->reportId));
    if (!getTamTemplateKey(tam_tel_type_id, stream->templateKey))
    {
        return SAI_STATUS_UNINITIALIZED;
    }

    for (const auto& subscription : subscriptions)
    {
        if (subscription.objectType != SAI_OBJECT_TYPE_PORT)
        {
            return SAI_STATUS_NOT_SUPPORTED;
        }

        std::string hwifName = m_ifaceRegistry.resolveHwIfName(
                subscription.objectId,
                0);
        if (hwifName.empty())
        {
            SWSS_LOG_ERROR("No VPP interface for STEL object %s",
                    sai_serialize_object_id(subscription.objectId).c_str());
            return SAI_STATUS_ITEM_NOT_FOUND;
        }

        vpp_interface_stats_t stats{};
        uint64_t value = 0;
        if (vpp_intf_stats_query(hwifName.c_str(), &stats) != 0 ||
            !getPortCounterValue(stats, subscription.statId, value))
        {
            SWSS_LOG_ERROR("Unsupported VPP STEL counter %d on %s",
                    subscription.statId,
                    hwifName.c_str());
            return SAI_STATUS_NOT_SUPPORTED;
        }

        stream->counters.push_back({subscription, std::move(hwifName)});
    }

    stream->running = true;
    stream->worker = std::thread(&SwitchVpp::stelWorker, this, stream);

    {
        std::unique_lock<std::mutex> lock(stream->wakeMutex);
        if (!stream->wake.wait_for(
                lock,
                std::chrono::seconds(5),
                [&stream] { return stream->startupDone; }) ||
            !stream->startupOk)
        {
            stream->running = false;
            lock.unlock();
            stream->wake.notify_all();
            if (stream->worker.joinable())
            {
                stream->worker.join();
            }
            return SAI_STATUS_FAILURE;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_stelMutex);
        m_stelStreams[tam_tel_type_id] = stream;
    }

    SWSS_LOG_NOTICE("Started VPP STEL stream %s with %zu counters at %lu ns",
            sai_serialize_object_id(tam_tel_type_id).c_str(),
            stream->counters.size(),
            static_cast<unsigned long>(stream->interval.count()));
    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::stopStelStream(
        _In_ sai_object_id_t tam_tel_type_id)
{
    SWSS_LOG_ENTER();

    std::shared_ptr<StelStream> stream;
    {
        std::lock_guard<std::mutex> lock(m_stelMutex);
        auto it = m_stelStreams.find(tam_tel_type_id);
        if (it == m_stelStreams.end())
        {
            return;
        }
        stream = it->second;
        m_stelStreams.erase(it);
    }

    stream->running = false;
    stream->wake.notify_all();
    if (stream->worker.joinable())
    {
        stream->worker.join();
    }
}

void SwitchVpp::stopAllStelStreams()
{
    SWSS_LOG_ENTER();

    std::vector<std::shared_ptr<StelStream>> streams;
    {
        std::lock_guard<std::mutex> lock(m_stelMutex);
        for (auto& stream : m_stelStreams)
        {
            stream.second->running = false;
            stream.second->wake.notify_all();
            streams.push_back(stream.second);
        }
        m_stelStreams.clear();
    }

    for (auto& stream : streams)
    {
        if (stream->worker.joinable())
        {
            stream->worker.join();
        }
    }
}

void SwitchVpp::stelWorker(
        _In_ const std::shared_ptr<StelStream>& stream)
{
    SWSS_LOG_ENTER();

    GenlConnection connection;
    const bool connected = connection.connect();
    {
        std::lock_guard<std::mutex> lock(stream->wakeMutex);
        stream->startupOk = connected;
        stream->startupDone = true;
        if (!connected)
        {
            stream->running = false;
        }
    }
    stream->wake.notify_all();
    if (!connected)
    {
        return;
    }

    uint32_t sequence = 0;
    bool sendFailureLogged = false;
    auto wallTime = std::chrono::system_clock::now();
    auto wakeTime = std::chrono::steady_clock::now();

    while (stream->running)
    {
        std::chrono::nanoseconds interval;
        uint64_t intervalVersion;
        {
            std::lock_guard<std::mutex> lock(stream->wakeMutex);
            interval = stream->interval;
            intervalVersion = stream->intervalVersion;
        }

        bool sampleOk = true;
        std::map<std::string, vpp_interface_stats_t> portStats;
        for (const auto& counter : stream->counters)
        {
            if (portStats.find(counter.hwifName) == portStats.end())
            {
                vpp_interface_stats_t stats{};
                if (vpp_intf_stats_query(counter.hwifName.c_str(), &stats) == 0)
                {
                    portStats[counter.hwifName] = stats;
                }
                else
                {
                    sampleOk = false;
                }
            }
        }

        std::vector<uint64_t> values;
        values.reserve(stream->counters.size());
        for (const auto& counter : stream->counters)
        {
            auto it = portStats.find(counter.hwifName);
            uint64_t value = 0;
            if (it == portStats.end() || !getPortCounterValue(
                    it->second,
                    counter.subscription.statId,
                    value))
            {
                sampleOk = false;
                break;
            }
            values.push_back(value);
        }

        uint64_t timestampNs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                        wallTime.time_since_epoch()).count());
        if (sampleOk)
        {
            if (!connection.send(buildDataMessage(
                    stream->templateKey,
                    sequence++,
                    timestampNs,
                    values)))
            {
                if (!sendFailureLogged)
                {
                    SWSS_LOG_WARN("Failed to send a VPP STEL IPFIX record");
                    sendFailureLogged = true;
                }
            }
            else
            {
                sendFailureLogged = false;
            }
        }

        wallTime += interval;
        wakeTime += interval;
        std::unique_lock<std::mutex> lock(stream->wakeMutex);
        const bool interrupted = stream->wake.wait_until(
                lock,
                wakeTime,
                [&stream, intervalVersion]
                {
                    return !stream->running ||
                           stream->intervalVersion != intervalVersion;
                });
        if (interrupted && stream->running)
        {
            wallTime = std::chrono::system_clock::now();
            wakeTime = std::chrono::steady_clock::now();
        }
    }
}

sai_status_t SwitchVpp::setTamTelType(
        _In_ sai_object_id_t tam_tel_type_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr == nullptr)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr->id != SAI_TAM_TEL_TYPE_ATTR_STATE)
    {
        return set_internal(
                SAI_OBJECT_TYPE_TAM_TEL_TYPE,
                sai_serialize_object_id(tam_tel_type_id),
                attr);
    }

    switch (attr->value.s32)
    {
        case SAI_TAM_TEL_TYPE_STATE_CREATE_CONFIG:
            send_tam_tel_type_config_change(tam_tel_type_id);
            break;
        case SAI_TAM_TEL_TYPE_STATE_START_STREAM:
            CHECK_STATUS(startStelStream(tam_tel_type_id));
            break;
        case SAI_TAM_TEL_TYPE_STATE_STOP_STREAM:
            stopStelStream(tam_tel_type_id);
            break;
        default:
            return SAI_STATUS_INVALID_PARAMETER;
    }

    return set_internal(
            SAI_OBJECT_TYPE_TAM_TEL_TYPE,
            sai_serialize_object_id(tam_tel_type_id),
            attr);
}
