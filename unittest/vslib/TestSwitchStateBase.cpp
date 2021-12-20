#include "SwitchStateBase.h"
#include "MACsecAttr.h"

#include <gtest/gtest.h>

#include <vector>

using namespace saivs;

TEST(SwitchStateBase, loadMACsecAttrFromMACsecSA)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    SwitchStateBase ss(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_attribute_t attr;
    std::vector<sai_attribute_t> attrs;
    MACsecAttr macsecAttr;

    attr.id = SAI_MACSEC_SC_ATTR_FLOW_ID;
    attrs.push_back(attr);
    attr.id = SAI_MACSEC_SC_ATTR_MACSEC_SCI;
    attrs.push_back(attr);
    attr.id = SAI_MACSEC_SC_ATTR_ENCRYPTION_ENABLE;
    attrs.push_back(attr);
    attr.id = SAI_MACSEC_SC_ATTR_MACSEC_CIPHER_SUITE;
    attr.value.s32 = sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_128;
    attrs.push_back(attr);
    EXPECT_EQ(
        SAI_STATUS_SUCCESS,
        ss.create_internal(
            SAI_OBJECT_TYPE_MACSEC_SC,
            "oid:0x0",
            0,
            static_cast<uint32_t>(attrs.size()),
            attrs.data()));

    attr.id = SAI_MACSEC_SA_ATTR_SC_ID;
    attr.value.oid = 0;
    ss.loadMACsecAttrFromMACsecSA(0, 1 , &attr, macsecAttr);

    EXPECT_EQ(macsecAttr.m_cipher, MACsecAttr::CIPHER_NAME_GCM_AES_128);
}
