#include "MACsec.h"
#include "SwitchStateBase.h"
#include "SelectableFd.h"

#include <swss/logger.h>
#include <swss/select.h>
#include <swss/exec.h>

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>

#include <regex>
#include <cstring>
#include <system_error>
#include <cinttypes>

using namespace saivs;

#define MACSEC_ETHER_TYPE (0x88e5)
#define EAPOL_ETHER_TYPE (0x888e)
#define MAC_ADDRESS_SIZE (6)
#define ETH_FRAME_BUFFER_SIZE (0x4000)

MACsecForwarder::MACsecForwarder(
    const std::string &macsec_interface_name,
    int tapfd,
    int vethfd) : m_tapfd(tapfd),
                  m_vethfd(vethfd),
                  m_macsec_interface_name(macsec_interface_name),
                  m_run_thread(true)
{
    SWSS_LOG_ENTER();

    m_macsecfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (m_macsecfd < 0)
    {
        const std::string error_msg =
            "failed to open macsec socket " + m_macsec_interface_name + ", errno: " + std::to_string(errno);
        SWSS_LOG_ERROR("%s", error_msg.c_str());
        throw std::system_error(errno, std::generic_category(), error_msg);
    }

    struct sockaddr_ll sock_address;
    memset(&sock_address, 0, sizeof(sock_address));
    sock_address.sll_family = PF_PACKET;
    sock_address.sll_protocol = htons(ETH_P_ALL);
    sock_address.sll_ifindex = if_nametoindex(m_macsec_interface_name.c_str());
    if (sock_address.sll_ifindex == 0)
    {
        const std::string error_msg =
            "failed to get interface index for" + m_macsec_interface_name;
        SWSS_LOG_ERROR("%s", error_msg);
        close(m_macsecfd);
        throw std::runtime_error(error_msg);
    }

    if (SwitchStateBase::promisc(m_macsec_interface_name.c_str()))
    {
        const std::string error_msg =
            "promisc failed on " + m_macsec_interface_name;
        SWSS_LOG_ERROR("%s", error_msg);
        close(m_macsecfd);
        throw std::runtime_error(error_msg);
    }

    if (bind(
            m_macsecfd,
            (struct sockaddr *)&sock_address,
            sizeof(sock_address)) < 0)
    {
        const std::string error_msg =
            "bind failed on " + m_macsec_interface_name;
        SWSS_LOG_ERROR("%s", error_msg);
        close(m_macsecfd);
        throw std::runtime_error(error_msg);
    }

    m_forward_thread =
        std::make_shared<std::thread>(&MACsecForwarder::forward, this);

    SWSS_LOG_NOTICE(
        "setup MACsec forward rule for %s succeeded",
        m_macsec_interface_name.c_str());
}

MACsecForwarder::~MACsecForwarder()
{
    SWSS_LOG_ENTER();

    m_run_thread = false;
    m_exit_event.notify();
    m_forward_thread->join();
    int err = close(m_macsecfd);
    if (err)
    {
        SWSS_LOG_ERROR(
            "failed to remove macsec device: %s, err: %d",
            m_macsec_interface_name.c_str(),
            err);
    }
}

int MACsecForwarder::get_macsecfd() const
{
    SWSS_LOG_ENTER();

    return m_macsecfd;
}

void MACsecForwarder::forward()
{
    SWSS_LOG_ENTER();

    unsigned char buffer[ETH_FRAME_BUFFER_SIZE];

    swss::Select s;
    SelectableFd fd(m_macsecfd);

    s.addSelectable(&m_exit_event);
    s.addSelectable(&fd);

    while (m_run_thread)
    {
        swss::Selectable *sel = NULL;

        int result = s.select(&sel);

        if (result != swss::Select::OBJECT)
        {
            SWSS_LOG_ERROR(
                "selectable failed: %d, ending thread for %s",
                result,
                m_macsec_interface_name.c_str());
            return;
        }

        if (sel == &m_exit_event) // thread end event
            break;

        ssize_t size = read(m_macsecfd, buffer, sizeof(buffer));

        if (size < 0)
        {
            SWSS_LOG_ERROR(
                "failed to read from macsec device %s fd %d, errno(%d): %s",
                m_macsec_interface_name.c_str(),
                m_macsecfd,
                errno,
                strerror(errno));
            if (errno == EBADF)
            {
                // bad file descriptor, just close the thread
                SWSS_LOG_NOTICE(
                    "ending thread for macsec device %s",
                    m_macsec_interface_name.c_str());
                return;
            }
            continue;
        }
        auto mac_hdr = static_cast<ethhdr *>(
            static_cast<void *>(buffer));
        int forward_fd =
            ntohs(mac_hdr->h_proto) == MACSEC_ETHER_TYPE
                ? m_vethfd
                : m_tapfd;
        if (write(forward_fd, buffer, static_cast<int>(size)) < 0)
        {
            if (errno != ENETDOWN && errno != EIO)
            {
                SWSS_LOG_ERROR(
                    "failed to write to macsec device %s fd %d, errno(%d): %s",
                    m_macsec_interface_name.c_str(),
                    m_macsecfd,
                    errno,
                    strerror(errno));
            }
            if (errno == EBADF)
            {
                // bad file descriptor, just end thread
                SWSS_LOG_ERROR(
                    "ending thread for macsec device %s fd %d",
                    m_macsec_interface_name.c_str(),
                    m_macsecfd);
                return;
            }
            continue;
        }
    }
    SWSS_LOG_NOTICE(
        "ending thread proc for %s",
        m_macsec_interface_name.c_str());
}

