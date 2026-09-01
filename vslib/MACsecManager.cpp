#include "MACsecManager.h"
#include "MACsecIngressFilter.h"
#include "MACsecEgressFilter.h"

#include <swss/logger.h>
#include <swss/exec.h>

#include <unistd.h>

#include <regex>
#include <cstring>
#include <system_error>
#include <cinttypes>
#include <string>

using namespace saivs;

static constexpr macsec_an_t MAX_MACSEC_SA_NUMBER = 3;

MACsecManager::MACsecManager()
{
    SWSS_LOG_ENTER();

    // empty
}

MACsecManager::~MACsecManager()
{
    SWSS_LOG_ENTER();

    // empty
}

bool MACsecManager::create_macsec_port(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    // Linux MACsec driver would not differentiate the Ingress and Egress port,
    // We just need to create the Linux MACsec device when the egress port was created
    // And we assume that creating the ingress port is always success.
    if (attr.m_direction == SAI_MACSEC_DIRECTION_INGRESS)
    {
        return true;
    }

    return add_macsec_manager(attr.m_macsecName, attr.m_info);
}

bool MACsecManager::create_macsec_sc(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (is_macsec_sc_existing( attr.m_macsecName, attr.m_direction, attr.m_sci))
    {
        SWSS_LOG_WARN(
                "MACsec SC %s at the device %s has been created",
                attr.m_sci.c_str(),
                attr.m_macsecName.c_str());

        return true;
    }

    if (attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {

        if (!create_macsec_egress_sc(attr))
        {
            SWSS_LOG_WARN(
                    "Cannot create MACsec egress SC %s at the device %s",
                    attr.m_sci.c_str(),
                    attr.m_macsecName.c_str());

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
                    attr.m_macsecName.c_str());

            return false;
        }
    }

    return true;
}

bool MACsecManager::create_macsec_sa(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (is_macsec_sa_existing( attr.m_macsecName, attr.m_direction, attr.m_sci, attr.m_an))
    {
        SWSS_LOG_WARN(
                "MACsec SA %s:%u at the device %s has been created",
                attr.m_sci.c_str(),
                static_cast<std::uint32_t>(attr.m_an),
                attr.m_macsecName.c_str());

        return true;
    }

    if (!is_macsec_sc_existing( attr.m_macsecName, attr.m_direction, attr.m_sci))
    {
        if (!create_macsec_sc(attr))
        {
            SWSS_LOG_WARN(
                    "Cannot create MACsec SC %s at the device %s.",
                    attr.m_sci.c_str(),
                    attr.m_macsecName.c_str());

            return false;
        }

    }

    if (attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        if (!create_macsec_egress_sa(attr))
        {
            SWSS_LOG_WARN(
                    "Cannot create MACsec egress SA %s:%u at the device %s.",
                    attr.m_sci.c_str(),
                    static_cast<std::uint32_t>(attr.m_an),
                    attr.m_macsecName.c_str());

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
                    attr.m_macsecName.c_str());

            return false;
        }
    }

    return true;
}

bool MACsecManager::delete_macsec_port(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    // Linux MACsec driver would not differentiate the Ingress and Egress port,
    // We just need to delete the Linux MACsec device when the egress port was deleted
    // And we assume that deleting the ingress port is always success.
    if (attr.m_direction == SAI_MACSEC_DIRECTION_INGRESS)
    {
        return true;
    }

    return delete_macsec_manager(attr.m_macsecName);
}

bool MACsecManager::delete_macsec_sc(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (!is_macsec_sc_existing( attr.m_macsecName, attr.m_direction, attr.m_sci))
    {
        SWSS_LOG_WARN(
                "MACsec SC %s at the device %s isn't existing",
                attr.m_sci.c_str(),
                attr.m_macsecName.c_str());

        return true;
    }

    if (attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        if (!delete_macsec_egress_sc(attr))
        {
            SWSS_LOG_WARN(
                    "Cannot delete MACsec egress SC %s at the device %s.",
                    attr.m_sci.c_str(),
                    attr.m_macsecName.c_str());

            return false;
        }
    }
    else
    {
        if (!delete_macsec_ingress_sc(attr))
        {
            SWSS_LOG_WARN(
                    "Cannot delete MACsec ingress SC %s at the device %s.",
                    attr.m_sci.c_str(),
                    attr.m_macsecName.c_str());

            return false;
        }
    }

    return true;
}

