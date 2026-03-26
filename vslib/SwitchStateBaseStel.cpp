#include "SwitchStateBase.h"

#include "swss/logger.h"

#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>
#include <cassert>

#include "sonic_stel_uapi.h"

using namespace saivs;

/*
 * Virtual Stream Telemetry (STEL) implementation for vslib.
 *
 * When stream telemetry is enabled, vslib periodically generates
 * fake IPFIX data records and sends them through the sonic_stel
 * kernel module to countersyncd via genetlink multicast.
 *
 * Data flow:
 *   vslib (this code)
 *     → libnl-genl → NETLINK_GENERIC socket
 *     → sonic_stel.ko (SONIC_STEL_CMD_SEND_IPFIX)
 *     → genlmsg_multicast to "ipfix" group
 *     → countersyncd
 */

/* ────── genetlink helpers (using libnl-genl) ────── */

/**
 * @brief RAII wrapper for libnl socket + family resolution.
 */
struct GenlConnection
{
    struct nl_sock *sock = nullptr;
    int family_id = -1;

    GenlConnection() = default;

    ~GenlConnection()
    {
        if (sock)
        {
            nl_socket_free(sock);
            sock = nullptr;
        }
    }

    /**
     * @brief Connect to genetlink and resolve sonic_stel family.
     * @return 0 on success, -1 on failure
     */
    int connect()
    {
        SWSS_LOG_ENTER();
        sock = nl_socket_alloc();
        if (!sock)
        {
            SWSS_LOG_ERROR("Failed to allocate nl_sock");
            return -1;
        }

        /* Disable sequence number checking (we send fire-and-forget) */
        nl_socket_disable_seq_check(sock);

        int ret = genl_connect(sock);
        if (ret < 0)
        {
            SWSS_LOG_ERROR("genl_connect() failed: %s", nl_geterror(ret));
            nl_socket_free(sock);
            sock = nullptr;
            return -1;
        }

        family_id = genl_ctrl_resolve(sock, SONIC_STEL_FAMILY_NAME);
        if (family_id < 0)
        {
            SWSS_LOG_ERROR("genl_ctrl_resolve('%s') failed: %s - is the kernel module loaded?",
                           SONIC_STEL_FAMILY_NAME, nl_geterror(family_id));
            nl_socket_free(sock);
            sock = nullptr;
            return -1;
        }

        SWSS_LOG_NOTICE("Resolved sonic_stel family_id=%d", family_id);
        return 0;
    }

    /**
     * @brief Send IPFIX data via genetlink command.
     * @return 0 on success, negative on failure
     */
    int sendIpfix(const uint8_t *data, size_t data_len)
    {
        SWSS_LOG_ENTER();
        struct nl_msg *msg = nlmsg_alloc();
        if (!msg)
        {
            SWSS_LOG_ERROR("nlmsg_alloc() failed");
            return -1;
        }

        if (!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 0,
                          SONIC_STEL_CMD_SEND_IPFIX, 1))
        {
            SWSS_LOG_ERROR("genlmsg_put() failed");
            nlmsg_free(msg);
            return -1;
        }

        int ret = nla_put(msg, SONIC_STEL_ATTR_IPFIX_DATA, static_cast<int>(data_len), data);
        if (ret < 0)
        {
            SWSS_LOG_ERROR("nla_put() failed: %s", nl_geterror(ret));
            nlmsg_free(msg);
            return -1;
        }

        ret = nl_send_auto(sock, msg);
        nlmsg_free(msg);

        if (ret < 0)
        {
            SWSS_LOG_ERROR("nl_send_auto() failed: %s", nl_geterror(ret));
            return -1;
        }

        return 0;
    }

    /* Non-copyable */
    GenlConnection(const GenlConnection&) = delete;
    GenlConnection& operator=(const GenlConnection&) = delete;
};

/* ────── IPFIX data record generation ────── */

/**
 * Helper to write big-endian values into a buffer at a given offset.
 * All helpers assert that offset + size <= buf.size().
 */