MACsecFilter::MACsecFilter(
    const std::string &macsec_interface_name,
    int macsecfd) : m_macsecfd(macsecfd),
                    m_macsec_interface_name(macsec_interface_name)
{
    SWSS_LOG_ENTER();
}

TrafficFilter::FilterStatus MACsecFilter::execute(
    void *buffer,
    ssize_t &length)
{
    SWSS_LOG_ENTER();

    auto mac_hdr = static_cast<const ethhdr *>(buffer);
    if (ntohs(mac_hdr->h_proto) == EAPOL_ETHER_TYPE)
    {
        // EAPOL traffic bypass macsec interface
        // and directly forward to tap interface
        return TrafficFilter::CONTINUE;
    }

    return forward(buffer, length);
}

MACsecEgressFilter::MACsecEgressFilter(
    const std::string &macsec_interface_name,
    int macsecfd) : MACsecFilter(macsec_interface_name, macsecfd)
{}

TrafficFilter::FilterStatus MACsecEgressFilter::forward(
    void *buffer,
    ssize_t &length)
{
    if (write(m_macsecfd, buffer, length) < 0)
    {
        if (errno != ENETDOWN && errno != EIO)
        {
            SWSS_LOG_ERROR(
                "failed to write to macsec device %s fd %d, errno(%d): %s",
                m_macsec_interface_name.c_str(),
                m_macsecfd,
                errno,
                strerror(errno));
        }
        if (errno == EBADF)
        {
            // bad file descriptor, just end thread
            SWSS_LOG_ERROR(
                "ending thread for macsec device %s fd %d",
                m_macsec_interface_name.c_str(),
                m_macsecfd);
            return TrafficFilter::ERROR;
        }
    }
    return TrafficFilter::TERMINATE;
}

MACsecIngressFilter::MACsecIngressFilter(
    const std::string &macsec_interface_name,
    int macsecfd) : MACsecFilter(macsec_interface_name, macsecfd)
{}

TrafficFilter::FilterStatus MACsecIngressFilter::forward(
    void *buffer,
    ssize_t &length)
{
    return TrafficFilter::TERMINATE;
}

bool MACsecManager::add_macsec_manager(
    const std::string &macsec_interface,
    std::shared_ptr<HostInterfaceInfo> info)
{
    SWSS_LOG_ENTER();

    auto itr = m_managers.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(macsec_interface),
        std::forward_as_tuple());
    if (!itr.second)
    {
        SWSS_LOG_ERROR(
            "macsec manager for %s was existed",
            macsec_interface.c_str());
        return false;
    }
    auto &manager = itr.first->second;
    manager.m_info = info;
    manager.m_forwarder.reset(
        new MACsecForwarder(
            macsec_interface,
            manager.m_info->m_tapfd,
            manager.m_info->m_packet_socket));
    manager.m_ingress_filter.reset(
        new MACsecIngressFilter(
            macsec_interface,
            manager.m_forwarder->get_macsecfd()));
    manager.m_egress_filter.reset(
        new MACsecEgressFilter(
            macsec_interface,
            manager.m_forwarder->get_macsecfd()));
    manager.m_info->installEth2TapFilter(
        FilterPriority::MACSEC_FILTER,
        manager.m_ingress_filter);
    manager.m_info->installTap2EthFilter(
        FilterPriority::MACSEC_FILTER,
        manager.m_egress_filter);
    return true;
}

bool MACsecManager::delete_macsec_manager(const std::string &macsec_interface)
{
    SWSS_LOG_ENTER();

    auto itr = m_managers.find(macsec_interface);
    if (itr == m_managers.end())
    {
        SWSS_LOG_ERROR(
            "macsec manager for %s isn't existed",
            macsec_interface.c_str());
        return false;
    }
    auto &manager = itr->second;
    manager.m_info->uninstallEth2TapFilter(manager.m_ingress_filter);
    manager.m_info->uninstallTap2EthFilter(manager.m_egress_filter);
    m_managers.erase(itr);
    return true;
}


