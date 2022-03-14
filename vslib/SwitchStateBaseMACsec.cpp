#include "MACsecAttr.h"
#include "SwitchStateBase.h"
#include "meta/sai_serialize.h"

#include "swss/exec.h"
#include "swss/logger.h"

#include <cinttypes>
#include <string>
#include <vector>
#include <regex>

#include <net/if.h>
#include <arpa/inet.h>
#include <byteswap.h>

using namespace saivs;

#define SAI_VS_MACSEC_PREFIX "macsec_"
#define MACSEC_SYSTEM_IDENTIFIER (12)
#define MACSEC_PORT_IDENTIFIER (4)
#define MACSEC_SCI_LENGTH (MACSEC_SYSTEM_IDENTIFIER + MACSEC_PORT_IDENTIFIER)

#define SAI_METADATA_GET_ATTR_BY_ID(attr, attrId, attrCount, attrList) \
{ \
    attr = sai_metadata_get_attr_by_id(attrId, attrCount, attrList); \
    if (attr == NULL) \
    { \
        SWSS_LOG_ERROR("attr " #attrId " was not passed"); \
        return SAI_STATUS_FAILURE; \
    } \
}

sai_status_t SwitchStateBase::setAclEntryMACsecFlowActive(
        _In_ sai_object_id_t entryId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr == nullptr || entryId == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("attr or entryId is null");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr->id != SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW)
    {
        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr->id);

        SWSS_LOG_ERROR("Attribute type (%s) isn't correct, expect SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW", meta->attridname);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // Enable MACsec
    if (attr->value.aclaction.enable)
    {
        SWSS_LOG_DEBUG("Enable MACsec on entry %s", sai_serialize_object_id(entryId).c_str());

        auto sid = sai_serialize_object_id(entryId);

        CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, attr));

        std::vector<MACsecAttr> macsecAttrs;

        if (loadMACsecAttrsFromACLEntry(entryId, attr, SAI_OBJECT_TYPE_MACSEC_SA, macsecAttrs) == SAI_STATUS_SUCCESS)
        {
            // In Linux MACsec model, Egress SA need to be created before ingress SA
            for (auto &macsecAttr : macsecAttrs)
            {
                if (macsecAttr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
                {
                    if (m_macsecManager.create_macsec_sa(macsecAttr))
                    {
                        SWSS_LOG_NOTICE(
                            "Enable MACsec SA %s:%u at the device %s",
                            macsecAttr.m_sci.c_str(),
                            static_cast<std::uint32_t>(macsecAttr.m_an),
                            macsecAttr.m_macsecName.c_str());
                    }
                }
            }

            for (auto &macsecAttr : macsecAttrs)
            {
                if (macsecAttr.m_direction == SAI_MACSEC_DIRECTION_INGRESS)
                {
                    if (m_macsecManager.create_macsec_sa(macsecAttr))
                    {
                        SWSS_LOG_NOTICE(
                            "Enable MACsec SA %s:%u at the device %s",
                            macsecAttr.m_sci.c_str(),
                            static_cast<std::uint32_t>(macsecAttr.m_an),
                            macsecAttr.m_macsecName.c_str());
                    }
                    else
                    {
                        m_uncreatedIngressMACsecSAs.insert(macsecAttr);
                    }
                }
            }
        }
        else
        {
            SWSS_LOG_WARN("Cannot load MACsec SA from the entry %s", sai_serialize_object_id(entryId).c_str());
        }
    }
    // Disable MACsec
    else
    {
        SWSS_LOG_DEBUG("Disable MACsec on entry %s", sai_serialize_object_id(entryId).c_str());

        std::vector<MACsecAttr> macsecAttrs;

        if (loadMACsecAttrsFromACLEntry(entryId, attr, SAI_OBJECT_TYPE_MACSEC_SC, macsecAttrs) == SAI_STATUS_SUCCESS)
        {
            for (auto &macsecAttr : macsecAttrs)
            {
                if (m_macsecManager.delete_macsec_sa(macsecAttr))
                {
                    SWSS_LOG_NOTICE(
                        "Delete MACsec SA %s:%u at the device %s",
                        macsecAttr.m_sci.c_str(),
                        static_cast<std::uint32_t>(macsecAttr.m_an),
                        macsecAttr.m_macsecName.c_str());
                }
            }
        }
        else
        {
            SWSS_LOG_WARN("Cannot load MACsec SC from the entry %s", sai_serialize_object_id(entryId).c_str());
        }

        CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sai_serialize_object_id(entryId), attr));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::createMACsecPort(
        _In_ sai_object_id_t macsecSaId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsecAttr;

    if (loadMACsecAttr(SAI_OBJECT_TYPE_MACSEC_PORT, macsecSaId, attrCount, attrList, macsecAttr) == SAI_STATUS_SUCCESS)
    {
        if (m_macsecManager.create_macsec_port(macsecAttr))
        {
            SWSS_LOG_NOTICE("Enable MACsec port %s", macsecAttr.m_macsecName.c_str());
        }
    }

    auto sid = sai_serialize_object_id(macsecSaId);
    return create_internal(SAI_OBJECT_TYPE_MACSEC_PORT, sid, switchId, attrCount, attrList);
}