bool MACsecManager::delete_macsec_sa(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    if (!is_macsec_sa_existing( attr.m_macsecName, attr.m_direction, attr.m_sci, attr.m_an))
    {
        SWSS_LOG_WARN(
                "MACsec SA %s:%u at the device %s isn't existing",
                attr.m_sci.c_str(),
                static_cast<std::uint32_t>(attr.m_an),
                attr.m_macsecName.c_str());

        return true;
    }

    if (attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        if (!delete_macsec_egress_sa(attr))
        {
            SWSS_LOG_WARN(
                    "Cannot delete MACsec egress SA %s:%u at the device %s.",
                    attr.m_sci.c_str(),
                    static_cast<std::uint32_t>(attr.m_an),
                    attr.m_macsecName.c_str());

            return false;
        }
    }
    else
    {
        if (!delete_macsec_ingress_sa(attr))
        {
            SWSS_LOG_WARN(
                    "Cannot delete MACsec ingress SA %s:%u at the device %s.",
                    attr.m_sci.c_str(),
                    static_cast<std::uint32_t>(attr.m_an),
                    attr.m_macsecName.c_str());

            return false;
        }
    }


    return true;
}

bool MACsecManager::enable_macsec_filter(
        _In_ const std::string &macsecInterface,
        bool enable)
{
    SWSS_LOG_ENTER();

    auto itr = m_macsecTrafficManagers.find(macsecInterface);

    if (itr == m_macsecTrafficManagers.end())
    {
        SWSS_LOG_ERROR("MACsec port %s cannot be found", macsecInterface.c_str());
        return false;
    }

    auto &manager = itr->second;

    if (enable)
    {
        if (manager.m_forwarder == nullptr)
        {
            SWSS_LOG_ERROR("MACsec forwarder for device %s isn't existing", macsecInterface.c_str());

            return false;
        }

        manager.m_ingressFilter->set_macsec_fd(manager.m_forwarder->get_macsecfd());
        manager.m_egressFilter->set_macsec_fd(manager.m_forwarder->get_macsecfd());
    }

    manager.m_ingressFilter->enable_macsec_device(enable);
    manager.m_egressFilter->enable_macsec_device(enable);
    return true;
}

bool MACsecManager::update_macsec_sa_pn(
        _In_ const MACsecAttr &attr,
        _In_ sai_uint64_t pn)
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream
        << "/sbin/ip macsec set "
        << shellquote(attr.m_macsecName);

    if (attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        ostream << " tx";
    }
    else
    {
        ostream << " rx sci " << attr.m_sci;
    }

    ostream << " sa " << attr.m_an;

    if (attr.is_xpn())
    {
        ostream << " ssci " << attr.m_ssci;
    }

    ostream << ( attr.is_xpn() ? " xpn " : " pn " ) << pn;

    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

bool MACsecManager::get_macsec_sa_pn(
        _In_ const MACsecAttr &attr,
        _Out_ sai_uint64_t &pn) const
{
    SWSS_LOG_ENTER();

    pn = 1;
    std::string macsecSaInfo;

    if (!get_macsec_sa_info( attr.m_macsecName, attr.m_direction, attr.m_sci, attr.m_an, macsecSaInfo))
    {
        return false;
    }

    // Here is an example of MACsec SA
    //     0: PN 28, state on, key ebe9123ecbbfd96bee92c8ab01000000
    // Use pattern 'PN\s*(\d+)'
    // to extract packet number from MACsec SA

    const std::regex pattern("PN\\s*(\\d+)");
    std::smatch matches;

    if (std::regex_search(macsecSaInfo, matches, pattern))
    {
        if (matches.size() != 2)
        {
            SWSS_LOG_ERROR(
                    "Wrong match result %s in %s",
                    matches.str().c_str(),
                    macsecSaInfo.c_str());

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
                attr.m_macsecName.c_str());

        return false;
    }

}