std::string MACsecManager::shellquote(const std::string &str)
{
    static const std::regex re("([$`\"\\\n])");
    return "\"" + std::regex_replace(str, re, "\\$1") + "\"";
}

bool MACsecManager::exec(const std::string &command)
{
    SWSS_LOG_ENTER();

    std::string res;
    if (swss::exec(command, res) != 0)
    {
        SWSS_LOG_DEBUG("FAIL %s", command.c_str());
        return false;
    }
    return true;
}

bool MACsecManager::create_macsec_port(const MACsecAttr &attr)
{
    // Create MACsec Port
    // $ ip link add link <VETH_NAME> name <MACSEC_NAME> type macsec sci <SCI>
    // $ ip link set dev <MACSEC_NAME> up
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip link add link "
        << shellquote(attr.m_veth_name)
        << " name "
        << shellquote(attr.m_macsec_name)
        << " type macsec sci "
        << attr.m_sci;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    if (!exec(ostream.str()))
    {
        return false;
    }
    ostream.str("");
    ostream
        << "ip link set dev "
        << shellquote(attr.m_macsec_name)
        << " up";
    if (!exec(ostream.str()))
    {
        return false;
    }
    return add_macsec_manager(attr.m_macsec_name, attr.m_info);
}

bool MACsecManager::create_macsec_egress_sa(const MACsecAttr &attr)
{
    // Create MACsec Egress SA
    // $ ip macsec add macsec0 tx sa 0 pn 1 on key 00 <SAK>
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec add "
        << shellquote(attr.m_macsec_name)
        << " tx sa "
        << attr.m_an
        << " pn "
        << attr.m_pn
        << " on key 00 "
        << attr.m_sak;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return exec(ostream.str());
}

bool MACsecManager::create_macsec_ingress_sc(const MACsecAttr &attr)
{
    // Create MACsec Ingress SC
    // $ ip macsec add macsec0 rx sci <SCI>
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec add "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " on";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return exec(ostream.str());
}

bool MACsecManager::create_macsec_ingress_sa(const MACsecAttr &attr)
{
    // Create MACsec Ingress SA
    // $ ip macsec add macsec0 rx sci <SCI> sa <SA> pn <PN> on key 00 <SAK>
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec add "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an
        << " pn "
        << attr.m_pn
        << " on key 00 "
        << attr.m_sak;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return exec(ostream.str());
}

bool MACsecManager::enable_macsec(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    create_macsec_port(attr);
    if (attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        create_macsec_egress_sa(attr);
    }
    else
    {
        create_macsec_ingress_sc(attr);
        create_macsec_ingress_sa(attr);
    }

    return SAI_STATUS_SUCCESS;
}

bool MACsecManager::delete_macsec_port(const MACsecAttr &attr)
{
    // Delete MACsec Port
    // $ ip link del link <VETH_NAME> name <MACSEC_NAME> type macsec
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip link del link "
        << shellquote(attr.m_veth_name)
        << " name "
        << shellquote(attr.m_macsec_name)
        << " type macsec";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    if (!exec(ostream.str()))
    {
        return false;
    }
    return delete_macsec_manager(attr.m_macsec_name);
}

bool MACsecManager::delete_macsec_egress_sa(const MACsecAttr &attr)
{
    // Delete MACsec Egress SA
    // $ ip macsec set macsec0 tx sa 0 off
    // $ ip macsec del macsec0 tx sa 0
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec set "
        << shellquote(attr.m_macsec_name)
        << " tx sa "
        << attr.m_an
        << " off";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    exec(ostream.str());
    ostream.str("");
    ostream
        << "ip macsec del "
        << shellquote(attr.m_macsec_name)
        << " tx sa "
        << attr.m_an;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return exec(ostream.str());
}

bool MACsecManager::delete_macsec_ingress_sc(const MACsecAttr &attr)
{
    // Delete MACsec Ingress SC
    // $ ip macsec set macsec0 rx sci <SCI> off
    // $ ip macsec del macsec0 rx sci <SCI>
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec add "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " off";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    exec(ostream.str());
    ostream.str("");
    ostream
        << "ip macsec del "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return exec(ostream.str());
}

bool MACsecManager::delete_macsec_ingress_sa(const MACsecAttr &attr)
{
    // Delete MACsec Ingress SA
    // $ ip macsec set macsec0 rx sci <SCI> sa <SA> off
    // $ ip macsec del macsec0 rx sci <SCI> sa <SA>
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec set "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an
        << " off";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    exec(ostream.str());
    ostream.str("");
    ostream
        << "ip macsec del "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return exec(ostream.str());
}