sai_status_t SwitchStateBase::createMACsecSC(
        _In_ sai_object_id_t macsecScId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsecAttr;

    if (loadMACsecAttr(SAI_OBJECT_TYPE_MACSEC_SC, macsecScId, attrCount, attrList, macsecAttr) == SAI_STATUS_SUCCESS)
    {
        if (m_macsecManager.create_macsec_sc(macsecAttr))
        {
            SWSS_LOG_NOTICE(
                    "Create MACsec SC %s at the device %s",
                    macsecAttr.m_sci.c_str(),
                    macsecAttr.m_macsecName.c_str());
        }
    }

    auto sid = sai_serialize_object_id(macsecScId);
    return create_internal(SAI_OBJECT_TYPE_MACSEC_SC, sid, switchId, attrCount, attrList);
}

sai_status_t SwitchStateBase::createMACsecSA(
        _In_ sai_object_id_t macsecSaId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsecAttr;

    if (loadMACsecAttr(SAI_OBJECT_TYPE_MACSEC_SA, macsecSaId, attrCount, attrList, macsecAttr) == SAI_STATUS_SUCCESS)
    {
        if (m_macsecManager.create_macsec_sa(macsecAttr))
        {
            SWSS_LOG_NOTICE(
                    "Enable MACsec SA %s:%u at the device %s",
                    macsecAttr.m_sci.c_str(),
                    static_cast<std::uint32_t>(macsecAttr.m_an),
                    macsecAttr.m_macsecName.c_str());

            // Maybe there are some uncreated ingress SAs that were added into m_uncreatedIngressMACsecSAs
            // because the corresponding egress SA has not been created.
            // So retry to create them.
            if (macsecAttr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
            {
                retryCreateIngressMaCsecSAs();
            }
        }
        else
        {
            // In Linux MACsec model, Egress SA need to be created before ingress SA.
            // So, if try to create the ingress SA firstly, it will failed.
            // But to create the egress SA should be always successful.
            m_uncreatedIngressMACsecSAs.insert(macsecAttr);
        }
    }

    auto sid = sai_serialize_object_id(macsecSaId);
    return create_internal(SAI_OBJECT_TYPE_MACSEC_SA, sid, switchId, attrCount, attrList);
}

sai_status_t SwitchStateBase::removeMACsecPort(
        _In_ sai_object_id_t macsecPortId)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsecAttr;

    if (loadMACsecAttr(SAI_OBJECT_TYPE_MACSEC_PORT, macsecPortId, macsecAttr) == SAI_STATUS_SUCCESS)
    {
        if (m_macsecManager.delete_macsec_port(macsecAttr))
        {
            SWSS_LOG_NOTICE("The MACsec port %s is deleted", macsecAttr.m_macsecName.c_str());
        }
    }

    auto flowItr = m_macsecFlowPortMap.begin();

    while (flowItr != m_macsecFlowPortMap.end())
    {
        if (flowItr->second == macsecPortId)
        {
            flowItr = m_macsecFlowPortMap.erase(flowItr);
        }
        else
        {
            flowItr ++;
        }
    }

    auto saItr = m_uncreatedIngressMACsecSAs.begin();

    while (saItr != m_uncreatedIngressMACsecSAs.end())
    {
        if (saItr->m_macsecName == macsecAttr.m_macsecName)
        {
            saItr = m_uncreatedIngressMACsecSAs.erase(saItr);
        }
        else
        {
            saItr ++;
        }
    }

    auto sid = sai_serialize_object_id(macsecPortId);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_PORT, sid);
}