// Create MACsec Egress SC
// $ ip link add link <VETH_NAME> name <MACSEC_NAME> type macsec sci <SCI>
// $ ip link set dev <MACSEC_NAME> up
// MACsec egress SC will be automatically create when the MACsec port is created
bool MACsecManager::create_macsec_egress_sc(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream
        << "/sbin/ip link add link "
        << shellquote(attr.m_vethName)
        << " name "
        << shellquote(attr.m_macsecName)
        << " type macsec "
        << " sci " << attr.m_sci
        << " encrypt " << (attr.m_encryptionEnable ? " on " : " off ")
        << " cipher " << attr.m_cipher;

    // send_sci=on is the default behavior, so we only need to set it when m_sendSci is false
    if (!attr.m_sendSci)
    {
        ostream
        << " && /sbin/ip link set link "
        << shellquote(attr.m_vethName)
        << " name "
        << shellquote(attr.m_macsecName)
        << " type macsec "
        << " send_sci off";
    }

    ostream
        << " && /sbin/ip link set dev "
        << shellquote(attr.m_macsecName)
        << " up";

    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    if (!exec(ostream.str()))
    {
        return false;
    }

    return
        add_macsec_forwarder(attr.m_macsecName)
        && enable_macsec_filter(attr.m_macsecName, true);
}

