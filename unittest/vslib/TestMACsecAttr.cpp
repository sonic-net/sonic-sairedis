#include "MACsecAttr.h"

#include <gtest/gtest.h>

#include <unordered_set>

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

TEST(MACsecAttr, unordered_set)
{
    MACsecAttr attr1, attr2, attr3;
    std::unordered_set<MACsecAttr, MACsecAttr::Hash> attrSet;

    attr1.m_macsecName = "abc";
    attr2.m_macsecName = "abc";
    attr3.m_macsecName = "123";
    attrSet.insert(attr1);

    EXPECT_NE(attrSet.find(attr2), attrSet.end());
    EXPECT_EQ(attrSet.find(attr3), attrSet.end());


    attrSet.clear();
    attr1.m_sci = "0";
    attrSet.insert(attr1);

    EXPECT_EQ(attrSet.find(attr2), attrSet.end());

    attr2.m_sci = "0";

    EXPECT_NE(attrSet.find(attr2), attrSet.end());


    attrSet.clear();
    attr1.m_sci = "0";
    attr1.m_an = 0;
    attrSet.insert(attr1);

    EXPECT_EQ(attrSet.find(attr2), attrSet.end());

    attr2.m_an = 0;

    EXPECT_NE(attrSet.find(attr2), attrSet.end());


    attrSet.clear();
    attr1.m_authKey = "abc";
    attrSet.insert(attr1);

    EXPECT_NE(attrSet.find(attr2), attrSet.end());
}