sai_status_t SwitchStateBase::removeMACsecSC(
        _In_ sai_object_id_t macsecScId)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsecAttr;

    if (loadMACsecAttr(SAI_OBJECT_TYPE_MACSEC_SC, macsecScId, macsecAttr) == SAI_STATUS_SUCCESS)
    {
        if (m_macsecManager.delete_macsec_sc(macsecAttr))
        {
            SWSS_LOG_NOTICE(
                    "The MACsec sc %s in device %s is deleted",
                    macsecAttr.m_sci.c_str(),
                    macsecAttr.m_macsecName.c_str());
        }
    }

    auto saItr = m_uncreatedIngressMACsecSAs.begin();

    while (saItr != m_uncreatedIngressMACsecSAs.end())
    {
        if (saItr->m_macsecName == macsecAttr.m_macsecName && saItr->m_sci == macsecAttr.m_sci)
        {
            saItr = m_uncreatedIngressMACsecSAs.erase(saItr);
        }
        else
        {
            saItr ++;
        }
    }

    auto sid = sai_serialize_object_id(macsecScId);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_SC, sid);
}

sai_status_t SwitchStateBase::removeMACsecSA(
        _In_ sai_object_id_t macsecSaId)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsecAttr;

    if (loadMACsecAttr(SAI_OBJECT_TYPE_MACSEC_SA, macsecSaId, macsecAttr) == SAI_STATUS_SUCCESS)
    {
        if (m_macsecManager.delete_macsec_sa(macsecAttr))
        {
            SWSS_LOG_NOTICE(
                    "The MACsec SA %s:%u at the device %s is deleted.",
                    macsecAttr.m_sci.c_str(),
                    static_cast<std::uint32_t>(macsecAttr.m_an),
                    macsecAttr.m_macsecName.c_str());
        }
    }

    auto sid = sai_serialize_object_id(macsecSaId);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_SA, sid);
}