static void put_u16_be(std::vector<uint8_t> &buf, size_t offset, uint16_t val)
{
    // SWSS_LOG_ENTER() omitted - hot-path helper
    assert(offset + 2 <= buf.size());
    buf[offset]     = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>(val & 0xFF);
}

static void put_u32_be(std::vector<uint8_t> &buf, size_t offset, uint32_t val)
{
    // SWSS_LOG_ENTER() omitted - hot-path helper
    assert(offset + 4 <= buf.size());
    buf[offset]     = static_cast<uint8_t>((val >> 24) & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[offset + 2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[offset + 3] = static_cast<uint8_t>(val & 0xFF);
}

static void put_u64_be(std::vector<uint8_t> &buf, size_t offset, uint64_t val)
{
    // SWSS_LOG_ENTER() omitted - hot-path helper
    assert(offset + 8 <= buf.size());
    for (int i = 7; i >= 0; --i)
    {
        buf[offset + (7 - i)] = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
    }
}

/**
 * @brief Build a complete IPFIX message with header + data set.
 *
 * Format per HLD 7.2.1 + 7.2.3:
 *   IPFIX Header (16B):
 *     Version=0x000a, Length, Timestamp, SeqNum, DomainID=0
 *   Data Set:
 *     Set ID = template_id, Set Length
 *     observationTimeNanoseconds (8B)
 *     Counter values (8B each)
 */
static std::vector<uint8_t> build_ipfix_data_message(
    uint16_t template_id,
    uint32_t seq_num,
    uint64_t timestamp_ns,
    const std::vector<uint64_t> &counter_values)
{
    // SWSS_LOG_ENTER() omitted - called from hot-path worker thread
    size_t num_counters = counter_values.size();
    // Data set: set_header(4) + timestamp(8) + counters(8*N)
    size_t data_set_len = 4 + 8 + num_counters * 8;
    // IPFIX message: header(16) + data_set
    size_t msg_len = 16 + data_set_len;

    std::vector<uint8_t> msg(msg_len, 0);
    size_t off = 0;

    /* IPFIX Header */
    put_u16_be(msg, off, 0x000a);      off += 2;  // Version
    put_u16_be(msg, off, static_cast<uint16_t>(msg_len)); off += 2;  // Message Length
    auto now_s = static_cast<uint32_t>(timestamp_ns / 1000000000ULL);
    put_u32_be(msg, off, now_s);       off += 4;  // Export Timestamp (seconds)
    put_u32_be(msg, off, seq_num);     off += 4;  // Sequence Number
    put_u32_be(msg, off, 0);           off += 4;  // Observation Domain ID = 0

    /* Data Set Header */
    put_u16_be(msg, off, template_id); off += 2;  // Set ID = Template ID
    put_u16_be(msg, off, static_cast<uint16_t>(data_set_len)); off += 2; // Set Length

    /* observationTimeNanoseconds */
    put_u64_be(msg, off, timestamp_ns); off += 8;

    /* Counter values */
    for (auto val : counter_values)
    {
        put_u64_be(msg, off, val);     off += 8;
    }

    return msg;
}

/**
 * @brief Build a complete IPFIX message wrapping a template set.
 *
 * The template binary from refresh_tam_tel_ipfix_templates() is already
 * a valid Template Set (Set ID=2). We wrap it in an IPFIX Message Header.
 */
static std::vector<uint8_t> build_ipfix_template_message(
    uint32_t seq_num,
    const std::vector<uint8_t> &template_set)
{
    // SWSS_LOG_ENTER() omitted - called from worker thread
    size_t msg_len = 16 + template_set.size();
    std::vector<uint8_t> msg(msg_len, 0);
    size_t off = 0;

    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto now_s = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now).count());

    /* IPFIX Header */
    put_u16_be(msg, off, 0x000a);      off += 2;  // Version
    put_u16_be(msg, off, static_cast<uint16_t>(msg_len)); off += 2;
    put_u32_be(msg, off, now_s);       off += 4;  // Export Timestamp
    put_u32_be(msg, off, seq_num);     off += 4;  // Sequence Number
    put_u32_be(msg, off, 0);           off += 4;  // Observation Domain ID

    /* Template Set (already includes Set ID=2, Set Length, etc.) */
    std::memcpy(msg.data() + off, template_set.data(), template_set.size());

    return msg;
}

