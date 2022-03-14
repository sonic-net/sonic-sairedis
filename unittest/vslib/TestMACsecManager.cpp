#include "MACsecAttr.h"
#include "MACsecManager.h"

#include <gtest/gtest.h>

#include <vector>

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