sai_status_t SwitchStateBase::getACLTable(
        _In_ sai_object_id_t entryId,
        _Out_ sai_object_id_t &tableId)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    // Find ACL Table
    attr.id = SAI_ACL_ENTRY_ATTR_TABLE_ID;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_ACL_ENTRY, entryId, 1, &attr));

    tableId = attr.value.oid;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::findPortByMACsecFlow(
        _In_ sai_object_id_t macsecFlowId,
        _Out_ sai_object_id_t &portId)
{
    SWSS_LOG_ENTER();

    auto itr = m_macsecFlowPortMap.find(macsecFlowId);

    if (itr != m_macsecFlowPortMap.end())
    {
        portId = itr->second;

        return SAI_STATUS_SUCCESS;
    }

    sai_attribute_t attr;

    // MACsec flow => ACL entry => ACL table
    // MACsec flow => MACsec SC
    // (ACL table & MACsec SC) => port

    // Find ACL Entry
    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW;
    attr.value.aclaction.parameter.oid = macsecFlowId;
    attr.value.aclaction.enable = true;
    std::vector<sai_object_id_t> aclEntryIds;
    findObjects(SAI_OBJECT_TYPE_ACL_ENTRY, attr, aclEntryIds);

    if (aclEntryIds.empty())
    {
        // No ACL Entry is bound to the flow
        SWSS_LOG_DEBUG(
                "Cannot find corresponding ACL entry for the flow %s",
                sai_serialize_object_id(macsecFlowId).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Find ACL Table
    sai_object_id_t aclTableId;

    // All ACL entries with same MACsec flow correspond one ACL Table
    // Because an ACL Table corresponds one MACsec Port
    // And a flow never belongs to two MACsec Port
    if (getACLTable(aclEntryIds.front(), aclTableId) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR(
                "Cannot find corresponding ACL table for the entry %s",
                sai_serialize_object_id(aclEntryIds.front()).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Find MACsec SC
    attr.id = SAI_MACSEC_SC_ATTR_FLOW_ID;
    attr.value.oid = macsecFlowId;
    std::vector<sai_object_id_t> macsecScIds;
    findObjects(SAI_OBJECT_TYPE_MACSEC_SC, attr, macsecScIds);

    // One MACsec SC will only belong to one MACsec flow
    // Meanwhile one MACsec flow will just belong to one port
    if (macsecScIds.empty())
    {
        SWSS_LOG_ERROR(
                "Cannot find corresponding MACsec SC for the flow %s",
                sai_serialize_object_id(macsecFlowId).c_str());

        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_MACSEC_SC_ATTR_MACSEC_DIRECTION;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_MACSEC_SC, macsecScIds.front(), 1, &attr));

    auto direction = attr.value.s32;

    // Find port
    attr.id = (direction == SAI_MACSEC_DIRECTION_EGRESS) ? SAI_PORT_ATTR_EGRESS_MACSEC_ACL : SAI_PORT_ATTR_INGRESS_MACSEC_ACL;
    attr.value.oid = aclTableId;

    std::vector<sai_object_id_t> port_ids;

    findObjects(SAI_OBJECT_TYPE_PORT, attr, port_ids);

    if (port_ids.size() != 1)
    {
        SWSS_LOG_ERROR(
                "Expect one port to one ACL table %s, but got %zu",
                sai_serialize_object_id(aclTableId).c_str(), port_ids.size());

        return SAI_STATUS_FAILURE;
    }

    portId = port_ids.front();

    m_macsecFlowPortMap[macsecFlowId] = portId;

    return SAI_STATUS_SUCCESS;
}

std::shared_ptr<HostInterfaceInfo> SwitchStateBase::findHostInterfaceInfoByPort(
        _In_ sai_object_id_t portId)
{
    SWSS_LOG_ENTER();

    std::shared_ptr<HostInterfaceInfo> info;

    for (auto &kvp : m_hostif_info_map)
    {
        if (kvp.second->m_portId == portId)
        {
            info = kvp.second;
            break;
        }
    }

    return info;
}

sai_status_t SwitchStateBase::loadMACsecAttrFromMACsecPort(
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ MACsecAttr &macsecAttr)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t *attr = nullptr;

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_PORT_ATTR_MACSEC_DIRECTION, attrCount, attrList);

    macsecAttr.m_direction = attr->value.s32;

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_PORT_ATTR_PORT_ID, attrCount, attrList);

    auto portId = attr->value.oid;

    macsecAttr.m_info = findHostInterfaceInfoByPort(portId);

    if (macsecAttr.m_info == nullptr)
    {
        SWSS_LOG_ERROR("Cannot find corresponding port %s", sai_serialize_object_id(portId).c_str());

        return SAI_STATUS_FAILURE;
    }

    macsecAttr.m_vethName = vs_get_veth_name(macsecAttr.m_info->m_name, portId);
    macsecAttr.m_macsecName = SAI_VS_MACSEC_PREFIX + macsecAttr.m_vethName;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecAttrFromMACsecSC(
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ MACsecAttr &macsecAttr)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t *attr = nullptr;

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SC_ATTR_MACSEC_CIPHER_SUITE, attrCount, attrList);

    macsecAttr.m_cipher = MACsecAttr::get_cipher_name(attr->value.s32);

    if (macsecAttr.m_cipher == MACsecAttr::CIPHER_NAME_INVALID)
    {
        return SAI_STATUS_FAILURE;
    }

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SC_ATTR_MACSEC_DIRECTION, attrCount, attrList);

    macsecAttr.m_direction = attr->value.s32;

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SC_ATTR_MACSEC_SCI, attrCount, attrList);

    auto sci = attr->value.u64;
    std::stringstream sciHexStr;

    sciHexStr << std::setw(MACSEC_SCI_LENGTH) << std::setfill('0');

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    sciHexStr << std::hex << bswap_64(sci);
#else
    sciHexStr << std::hex << sci;