// Create MACsec Ingress SC
// $ ip macsec add <MACSEC_NAME> rx sci <SCI>
bool MACsecManager::create_macsec_ingress_sc(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream
        << "/sbin/ip macsec add "
        << shellquote(attr.m_macsecName)
        << " rx sci "
        << attr.m_sci
        << " on";

    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

// Create MACsec Egress SA
// $ ip macsec add <MACSEC_NAME> tx sa <AN> pn <PN> on key <AUTH_KEY> <SAK> &&
// ip link set link <VETH_NAME> name <MACSEC_NAME> type macsec encodingsa <AN>
bool MACsecManager::create_macsec_egress_sa(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream
        << "/sbin/ip macsec add "
        << shellquote(attr.m_macsecName)
        << " tx sa "
        << attr.m_an
        << ( attr.is_xpn() ? " xpn " : " pn ")
        << attr.m_pn
        << ( attr.is_xpn() ? " ssci " : "" )
        << ( attr.is_xpn() ? attr.m_ssci : "" )
        << ( attr.is_xpn() ? " salt " : "" )
        << ( attr.is_xpn() ? attr.m_salt : "" )
        << " on key "
        << attr.m_authKey
        << " "
        << attr.m_sak
        << " && ip link set link "
        << attr.m_vethName
        << " name "
        << attr.m_macsecName
        << " type macsec encodingsa "
        << attr.m_an;

    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

// Create MACsec Ingress SA
// $ ip macsec add <MACSEC_NAME> rx sci <SCI> sa <SA> pn <PN> on key <AUTH_KEY> <SAK>
bool MACsecManager::create_macsec_ingress_sa(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream
        << "/sbin/ip macsec add "
        << shellquote(attr.m_macsecName)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an
        << ( attr.is_xpn() ? " xpn " : " pn " )
        << attr.m_pn
        << ( attr.is_xpn() ? " ssci " : "" )
        << ( attr.is_xpn() ? attr.m_ssci : "" )
        << ( attr.is_xpn() ? " salt " : "" )
        << ( attr.is_xpn() ? attr.m_salt : "" )
        << " on key "
        << attr.m_authKey
        << " "
        << attr.m_sak;

    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

// Delete MACsec Egress SC
// $ ip link del link <VETH_NAME> name <MACSEC_NAME> type macsec
bool MACsecManager::delete_macsec_egress_sc(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    bool result = true;
    std::ostringstream ostream;
    ostream
        << "/sbin/ip link del link "
        << shellquote(attr.m_vethName)
        << " name "
        << shellquote(attr.m_macsecName)
        << " type macsec";

    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    result &= delete_macsec_forwarder(attr.m_macsecName);
    result &= enable_macsec_filter(attr.m_macsecName, false);
    result &= exec(ostream.str());

    return result;
}

// Delete MACsec Ingress SC
// $ ip macsec set <MACSEC_NAME> rx sci <SCI> off
// $ ip macsec del <MACSEC_NAME> rx sci <SCI>
bool MACsecManager::delete_macsec_ingress_sc(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream
        << "/sbin/ip macsec set "
        << shellquote(attr.m_macsecName)
        << " rx sci "
        << attr.m_sci
        << " off"
        << " && ip macsec del "
        << shellquote(attr.m_macsecName)
        << " rx sci "
        << attr.m_sci;

    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

// Delete MACsec Egress SA
// $ ip macsec set <MACSEC_NAME> tx sa 0 off
// $ ip macsec del <MACSEC_NAME> tx sa 0
bool MACsecManager::delete_macsec_egress_sa(
        _In_ const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream
        << "/sbin/ip macsec set "
        << shellquote(attr.m_macsecName)
        << " tx sa "
        << attr.m_an
        << " off"
        << " && ip macsec del "
        << shellquote(attr.m_macsecName)
        << " tx sa "
        << attr.m_an
        ;

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

    std::ostringstream ostream;
    ostream
        << "/sbin/ip macsec set "
        << shellquote(attr.m_macsecName)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an
        << " off"
        << " && ip macsec del "
        << shellquote(attr.m_macsecName)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an;

    SWSS_LOG_NOTICE("%s", ostream.str().c_str());

    return exec(ostream.str());
}

bool MACsecManager::add_macsec_forwarder(
        _In_ const std::string &macsecInterface)
{
    SWSS_LOG_ENTER();

    auto itr = m_macsecTrafficManagers.find(macsecInterface);

    if (itr == m_macsecTrafficManagers.end())
    {
        SWSS_LOG_ERROR("MACsec port %s cannot be found", macsecInterface.c_str());

        return false;
    }

    auto &manager = itr->second;

    manager.m_forwarder = std::make_shared<MACsecForwarder>(macsecInterface, manager.m_info);
    return true;
}

bool MACsecManager::delete_macsec_forwarder(
        _In_ const std::string &macsecInterface)
{
    SWSS_LOG_ENTER();

    auto itr = m_macsecTrafficManagers.find(macsecInterface);

    if (itr == m_macsecTrafficManagers.end())
    {
        SWSS_LOG_ERROR("MACsec port %s cannot be found", macsecInterface.c_str());

        return false;
    }

    auto &manager = itr->second;

    manager.m_forwarder.reset();
    return true;
}

bool MACsecManager::add_macsec_manager(
        _In_ const std::string &macsecInterface,
        _In_ std::shared_ptr<HostInterfaceInfo> info)
{
    SWSS_LOG_ENTER();

    auto itr = m_macsecTrafficManagers.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(macsecInterface),
            std::forward_as_tuple());

    if (!itr.second)
    {
        SWSS_LOG_ERROR(
                "macsec manager for %s was existed",
                macsecInterface.c_str());

        return false;
    }

    auto &manager = itr.first->second;

    manager.m_info = info;

    manager.m_ingressFilter = std::make_shared<MACsecIngressFilter>(macsecInterface);
    manager.m_egressFilter = std::make_shared<MACsecEgressFilter>(macsecInterface);

    manager.m_info->installEth2TapFilter(
            FilterPriority::MACSEC_FILTER,
            manager.m_ingressFilter);
    manager.m_info->installTap2EthFilter(
            FilterPriority::MACSEC_FILTER,
            manager.m_egressFilter);

    return true;
}

bool MACsecManager::delete_macsec_manager(
        _In_ const std::string &macsecInterface)
{
    SWSS_LOG_ENTER();

    auto itr = m_macsecTrafficManagers.find(macsecInterface);

    if (itr == m_macsecTrafficManagers.end())
    {

        SWSS_LOG_ERROR(
                "macsec manager for %s isn't existed",
                macsecInterface.c_str());

        return false;
    }

    auto &manager = itr->second;

    manager.m_info->uninstallEth2TapFilter(manager.m_ingressFilter);
    manager.m_info->uninstallTap2EthFilter(manager.m_egressFilter);
    m_macsecTrafficManagers.erase(itr);

    return true;
}

// Query MACsec session
// $ ip macsec show <MACSEC_NAME>
bool MACsecManager::get_macsec_device_info(
        _In_ const std::string &macsecDevice,
        _Out_ std::string &info) const
{
    SWSS_LOG_ENTER();

    std::ostringstream ostream;
    ostream
        << "/sbin/ip macsec show "
        << shellquote(macsecDevice);

    return exec(ostream.str(), info);
}

bool MACsecManager::is_macsec_device_existing(
        _In_ const std::string &macsecDevice) const
{
    SWSS_LOG_ENTER();

    std::string macsec_info;

    return get_macsec_device_info(macsecDevice, macsec_info);
}

bool MACsecManager::get_macsec_sc_info(
        _In_ const std::string &macsecDevice,
        _In_ sai_int32_t direction,
        _In_ const std::string &sci,
        _Out_ std::string &info) const
{
    SWSS_LOG_ENTER();

    std::string macsec_info;

    if (!get_macsec_device_info(macsecDevice, macsec_info))
    {
        SWSS_LOG_DEBUG(
                "MACsec device %s is nonexisting",
                macsecDevice.c_str());

        return false;
    }

    const char *strDirection = (direction == SAI_MACSEC_DIRECTION_EGRESS) ? "TXSC" : "RXSC";

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
        << strDirection
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

    return false;
}

bool MACsecManager::is_macsec_sc_existing(
        _In_ const std::string &macsecDevice,
        _In_ sai_int32_t direction,
        _In_ const std::string &sci) const
{
    SWSS_LOG_ENTER();

    std::string macsec_sc_info;

    return get_macsec_sc_info(macsecDevice, direction, sci, macsec_sc_info);
}

bool MACsecManager::get_macsec_sa_info(
        _In_ const std::string &macsecDevice,
        _In_ sai_int32_t direction,
        _In_ const std::string &sci,
        _In_ macsec_an_t an,
        _Out_ std::string &info) const
{
    SWSS_LOG_ENTER();

    std::string macsec_sc_info;

    if (!get_macsec_sc_info(macsecDevice, direction, sci, macsec_sc_info))
    {
        SWSS_LOG_DEBUG(
                "The MACsec SC %s at the device %s is nonexisting.",
                sci.c_str(),
                macsecDevice.c_str());

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

    return false;
}

bool MACsecManager::is_macsec_sa_existing(
        _In_ const std::string &macsecDevice,
        _In_ sai_int32_t direction,
        _In_ const std::string &sci,
        _In_ macsec_an_t an) const
{
    SWSS_LOG_ENTER();

    std::string macsecSaInfo;

    return get_macsec_sa_info( macsecDevice, direction, sci, an, macsecSaInfo);
}

size_t MACsecManager::get_macsec_sa_count(
        _In_ const std::string &macsecDevice,
        _In_ sai_int32_t direction,
        _In_ const std::string &sci) const
{
    SWSS_LOG_ENTER();

    size_t sa_count = 0;

    for (macsec_an_t an = 0; an <= MAX_MACSEC_SA_NUMBER; an++) // lgtm [cpp/constant-comparison]
    {
        if (is_macsec_sa_existing(macsecDevice, direction, sci, an))
        {
            sa_count++;
        }
    }

    return sa_count;
}

void MACsecManager::cleanup_macsec_device() const
{
    SWSS_LOG_ENTER();

    if (access("/sbin/ip", F_OK) == -1)
    {
        SWSS_LOG_WARN("file /sbin/ip not accessible, skipping");
        return;
    }

    std::string macsecInfos;

    if (!exec("/sbin/ip macsec show", macsecInfos))
    {
        // this is workaround, there was exception thrown here, and it probably
        // suggest that, ip command is in place, but don't support "macsec"
        SWSS_LOG_WARN("Cannot show MACsec ports");
        return;
    }

    // Here is an example of MACsec device information
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
    const std::regex pattern("\\d+:\\s*(\\w+):");
    std::smatch matches;
    std::string::const_iterator searchPos(macsecInfos.cbegin());

    while(std::regex_search(searchPos, macsecInfos.cend(), matches, pattern))
    {
        std::ostringstream ostream;
        ostream << "/sbin/ip link del " << matches[1].str();

        if (!exec(ostream.str()))
        {
            SWSS_LOG_WARN(
                    "Cannot cleanup MACsec interface %s",
                    matches[1].str().c_str());
        }

        searchPos = matches.suffix().first;
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