/* ────── Stream telemetry thread ────── */

void SwitchStateBase::stelWorkerThread(
    _In_ uint32_t poll_interval_us,
    _In_ uint16_t template_id,
    _In_ size_t num_counters,
    _In_ std::vector<uint8_t> ipfix_template)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("STEL worker thread started: poll_interval=%u us, template_id=%u, counters=%zu",
                    poll_interval_us, template_id, num_counters);

    /* Connect to sonic_stel via libnl-genl */
    GenlConnection conn;
    if (conn.connect() < 0)
    {
        SWSS_LOG_ERROR("Failed to connect to sonic_stel genetlink - aborting STEL thread");
        return;
    }

    /* Generate fake counter data in a loop */
    uint32_t seq_num = 0;
    std::vector<uint64_t> counters(num_counters, 0);

    /* Per RFC 7011, send Template Record before any Data Records.
     * Also re-send periodically (every ~30 seconds). */
    const uint32_t template_resend_interval = 30000000 / poll_interval_us; // ticks per 30s
    uint32_t ticks_since_template = template_resend_interval; // force send on first iteration

    while (m_stelRunning)
    {
        /* Send template periodically */
        if (!ipfix_template.empty() && ticks_since_template >= template_resend_interval)
        {
            auto tmpl_msg = build_ipfix_template_message(seq_num, ipfix_template);
            if (conn.sendIpfix(tmpl_msg.data(), tmpl_msg.size()) < 0)
            {
                SWSS_LOG_WARN("Failed to send IPFIX template (seq=%u)", seq_num);
            }
            else
            {
                SWSS_LOG_INFO("Sent IPFIX template (%zu bytes, seq=%u)", tmpl_msg.size(), seq_num);
            }
            seq_num++;
            ticks_since_template = 0;
        }

        /* Use system_clock for wall-clock time (IPFIX observationTimeNanoseconds) */
        auto now = std::chrono::system_clock::now().time_since_epoch();
        uint64_t timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

        /* Increment counters with fake data (monotonically increasing) */
        for (size_t i = 0; i < num_counters; i++)
        {
            counters[i] += (i + 1) * 10; // Simple pattern: counter[i] += (i+1)*10 per tick
        }

        auto ipfix_msg = build_ipfix_data_message(template_id, seq_num, timestamp_ns, counters);

        if (conn.sendIpfix(ipfix_msg.data(), ipfix_msg.size()) < 0)
        {
            SWSS_LOG_WARN("Failed to send IPFIX data (seq=%u), will retry", seq_num);
        }

        seq_num++;
        ticks_since_template++;
        std::this_thread::sleep_for(std::chrono::microseconds(poll_interval_us));
    }

    SWSS_LOG_NOTICE("STEL worker thread stopped");
}

sai_status_t SwitchStateBase::startStelStream(
    _In_ uint32_t poll_interval_us,
    _In_ uint16_t template_id,
    _In_ size_t num_counters,
    _In_ const std::vector<uint8_t> &ipfix_template)
{
    SWSS_LOG_ENTER();

    if (m_stelRunning)
    {
        SWSS_LOG_WARN("STEL stream already running, stopping first");
        stopStelStream();
    }

    m_stelRunning = true;
    m_stelThread = std::make_shared<std::thread>(
        &SwitchStateBase::stelWorkerThread, this,
        poll_interval_us, template_id, num_counters, ipfix_template);

    SWSS_LOG_NOTICE("STEL stream started");
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::stopStelStream()
{
    SWSS_LOG_ENTER();

    if (!m_stelRunning)
    {
        return SAI_STATUS_SUCCESS;
    }

    m_stelRunning = false;
    if (m_stelThread && m_stelThread->joinable())
    {
        m_stelThread->join();
    }
    m_stelThread = nullptr;

    SWSS_LOG_NOTICE("STEL stream stopped");
    return SAI_STATUS_SUCCESS;
}