#endif

    macsecAttr.m_sci = sciHexStr.str();

    if (macsecAttr.m_sci.length() < MACSEC_SCI_LENGTH)
    {
        // Fill leading zero to meet the MACsec SCI length
        macsecAttr.m_sci = std::string(MACSEC_SCI_LENGTH - macsecAttr.m_sci.length(), '0') + macsecAttr.m_sci;
    }

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SC_ATTR_MACSEC_EXPLICIT_SCI_ENABLE, attrCount, attrList);

    macsecAttr.m_sendSci = attr->value.booldata;

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SC_ATTR_ENCRYPTION_ENABLE, attrCount, attrList);

    macsecAttr.m_encryptionEnable = attr->value.booldata;


    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SC_ATTR_FLOW_ID, attrCount, attrList);

    auto flow_id = attr->value.oid;

    sai_object_id_t portId = SAI_NULL_OBJECT_ID;

    CHECK_STATUS(findPortByMACsecFlow(flow_id, portId));

    macsecAttr.m_info = findHostInterfaceInfoByPort(portId);

    if (macsecAttr.m_info == nullptr)
    {
        SWSS_LOG_ERROR("Cannot find corresponding port %s", sai_serialize_object_id(portId).c_str());

        return SAI_STATUS_FAILURE;
    }

    macsecAttr.m_vethName = vs_get_veth_name(macsecAttr.m_info->m_name, portId);
    macsecAttr.m_macsecName = SAI_VS_MACSEC_PREFIX + macsecAttr.m_vethName;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecAttrFromMACsecSA(
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ MACsecAttr &macsecAttr)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t *attr = nullptr;

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_SC_ID, attrCount, attrList);

    // Find MACsec SC attributes
    std::vector<sai_attribute_t> attrs(5);
    attrs[0].id = SAI_MACSEC_SC_ATTR_FLOW_ID;
    attrs[1].id = SAI_MACSEC_SC_ATTR_MACSEC_SCI;
    attrs[2].id = SAI_MACSEC_SC_ATTR_ENCRYPTION_ENABLE;
    attrs[3].id = SAI_MACSEC_SC_ATTR_MACSEC_CIPHER_SUITE;
    attrs[4].id = SAI_MACSEC_SC_ATTR_MACSEC_EXPLICIT_SCI_ENABLE;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_MACSEC_SC, attr->value.oid, static_cast<uint32_t>(attrs.size()), attrs.data()));

    macsecAttr.m_cipher = MACsecAttr::get_cipher_name(attrs[3].value.s32);

    if (macsecAttr.m_cipher == MACsecAttr::CIPHER_NAME_INVALID)
    {
        return SAI_STATUS_FAILURE;
    }

    auto flow_id = attrs[0].value.oid;
    auto sci = attrs[1].value.u64;
    std::stringstream sciHexStr;
    macsecAttr.m_encryptionEnable = attrs[2].value.booldata;
    bool is_sak_128_bit = (attrs[3].value.s32 == SAI_MACSEC_CIPHER_SUITE_GCM_AES_128 || attrs[3].value.s32 == SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_128);
    macsecAttr.m_sendSci = attrs[4].value.booldata;

    sciHexStr << std::setw(MACSEC_SCI_LENGTH) << std::setfill('0');

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    sciHexStr << std::hex << bswap_64(sci);
#else
    sciHexStr << std::hex << sci;
