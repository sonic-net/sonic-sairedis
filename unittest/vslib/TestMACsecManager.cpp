#include "MACsecAttr.h"
#include "MACsecManager.h"

#include <swss/logger.h>

#include <gtest/gtest.h>

#include <set>

using namespace saivs;

TEST(MACsecManager, create_macsec_ingress_sa)
{
    // This is a system call that may not be valid in the test environment,
    // So, this case is just for the testing coverage checking.

    MACsecManager manager;

    MACsecAttr attr;
    attr.m_vethName = "eth0";
    attr.m_macsecName = "macsec_eth0";
    attr.m_sci = "02:42:ac:11:00:03";
    attr.m_an = 0;
    attr.m_pn = 1;
    attr.m_cipher = MACsecAttr::CIPHER_NAME_GCM_AES_XPN_128;
    attr.m_ssci = 0x1;
    attr.m_salt = "";
    attr.m_authKey = "";
    attr.m_sak = "";
    manager.create_macsec_ingress_sa(attr);
}

TEST(MACsecManager, update_macsec_sa_pn)
{
    // This is a system call that may not be valid in the test environment,
    // So, this case is just for the testing coverage checking.

    MACsecManager manager;

    MACsecAttr attr;
    attr.m_vethName = "eth0";
    attr.m_macsecName = "macsec_eth0";
    attr.m_sci = "02:42:ac:11:00:03";
    attr.m_an = 0;
    attr.m_pn = 1;
    attr.m_cipher = MACsecAttr::CIPHER_NAME_GCM_AES_XPN_128;
    attr.m_ssci = 0x1;
    attr.m_salt = "";
    attr.m_authKey = "";
    attr.m_sak = "";
    manager.update_macsec_sa_pn(attr, 2);
}

class MockMACsecManager_CleanupMACsecDevice : public MACsecManager
{
public:
    mutable std::set<std::string> m_rest_devices;
protected:
    virtual bool exec(
        _In_ const std::string &command,
        _Out_ std::string &output) const
    {
        SWSS_LOG_ENTER();

        if (command == "/sbin/ip macsec show")
        {
            output = "\
2774: macsec0: protect on validate strict sc off sa off encrypt on send_sci on end_station off scb off replay on window 0\n\
    cipher suite: GCM-AES-128, using ICV length 16\n\
    TXSC: fe5400409b920001 on SA 0\n\
2775: macsec1: protect on validate strict sc off sa off encrypt on send_sci on end_station off scb off replay on window 0\n\
    cipher suite: GCM-AES-128, using ICV length 16\n\
    TXSC: fe5400409b920001 on SA 0\n\
2776: macsec2: protect on validate strict sc off sa off encrypt on send_sci on end_station off scb off replay on window 0\n\
    cipher suite: GCM-AES-128, using ICV length 16\n\
    TXSC: fe5400409b920001 on SA 0";
            m_rest_devices.insert("macsec0");
            m_rest_devices.insert("macsec1");
            m_rest_devices.insert("macsec2");
            return true;
        }
        else if (command.rfind("/sbin/ip link del ") == 0)
        {
            std::string name = command.substr(strlen("/sbin/ip link del "));
            m_rest_devices.erase(name);
            return true;
        }
        return MACsecManager::exec(command, output);
    }
};

TEST(MACsecManager, cleanup_macsec_device)
{
    MockMACsecManager_CleanupMACsecDevice manager;

    manager.cleanup_macsec_device();

    EXPECT_EQ(manager.m_rest_devices.size(), 0);
}
