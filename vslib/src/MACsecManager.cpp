#include "MACsecManager.h"
#include "MACsecIngressFilter.h"
#include "MACsecEgressFilter.h"

#include <swss/logger.h>
#include <swss/exec.h>

#include <regex>
#include <cstring>
#include <system_error>
#include <cinttypes>

using namespace saivs;

MACsecManager::MACsecManager()
{
    cleanup_macsec_device();
}

MACsecManager::~MACsecManager()
{
    cleanup_macsec_device();
}

// Create MACsec Port
// $ ip link add link <VETH_NAME> name <MACSEC_NAME> type macsec sci <SCI>
// $ ip link set dev <MACSEC_NAME> up
// MACsec egress SC will be automatically create when the MACsec port is created
bool MACsecManager::create_macsec_port(
    _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (is_macsec_device_existing(attr.m_macsec_name))
    {
        return true;
    }

    std::ostringstream ostream;
    ostream
        << "ip link add link "
        << shellquote(attr.m_veth_name)
        << " name "
        << shellquote(attr.m_macsec_name)
        << " type macsec "
        << " sci " << attr.m_sci
        << " encrypt " << (attr.m_encryption_enable ? " on " : " off ")
        << " && ip link set dev "
        << shellquote(attr.m_macsec_name)
        << " up";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    if (!exec(ostream.str()))
    {
        return false;
    }

    return add_macsec_manager(attr.m_macsec_name, attr.m_info);
}