#endif

    macsecAttr.m_sci = sciHexStr.str();

    if (macsecAttr.m_sci.length() != MACSEC_SCI_LENGTH)
    {
        SWSS_LOG_ERROR("Invalid SCI %s", macsecAttr.m_sci.c_str());

        return SAI_STATUS_FAILURE;
    }

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_MACSEC_DIRECTION, attrCount, attrList);

    macsecAttr.m_direction = attr->value.s32;

    // Find veth name
    sai_object_id_t portId = SAI_NULL_OBJECT_ID;

    CHECK_STATUS(findPortByMACsecFlow(flow_id, portId));

    macsecAttr.m_info = findHostInterfaceInfoByPort(portId);

    if (macsecAttr.m_info == nullptr)
    {
        SWSS_LOG_ERROR("Cannot find corresponding port %s", sai_serialize_object_id(portId).c_str());

        return SAI_STATUS_FAILURE;
    }

    macsecAttr.m_vethName = vs_get_veth_name(macsecAttr.m_info->m_name, portId);
    macsecAttr.m_macsecName = SAI_VS_MACSEC_PREFIX + macsecAttr.m_vethName;

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_AN, attrCount, attrList);

    macsecAttr.m_an = attr->value.u8;

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_SAK, attrCount, attrList);

    macsecAttr.m_sak = sai_serialize_hex_binary(attr->value.macsecsak);

    if (is_sak_128_bit)
    {
        macsecAttr.m_sak = macsecAttr.m_sak.substr(macsecAttr.m_sak.length() / 2, macsecAttr.m_sak.length() / 2);
    }

    SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_AUTH_KEY, attrCount, attrList);
    macsecAttr.m_authKey = sai_serialize_hex_binary(attr->value.macsecauthkey);

    if (macsecAttr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_CONFIGURED_EGRESS_XPN, attrCount, attrList);
    }
    else
    {
        SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_MINIMUM_INGRESS_XPN, attrCount, attrList);
    }

    macsecAttr.m_pn = attr->value.u64;

    if (macsecAttr.is_xpn())
    {
        SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_MACSEC_SSCI, attrCount, attrList);

        // The Linux kernel directly uses ssci to XOR with the salt that is network order,
        // So, this conversion is useful to convert SSCI from the host order to network order.
        macsecAttr.m_ssci = htonl(attr->value.u32);

        SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_SALT, attrCount, attrList);

        macsecAttr.m_salt = sai_serialize_hex_binary(attr->value.macsecsalt);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecAttr(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ MACsecAttr &macsecAttr)
{
    SWSS_LOG_ENTER();

    if (attrList == nullptr || attrCount == 0)
    {
        SWSS_LOG_ERROR("Attribute list is empty");

        return SAI_STATUS_FAILURE;
    }

    switch (objectType)
    {
        case SAI_OBJECT_TYPE_MACSEC_PORT:
            return loadMACsecAttrFromMACsecPort(objectId, attrCount, attrList, macsecAttr);

        case SAI_OBJECT_TYPE_MACSEC_SC:
            return loadMACsecAttrFromMACsecSC(objectId, attrCount, attrList, macsecAttr);

        case SAI_OBJECT_TYPE_MACSEC_SA:
            return loadMACsecAttrFromMACsecSA(objectId, attrCount, attrList, macsecAttr);

        default:
            SWSS_LOG_ERROR("Wrong type %s", sai_serialize_object_type(objectType).c_str());

            break;
    }

    return SAI_STATUS_FAILURE;
}

