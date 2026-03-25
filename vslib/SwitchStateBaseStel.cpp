#include "SwitchStateBase.h"

#include "swss/logger.h"

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>

#include "kernel/sonic_stel_uapi.h"

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
 *     → NETLINK_GENERIC socket
 *     → sonic_stel.ko (SONIC_STEL_CMD_SEND_IPFIX)
 *     → genlmsg_multicast to "ipfix" group
 *     → countersyncd
 */

/* ────── genetlink helpers (raw socket, no libnl dependency) ────── */

/**
 * @brief Resolve genetlink family ID by name.
 *
 * Sends CTRL_CMD_GETFAMILY to the kernel and parses the response
 * to extract the family ID for the given family name.
 *
 * @return family ID on success, -1 on failure
 */
static int resolve_genl_family(int sock, const char *family_name)
{
    /* Build CTRL_CMD_GETFAMILY request using dynamic buffer for proper alignment */
    size_t name_len = strlen(family_name) + 1;
    size_t nla_padded = NLA_ALIGN(name_len);
    size_t msg_len = NLMSG_HDRLEN + GENL_HDRLEN + NLA_HDRLEN + nla_padded;

    std::vector<uint8_t> req_buf(msg_len, 0);

    /* nlmsghdr at offset 0 */
    auto *nlh = reinterpret_cast<struct nlmsghdr *>(req_buf.data());
    nlh->nlmsg_len   = msg_len;
    nlh->nlmsg_type  = GENL_ID_CTRL;
    nlh->nlmsg_flags = NLM_F_REQUEST;
    nlh->nlmsg_seq   = 1;
    nlh->nlmsg_pid   = 0;

    /* genlmsghdr at offset NLMSG_HDRLEN (16) */
    auto *genl = reinterpret_cast<struct genlmsghdr *>(req_buf.data() + NLMSG_HDRLEN);
    genl->cmd     = CTRL_CMD_GETFAMILY;
    genl->version = 1;

    /* nlattr at offset NLMSG_HDRLEN + GENL_HDRLEN (20) */
    auto *nla = reinterpret_cast<struct nlattr *>(req_buf.data() + NLMSG_HDRLEN + GENL_HDRLEN);
    nla->nla_len  = NLA_HDRLEN + name_len;
    nla->nla_type = CTRL_ATTR_FAMILY_NAME;
    memcpy(req_buf.data() + NLMSG_HDRLEN + GENL_HDRLEN + NLA_HDRLEN, family_name, name_len);

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;

    if (sendto(sock, req_buf.data(), msg_len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        SWSS_LOG_ERROR("Failed to send CTRL_CMD_GETFAMILY: %s", strerror(errno));
        return -1;
    }

    /* Parse response */
    char buf[4096];
    ssize_t len = recv(sock, buf, sizeof(buf), 0);
    if (len < 0)
    {
        SWSS_LOG_ERROR("Failed to recv CTRL_CMD_GETFAMILY response: %s", strerror(errno));
        return -1;
    }

    auto *nlh = reinterpret_cast<struct nlmsghdr *>(buf);
    if (!NLMSG_OK(nlh, (uint32_t)len) || nlh->nlmsg_type == NLMSG_ERROR)
    {
        SWSS_LOG_ERROR("CTRL_CMD_GETFAMILY failed (family '%s' not found)", family_name);
        return -1;
    }

    /* Skip nlmsghdr + genlmsghdr, parse NLA attributes */
    int payload_len = len - NLMSG_HDRLEN - GENL_HDRLEN;
    auto *nla = reinterpret_cast<struct nlattr *>(buf + NLMSG_HDRLEN + GENL_HDRLEN);

    while (payload_len > 0 && payload_len >= (int)NLA_HDRLEN)
    {
        if (nla->nla_type == CTRL_ATTR_FAMILY_ID)
        {
            uint16_t family_id = *reinterpret_cast<uint16_t *>(
                reinterpret_cast<char *>(nla) + NLA_HDRLEN);
            return family_id;
        }
        int nla_total = NLA_ALIGN(nla->nla_len);
        payload_len -= nla_total;
        nla = reinterpret_cast<struct nlattr *>(
            reinterpret_cast<char *>(nla) + nla_total);
    }

    SWSS_LOG_ERROR("CTRL_ATTR_FAMILY_ID not found in response");
    return -1;
}

/**
 * @brief Send IPFIX data to sonic_stel kernel module via genetlink.
 *
 * Constructs a genetlink message with SONIC_STEL_CMD_SEND_IPFIX command
 * and SONIC_STEL_ATTR_IPFIX_DATA attribute containing raw IPFIX payload.
 *
 * @return 0 on success, -1 on failure
 */
static int send_ipfix_via_genl(int sock, uint16_t family_id,
                                const uint8_t *data, size_t data_len)
{
    /* Build genetlink message */
    size_t nla_len = NLA_HDRLEN + data_len;
    size_t nla_total = NLA_ALIGN(nla_len);
    size_t msg_len = NLMSG_HDRLEN + GENL_HDRLEN + nla_total;

    std::vector<uint8_t> msg(msg_len, 0);

    /* nlmsghdr */
    auto *nlh = reinterpret_cast<struct nlmsghdr *>(msg.data());
    nlh->nlmsg_len   = msg_len;
    nlh->nlmsg_type  = family_id;
    nlh->nlmsg_flags = NLM_F_REQUEST;
    nlh->nlmsg_seq   = 0;
    nlh->nlmsg_pid   = 0;

    /* genlmsghdr */
    auto *genl = reinterpret_cast<struct genlmsghdr *>(msg.data() + NLMSG_HDRLEN);
    genl->cmd     = SONIC_STEL_CMD_SEND_IPFIX;
    genl->version = 1;

    /* NLA: IPFIX data */
    auto *nla = reinterpret_cast<struct nlattr *>(msg.data() + NLMSG_HDRLEN + GENL_HDRLEN);
    nla->nla_len  = nla_len;
    nla->nla_type = SONIC_STEL_ATTR_IPFIX_DATA;
    memcpy(reinterpret_cast<char *>(nla) + NLA_HDRLEN, data, data_len);

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;

    if (sendto(sock, msg.data(), msg_len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        SWSS_LOG_ERROR("Failed to send IPFIX data via genetlink: %s", strerror(errno));
        return -1;
    }

    return 0;
}

/* ────── IPFIX data record generation ────── */

/**
 * Helper to write big-endian values into a buffer at a given offset.
 */
static void put_u16_be(std::vector<uint8_t> &buf, size_t offset, uint16_t val)
{
    buf[offset]     = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>(val & 0xFF);
}

static void put_u32_be(std::vector<uint8_t> &buf, size_t offset, uint32_t val)
{
    buf[offset]     = static_cast<uint8_t>((val >> 24) & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[offset + 2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[offset + 3] = static_cast<uint8_t>(val & 0xFF);
}

static void put_u64_be(std::vector<uint8_t> &buf, size_t offset, uint64_t val)
{
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
    size_t num_counters = counter_values.size();
    // Data set: set_header(4) + timestamp(8) + counters(8*N)
    size_t data_set_len = 4 + 8 + num_counters * 8;
    // IPFIX message: header(16) + data_set
    size_t msg_len = 16 + data_set_len;

    std::vector<uint8_t> msg(msg_len, 0);
    size_t off = 0;

    /* IPFIX Header */
    put_u16_be(msg, off, 0x000a);      off += 2;  // Version
    put_u16_be(msg, off, msg_len);     off += 2;  // Message Length
    auto now_s = static_cast<uint32_t>(timestamp_ns / 1000000000ULL);
    put_u32_be(msg, off, now_s);       off += 4;  // Export Timestamp (seconds)
    put_u32_be(msg, off, seq_num);     off += 4;  // Sequence Number
    put_u32_be(msg, off, 0);           off += 4;  // Observation Domain ID = 0

    /* Data Set Header */
    put_u16_be(msg, off, template_id); off += 2;  // Set ID = Template ID
    put_u16_be(msg, off, data_set_len); off += 2; // Set Length

    /* observationTimeNanoseconds */
    put_u64_be(msg, off, timestamp_ns); off += 8;

    /* Counter values */
    for (auto val : counter_values)
    {
        put_u64_be(msg, off, val);     off += 8;
    }

    return msg;
}

/* ────── Stream telemetry thread ────── */

void SwitchStateBase::stelWorkerThread(
    _In_ uint32_t poll_interval_us,
    _In_ uint16_t template_id,
    _In_ size_t num_counters)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("STEL worker thread started: poll_interval=%u us, template_id=%u, counters=%zu",
                    poll_interval_us, template_id, num_counters);

    /* Open genetlink socket */
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if (sock < 0)
    {
        SWSS_LOG_ERROR("Failed to open NETLINK_GENERIC socket: %s", strerror(errno));
        return;
    }

    struct sockaddr_nl local;
    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_pid    = 0; // let kernel assign

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
        SWSS_LOG_ERROR("Failed to bind netlink socket: %s", strerror(errno));
        close(sock);
        return;
    }

    /* Resolve sonic_stel family */
    int family_id = resolve_genl_family(sock, SONIC_STEL_FAMILY_NAME);
    if (family_id < 0)
    {
        SWSS_LOG_ERROR("sonic_stel family not found - is the kernel module loaded?");
        close(sock);
        return;
    }

    SWSS_LOG_NOTICE("Resolved sonic_stel family_id=%d", family_id);

    /* Generate fake counter data in a loop */
    uint32_t seq_num = 0;
    std::vector<uint64_t> counters(num_counters, 0);

    while (m_stelRunning)
    {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        uint64_t timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

        /* Increment counters with fake data (monotonically increasing) */
        for (size_t i = 0; i < num_counters; i++)
        {
            counters[i] += (i + 1) * 10; // Simple pattern: counter[i] += (i+1)*10 per tick
        }

        auto ipfix_msg = build_ipfix_data_message(template_id, seq_num, timestamp_ns, counters);

        if (send_ipfix_via_genl(sock, static_cast<uint16_t>(family_id),
                                ipfix_msg.data(), ipfix_msg.size()) < 0)
        {
            SWSS_LOG_WARN("Failed to send IPFIX data (seq=%u), will retry", seq_num);
        }

        seq_num++;
        std::this_thread::sleep_for(std::chrono::microseconds(poll_interval_us));
    }

    close(sock);
    SWSS_LOG_NOTICE("STEL worker thread stopped");
}

sai_status_t SwitchStateBase::startStelStream(
    _In_ uint32_t poll_interval_us,
    _In_ uint16_t template_id,
    _In_ size_t num_counters)
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
        poll_interval_us, template_id, num_counters);

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
