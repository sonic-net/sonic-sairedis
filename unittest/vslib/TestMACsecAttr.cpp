#include "MACsecAttr.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(MACsecAttr, ctr)
{
    MACsecAttr sec;
}

TEST(MACsecAttr, dtr)
{
    MACsecAttr sec;
}

TEST(MACsecAttr, get_cipher_name)
{
    EXPECT_EQ(MACsecAttr::get_cipher_name(sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_128), MACsecAttr::CIPHER_NAME_GCM_AES_128);

    EXPECT_EQ(MACsecAttr::get_cipher_name(sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_256), MACsecAttr::CIPHER_NAME_GCM_AES_256);

    EXPECT_EQ(MACsecAttr::get_cipher_name(sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_128), MACsecAttr::CIPHER_NAME_GCM_AES_XPN_128);

    EXPECT_EQ(MACsecAttr::get_cipher_name(sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_256), MACsecAttr::CIPHER_NAME_GCM_AES_XPN_256);

    EXPECT_EQ(MACsecAttr::get_cipher_name(100), MACsecAttr::CIPHER_NAME_INVALID);
}

TEST(MACsecAttr, is_xpn)
{
    MACsecAttr attr;
    attr.m_cipher = MACsecAttr::CIPHER_NAME_GCM_AES_128;

    EXPECT_FALSE(attr.is_xpn());

    attr.m_cipher = MACsecAttr::CIPHER_NAME_GCM_AES_256;

    EXPECT_FALSE(attr.is_xpn());

    attr.m_cipher = MACsecAttr::CIPHER_NAME_GCM_AES_XPN_128;

    EXPECT_TRUE(attr.is_xpn());

    attr.m_cipher = MACsecAttr::CIPHER_NAME_GCM_AES_XPN_256;

    EXPECT_TRUE(attr.is_xpn());
}
