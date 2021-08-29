
#include "SwitchStateBase.h"

#include <gtest/gtest.h>

#include <vector>

using namespace saivs;

#define CIPHER_NAME_GCM_AES_128 "GCM-AES-128"
#define CIPHER_NAME_GCM_AES_256 "GCM-AES-256"
#define CIPHER_NAME_GCM_AES_XPN_128 "GCM-AES-XPN-128"
#define CIPHER_NAME_GCM_AES_XPN_256 "GCM-AES-XPN-256"
#define DEFAULT_CIPHER_NAME CIPHER_NAME_GCM_AES_128


TEST(SwitchStateBase, loadMACsecAttrFromMACsecSC)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    SwitchStateBase ssb(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_attribute_t attr;
    MACsecAttr macsecAttr;

    attr.id = SAI_MACSEC_SC_ATTR_MACSEC_CIPHER_SUITE;
    attr.value.s32 = sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_128;
    ssb.loadMACsecAttrFromMACsecSC(0, 1 , &attr, macsecAttr);
    EXPECT_EQ(macsecAttr.m_cipher, CIPHER_NAME_GCM_AES_128);

    attr.id = SAI_MACSEC_SC_ATTR_MACSEC_CIPHER_SUITE;
    attr.value.s32 = sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_256;
    ssb.loadMACsecAttrFromMACsecSC(0, 1 , &attr, macsecAttr);
    EXPECT_EQ(macsecAttr.m_cipher, CIPHER_NAME_GCM_AES_256);

    attr.id = SAI_MACSEC_SC_ATTR_MACSEC_CIPHER_SUITE;
    attr.value.s32 = sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_128;
    ssb.loadMACsecAttrFromMACsecSC(0, 1 , &attr, macsecAttr);
    EXPECT_EQ(macsecAttr.m_cipher, CIPHER_NAME_GCM_AES_XPN_128);

    attr.id = SAI_MACSEC_SC_ATTR_MACSEC_CIPHER_SUITE;
    attr.value.s32 = sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_256;
    ssb.loadMACsecAttrFromMACsecSC(0, 1 , &attr, macsecAttr);
    EXPECT_EQ(macsecAttr.m_cipher, CIPHER_NAME_GCM_AES_XPN_256);

}