// Create MACsec Egress SA
// $ ip macsec add <MACSEC_NAME> tx sa <AN> pn <PN> on key <AUTH_KEY> <SAK> &&
// ip link set link <VETH_NAME> name <MACSEC_NAME> type macsec encodingsa <AN>
bool MACsecManager::create_macsec_egress_sa(
    _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (is_macsec_sa_existing(
        attr.m_macsec_name,
        attr.m_direction,
        attr.m_sci,
        attr.m_an))
    {
        return true;
    }

    if (!create_macsec_port(attr))
    {
        SWSS_LOG_WARN(
            "Cannot create MACsec device %s",
            attr.m_macsec_name.c_str());
        return false;
    }

    std::ostringstream ostream;
    ostream
        << "ip macsec add "
        << shellquote(attr.m_macsec_name)
        << " tx sa "
        << attr.m_an
        << " pn "
        << attr.m_pn
        << " on key "
        << attr.m_auth_key
        << " "
        << attr.m_sak
        << " && ip link set link "
        << attr.m_veth_name
        << " name "
        << attr.m_macsec_name
        << " type macsec encodingsa "
        << attr.m_an;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

bool MACsecManager::create_macsec_sc(
    _In_ const MACsecAttr &attr)
{
    if (attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        if (!create_macsec_port(attr))
        {
            SWSS_LOG_WARN(
                "Cannot create MACsec egress SC %s at the device %s",
                attr.m_sci.c_str(),
                attr.m_macsec_name.c_str());
            return false;
        }
    }
    else
    {
        if (!create_macsec_ingress_sc(attr))
        {
            SWSS_LOG_WARN(
                "Cannot create MACsec ingress SC %s at the device %s",
                attr.m_sci.c_str(),
                attr.m_macsec_name.c_str());
            return false;
        }
    }
    return true;
}

// Create MACsec Ingress SC
// $ ip macsec add <MACSEC_NAME> rx sci <SCI>
bool MACsecManager::create_macsec_ingress_sc(
    _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (is_macsec_sc_existing(
        attr.m_macsec_name,
        attr.m_direction,
        attr.m_sci))
    {
        return true;
    }

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

// Create MACsec Ingress SA
// $ ip macsec add <MACSEC_NAME> rx sci <SCI> sa <SA> pn <PN> on key <AUTH_KEY> <SAK>
bool MACsecManager::create_macsec_ingress_sa(
    _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (is_macsec_sa_existing(
        attr.m_macsec_name,
        attr.m_direction,
        attr.m_sci,
        attr.m_an))
    {
        return true;
    }

    if (!create_macsec_ingress_sc(attr))
    {
        SWSS_LOG_WARN(
            "Cannot create MACsec ingress SC %s at the device %s.",
            attr.m_sci.c_str(),
            attr.m_macsec_name.c_str());
        return false;
    }

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
        << " on key "
        << attr.m_auth_key
        << " "
        << attr.m_sak;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

bool MACsecManager::create_macsec_sa(
    _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {

        if (!create_macsec_egress_sa(attr))
        {
            SWSS_LOG_WARN(
                "Cannot create MACsec egress SA %s:%u at the device %s.",
                attr.m_sci.c_str(),
                static_cast<std::uint32_t>(attr.m_an),
                attr.m_macsec_name.c_str());
            return false;
        }

    }
    else
    {

        if (!create_macsec_ingress_sa(attr))
        {
            SWSS_LOG_WARN(
                "Cannot create MACsec ingress SA %s:%u at the device %s.",
                attr.m_sci.c_str(),
                static_cast<std::uint32_t>(attr.m_an),
                attr.m_macsec_name.c_str());
            return false;
        }

    }

    return true;
}

// Delete MACsec Port
// $ ip link del link <VETH_NAME> name <MACSEC_NAME> type macsec
bool MACsecManager::delete_macsec_port(
    _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (!is_macsec_device_existing(attr.m_macsec_name))
    {
        // This macsec device has been deleted
        return true;
    }

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

    return delete_macsec_traffic_manager(attr.m_macsec_name);
}

// Delete MACsec Egress SA
// $ ip macsec set <MACSEC_NAME> tx sa 0 off
// $ ip macsec del <MACSEC_NAME> tx sa 0
bool MACsecManager::delete_macsec_egress_sa(
    _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (!is_macsec_sa_existing(
        attr.m_macsec_name,
        attr.m_direction,
        attr.m_sci, attr.m_an))
    {
        return true;
    }

    std::ostringstream ostream;
    ostream
        << "ip macsec set "
        << shellquote(attr.m_macsec_name)
        << " tx sa "
        << attr.m_an
        << " off"
        << " && ip macsec del "
        << shellquote(attr.m_macsec_name)
        << " tx sa "
        << attr.m_an
        ;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

// Delete MACsec Ingress SC
// $ ip macsec set <MACSEC_NAME> rx sci <SCI> off
// $ ip macsec del <MACSEC_NAME> rx sci <SCI>
bool MACsecManager::delete_macsec_ingress_sc(
    _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (!is_macsec_sc_existing(
        attr.m_macsec_name,
        attr.m_direction,
        attr.m_sci))
    {
        return true;
    }

    std::ostringstream ostream;
    ostream
        << "ip macsec add "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " off"
        << " && ip macsec del "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

// Delete MACsec Ingress SA
// $ ip macsec set <MACSEC_NAME> rx sci <SCI> sa <SA> off
// $ ip macsec del <MACSEC_NAME> rx sci <SCI> sa <SA>
bool MACsecManager::delete_macsec_ingress_sa(
    _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (!is_macsec_sa_existing(
        attr.m_macsec_name,
        attr.m_direction,
        attr.m_sci,
        attr.m_an))
    {
        return true;
    }

    std::ostringstream ostream;
    ostream
        << "ip macsec set "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an
        << " off"
        << " && ip macsec del "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

bool MACsecManager::get_macsec_sa_pn(
    _In_ const MACsecAttr &attr,
    _Out_ sai_uint64_t &pn) const
{
    SWSS_LOG_ENTER();

    pn = 1;

    std::string macsec_sa_info;

    if (!get_macsec_sa_info(
        attr.m_macsec_name,
        attr.m_direction,
        attr.m_sci,
        attr.m_an,
        macsec_sa_info))
    {
        return false;
    }

    // Here is an example of MACsec SA
    //     0: PN 28, state on, key ebe9123ecbbfd96bee92c8ab01000000
    // Use pattern 'PN\s*(\d+)'
    // to extract packet number from MACsec SA

    const std::regex pattern("PN\\s*(\\d+)");
    std::smatch matches;

    if (std::regex_search(macsec_sa_info, matches, pattern))
    {

        if (matches.size() != 2)
        {
            SWSS_LOG_ERROR(
                "Wrong match result %s in %s",
                matches.str().c_str(),
                macsec_sa_info.c_str());
            return false;
        }

        std::istringstream iss(matches[1].str());
        iss >> pn;
        return true;
    }
    else
    {

        SWSS_LOG_WARN(
            "The packet number isn't in the MACsec SA %s:%u at the device %s.",
            attr.m_sci.c_str(),
            static_cast<std::uint32_t>(attr.m_an),
            attr.m_macsec_name.c_str());

        return false;
    }

}

bool MACsecManager::add_macsec_manager(
    _In_ const std::string &macsec_interface,
    _In_ std::shared_ptr<HostInterfaceInfo> info)
{
    SWSS_LOG_ENTER();

    auto itr = m_macsec_traffic_managers.emplace(
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
            manager.m_info->m_tapfd));
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

bool MACsecManager::delete_macsec_traffic_manager(
    _In_ const std::string &macsec_interface)
{
    SWSS_LOG_ENTER();

    auto itr = m_macsec_traffic_managers.find(macsec_interface);

    if (itr == m_macsec_traffic_managers.end())
    {
        SWSS_LOG_ERROR(
            "macsec manager for %s isn't existed",
            macsec_interface.c_str());
        return false;
    }

    auto &manager = itr->second;
    manager.m_info->uninstallEth2TapFilter(manager.m_ingress_filter);
    manager.m_info->uninstallTap2EthFilter(manager.m_egress_filter);
    m_macsec_traffic_managers.erase(itr);

    return true;
}

// Query MACsec session
// $ ip macsec show <MACSEC_NAME>
bool MACsecManager::get_macsec_device_info(
    _In_ const std::string &macsec_device,
    _Out_ std::string &info) const
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream
        << "ip macsec show "
        << shellquote(macsec_device);

    return exec(ostream.str(), info);
}

bool MACsecManager::is_macsec_device_existing(
    _In_ const std::string &macsec_device) const
{
    SWSS_LOG_ENTER();

    std::string macsec_info;

    return get_macsec_device_info(macsec_device, macsec_info);
}

bool MACsecManager::get_macsec_sc_info(
    _In_ const std::string &macsec_device,
    _In_ sai_int32_t direction,
    _In_ const std::string &sci,
    _Out_ std::string &info) const
{
    SWSS_LOG_ENTER();

    std::string macsec_info;

    if (!get_macsec_device_info(macsec_device, macsec_info))
    {
        SWSS_LOG_WARN(
            "MACsec device %s is nonexisting",
            macsec_device.c_str());
        return false;
    }

    const char *str_direction = nullptr;

    if (direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        str_direction = "TXSC";
    }
    else
    {
        str_direction = "RXSC";
    }

    // Here is an example of MACsec device information
    // cipher suite: GCM-AES-128, using ICV length 16
    // TXSC: fe5400bd9b360001 on SA 0
    //     0: PN 84, state on, key ebe9123ecbbfd96bee92c8ab01000000
    //     1: PN 0, state on, key ebe9123ecbbfd96bee92c8ab01000001
    // RXSC: 5254001234560001, state on
    //     0: PN 28, state on, key ebe9123ecbbfd96bee92c8ab01000000
    // Use pattern '<DIRECTION>:\s*<SCI>[ \w,]+\n?(?:\s*\d:[,\w ]+\n?)*'
    // to extract MACsec SC information
    std::ostringstream ostream;
    ostream
        << str_direction
        << ":\\s*"
        << sci
        << "[ \\w,]+\n?(?:\\s*\\d:[,\\w ]+\n?)*";

    const std::regex pattern(ostream.str());
    std::smatch matches;

    if (std::regex_search(macsec_info, matches, pattern))
    {
        info = matches[0].str();
        return true;
    }
    else
    {
        return false;
    }

}

bool MACsecManager::is_macsec_sc_existing(
    _In_ const std::string &macsec_device,
    _In_ sai_int32_t direction,
    _In_ const std::string &sci) const
{
    SWSS_LOG_ENTER();

    std::string macsec_sc_info;

    return get_macsec_sc_info(
        macsec_device,
        direction,
        sci,
        macsec_sc_info);
}

bool MACsecManager::get_macsec_sa_info(
    _In_ const std::string &macsec_device,
    _In_ sai_int32_t direction,
    _In_ const std::string &sci,
    _In_ macsec_an_t an,
    _Out_ std::string &info) const
{
    SWSS_LOG_ENTER();

    std::string macsec_sc_info;

    if (!get_macsec_sc_info(
        macsec_device,
        direction,
        sci,
        macsec_sc_info))
    {

        SWSS_LOG_WARN(
            "The MACsec SC %s at the device %s is nonexisting.",
            sci.c_str(),
            macsec_device.c_str());

        return false;
    }

    // Here is an example of MACsec SA
    //     0: PN 28, state on, key ebe9123ecbbfd96bee92c8ab01000000
    // Use pattern '\s*<AN>:\s*PN\s*\d+[,\w ]+\n?'
    // to extract MACsec SA information from MACsec SC
    std::ostringstream ostream;
    ostream
        << "\\s*"
        << an
        << ":\\s*PN\\s*\\d+[,\\w ]+\n?";

    const std::regex pattern(ostream.str());
    std::smatch matches;

    if (std::regex_search(macsec_sc_info, matches, pattern))
    {
        info = matches[0].str();
        return true;
    }
    else
    {
        return false;
    }

}

bool MACsecManager::is_macsec_sa_existing(
    _In_ const std::string &macsec_device,
    _In_ sai_int32_t direction,
    _In_ const std::string &sci,
    _In_ macsec_an_t an) const
{
    SWSS_LOG_ENTER();

    std::string macsec_sa_info;

    return get_macsec_sa_info(
        macsec_device,
        direction,
        sci,
        an,
        macsec_sa_info);
}

void MACsecManager::cleanup_macsec_device() const
{
    SWSS_LOG_ENTER();

    std::string macsec_infos;

    if (!exec("ip macsec show", macsec_infos))
    {
        SWSS_LOG_THROW("Cannot show MACsec ports");
    }

    // Here is an example of MACsec device informations
    // 2774: macsec0: protect on validate strict sc off sa off encrypt on send_sci on end_station off scb off replay on window 0
    //     cipher suite: GCM-AES-128, using ICV length 16
    //     TXSC: fe5400409b920001 on SA 0
    // 2775: macsec1: protect on validate strict sc off sa off encrypt on send_sci on end_station off scb off replay on window 0
    //     cipher suite: GCM-AES-128, using ICV length 16
    //     TXSC: fe5400409b920001 on SA 0
    // 2776: macsec2: protect on validate strict sc off sa off encrypt on send_sci on end_station off scb off replay on window 0
    //     cipher suite: GCM-AES-128, using ICV length 16
    //     TXSC: fe5400409b920001 on SA 0
    // Use pattern : '^\d+:\s*(\w+):' to extract all MACsec interface names
    const std::regex pattern("^\\d+:\\s*(\\w+):");
    std::smatch matches;
    std::string::const_iterator search_pos(macsec_infos.cbegin());

    while(std::regex_search(search_pos, macsec_infos.cend(), matches, pattern))
    {
        std::ostringstream ostream;
        ostream
            << "ip link del "
            << matches[1].str();
        if (!exec(ostream.str()))
        {
            SWSS_LOG_WARN(
                "Cannot cleanup MACsec interface %s",
                matches[1].str().c_str());
        }
        search_pos = matches.suffix().first;
    }

}

std::string MACsecManager::shellquote(
    _In_ const std::string &str) const
{
    SWSS_LOG_ENTER();

    static const std::regex re("([$`\"\\\n])");

    return "\"" + std::regex_replace(str, re, "\\$1") + "\"";
}

bool MACsecManager::exec(
    _In_ const std::string &command,
    _Out_ std::string &output) const
{
    SWSS_LOG_ENTER();

    if (swss::exec(command, output) != 0)
    {
        SWSS_LOG_DEBUG("FAIL %s", command.c_str());
        return false;
    }

    return true;
}

bool MACsecManager::exec(
    _In_ const std::string &command) const
{
    SWSS_LOG_ENTER();

    std::string res;

    return exec(command, res);
}