sai_status_t SwitchStateBase::loadMACsecAttr(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _Out_ MACsecAttr &macsecAttr)
{
    SWSS_LOG_ENTER();

    std::vector<sai_attribute_t> attrs;

    if (dumpObject(objectId, attrs))
    {
        if (loadMACsecAttr(objectType, objectId, static_cast<uint32_t>(attrs.size()), attrs.data(), macsecAttr) != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_DEBUG(
                    "Cannot load attributions of %s %s",
                    sai_serialize_object_type(objectType).c_str(),
                    sai_serialize_object_id(objectId).c_str());

            return SAI_STATUS_FAILURE;
        }
    }
    else
    {
        SWSS_LOG_WARN(
                "The %s %s is not existed",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(objectId).c_str());

        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecAttrsFromACLEntry(
        _In_ sai_object_id_t entryId,
        _In_ const sai_attribute_t *entryAttr,
        _In_ sai_object_type_t objectType,
        _Out_ std::vector<MACsecAttr> &macsecAttrs)
{
    SWSS_LOG_ENTER();

    auto flow_id = entryAttr->value.aclaction.parameter.oid;

    macsecAttrs.clear();

    if (objectType == SAI_OBJECT_TYPE_MACSEC_PORT)
    {
        sai_object_id_t portId = SAI_NULL_OBJECT_ID;

        CHECK_STATUS(findPortByMACsecFlow(flow_id, portId));

        macsecAttrs.emplace_back();

        macsecAttrs.back().m_info = findHostInterfaceInfoByPort(portId);

        if (macsecAttrs.back().m_info == nullptr)
        {
            SWSS_LOG_ERROR("Cannot find corresponding port %s", sai_serialize_object_id(portId).c_str());

            macsecAttrs.clear();
            return SAI_STATUS_FAILURE;
        }

        macsecAttrs.back().m_vethName = vs_get_veth_name(macsecAttrs.back().m_info->m_name, portId);
        macsecAttrs.back().m_macsecName = SAI_VS_MACSEC_PREFIX + macsecAttrs.back().m_vethName;
        return SAI_STATUS_SUCCESS;
    }

    // Find all MACsec SCs that use this flow
    std::vector<sai_object_id_t> macsecScs;
    sai_attribute_t attr;
    attr.id = SAI_MACSEC_SC_ATTR_FLOW_ID;
    attr.value.oid = flow_id;
    findObjects(SAI_OBJECT_TYPE_MACSEC_SC, attr, macsecScs);

    if (objectType == SAI_OBJECT_TYPE_MACSEC_SC)
    {
        if (macsecScs.empty())
        {
            SWSS_LOG_DEBUG(
                    "No one MACsec SC is using the ACL entry %s",
                    sai_serialize_object_id(entryId).c_str());
        }

        macsecAttrs.reserve(macsecScs.size());

        for (auto sc_id : macsecScs)
        {
            macsecAttrs.emplace_back();

            if (loadMACsecAttr(SAI_OBJECT_TYPE_MACSEC_SC, sc_id, macsecAttrs.back()) != SAI_STATUS_SUCCESS)
            {
                // The fail log has been recorded at loadMACsecAttr
                macsecAttrs.pop_back();
            }
        }

        return SAI_STATUS_SUCCESS;
    }

    if (objectType == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        // Find all MACsec SAs that use this entry
        std::vector<sai_object_id_t> macsecSas;

        for (auto sc_id : macsecScs)
        {
            attr.id = SAI_MACSEC_SA_ATTR_SC_ID;
            attr.value.oid = sc_id;
            findObjects(SAI_OBJECT_TYPE_MACSEC_SA, attr, macsecSas);
        }

        if (macsecSas.empty())
        {
            SWSS_LOG_DEBUG(
                    "No one MACsec SA is using the ACL entry %s",
                    sai_serialize_object_id(entryId).c_str());
        }

        macsecAttrs.reserve(macsecSas.size());

        for (auto sa_id : macsecSas)
        {
            macsecAttrs.emplace_back();

            if (loadMACsecAttr(SAI_OBJECT_TYPE_MACSEC_SA, sa_id, macsecAttrs.back()) != SAI_STATUS_SUCCESS)
            {
                // The fail log has been recorded at loadMACsecAttr
                macsecAttrs.pop_back();
            }
        }

        return SAI_STATUS_SUCCESS;
    }

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t SwitchStateBase::getMACsecSAPacketNumber(
        _In_ sai_object_id_t macsecSaId,
        _Out_ sai_attribute_t &attr)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsecAttr;

    if (loadMACsecAttr(SAI_OBJECT_TYPE_MACSEC_SA, macsecSaId, macsecAttr) == SAI_STATUS_SUCCESS)
    {
        if (m_macsecManager.get_macsec_sa_pn(macsecAttr, attr.value.u64))
        {
            return SAI_STATUS_SUCCESS;
        }
    }
    else
    {
        SWSS_LOG_WARN(
                "The MACsec SA %s isn't existing",
                sai_serialize_object_id(macsecSaId).c_str());
    }

    return SAI_STATUS_FAILURE;
}

void SwitchStateBase::retryCreateIngressMaCsecSAs()
{
    SWSS_LOG_ENTER();

    auto itr = m_uncreatedIngressMACsecSAs.begin();

    while (itr != m_uncreatedIngressMACsecSAs.end())
    {
        if (m_macsecManager.create_macsec_sa(*itr))
        {
            SWSS_LOG_NOTICE(
                "Enable MACsec SA %s:%u at the device %s",
                itr->m_sci.c_str(),
                static_cast<std::uint32_t>(itr->m_an),
                itr->m_macsecName.c_str());

            itr = m_uncreatedIngressMACsecSAs.erase(itr);
        }
        else
        {
            itr ++;
        }
    }
}
