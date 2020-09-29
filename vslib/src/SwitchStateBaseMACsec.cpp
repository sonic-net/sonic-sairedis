#include "SwitchStateBase.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"

#include <cinttypes>
#include <string>
#include <vector>

#include <net/if.h>

using namespace saivs;


#define SAI_VS_MACSEC_PREFIX "macsec"

extern std::string sai_serialize_hex_binary(const sai_macsec_sak_t &value);

// Create MACsec Port
// $ ip link add link <VETH_NAME> name <MACSEC_NAME> type macsec sci <SCI>
static sai_status_t create_macsec_port(const MACsecSAAttr &attr)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream 
        << "ip link add link "
        << attr.veth_name
        << " name "
        << attr.macsec_name
        << " type macsec sci "
        << attr.sci;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return SAI_STATUS_SUCCESS;
}
// Create MACsec Egress SA
// $ ip macsec add macsec0 tx sa 0 pn 1 on key <KEY_ID> <SAK>
// Create MACsec Ingress SC
// $ ip macsec add macsec0 rx sci <SCI>
// Create MACsec Ingress SA
// $ ip macsec add macsec0 rx sci <SCI> sa <SA> pn <PN> on key <KEY_ID> <SAK>
static sai_status_t create_macsec_sa(const MACsecSAAttr &attr)
{
    SWSS_LOG_ENTER();

    // create_macsec_port(attr);

    // std::ostringstream ostream;
    // if (attr.direction == SAI_MACSEC_DIRECTION_EGRESS)
    // {
    //     ostream.clear();
    //     ostream 
    //         << "ip macsec add "
    //         << attr.macsec_name
    //         << " tx sa "
    //         << attr.an
    //         << " pn "
    //         << attr.pn
    //         << " on key 1 "
    //         << attr.sak;
    //     SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    // }
    // else if (attr.direction == SAI_MACSEC_DIRECTION_INGRESS)
    // {
    //     ostream.clear();
    //     ostream 
    //         << "ip macsec add "
    //         << attr.macsec_name
    //         << " rx sci "
    //         << attr.sci;
    //     SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    //     ostream.clear();
    //     ostream 
    //         << "ip macsec add "
    //         << attr.macsec_name
    //         << " rx sci "
    //         << attr.sci
    //         << "sa "
    //         << attr.an
    //         << " pn "
    //         << attr.pn
    //         << " on key 1 "
    //         << attr.sak;
    //     SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    // }
    // else
    // {
    //     return SAI_STATUS_INVALID_PARAMETER;
    // }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::setAclEntryMACsecFlowActive(
        _In_ sai_object_id_t entry_id,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("GANZE");

    if (attr == nullptr || entry_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("attr or entry_id is null");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto sid = sai_serialize_object_id(entry_id);
    CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, attr));
    if (attr->id == SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW)
    {
        std::vector<MACsecSAAttr> macsec_sa_attrs;
        if (loadMACsecSAAttrs(entry_id, attr, macsec_sa_attrs) == SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_NOTICE("Try Create MACsec SA");
            for (auto &sa_attr : macsec_sa_attrs)
            {
                if (attr->value.aclaction.enable)
                {
                    // Enable MACsec
                    create_macsec_sa(sa_attr);
                }
                else
                {
                    // Disable MACsec
                    SWSS_LOG_NOTICE("Disable MACsec");
                }
            }
        }
        else
        {
            SWSS_LOG_ERROR(
                "Cannot load MACsec SA from the entry %s",
                sai_serialize_object_id(entry_id).c_str());
            return SAI_STATUS_FAILURE;
        }
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::createMACsecSA(
        _In_ sai_object_id_t sa_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("GANZE");

    MACsecSAAttr macsec_sa_attr;
    if (loadMACsecSAAttr(attr_count, attr_list, macsec_sa_attr) == SAI_STATUS_SUCCESS)
    {
        create_macsec_sa(macsec_sa_attr);
    }

    auto sid = sai_serialize_object_id(sa_id);
    return create_internal(SAI_OBJECT_TYPE_MACSEC_SA, sid, switch_id, attr_count, attr_list);
}

sai_status_t SwitchStateBase::removeMACsecSA(
        _In_ sai_object_id_t sa_id)
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("GANZE");

    auto sid = sai_serialize_object_id(sa_id);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_SA, sid);
}

sai_status_t SwitchStateBase::getACLTable(
        _In_ sai_object_id_t entry_id,
        _Out_ sai_object_id_t &table_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    // Find ACL Table
    attr.id = SAI_ACL_ENTRY_ATTR_TABLE_ID;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    CHECK_STATUS(
        get(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            entry_id,
            1,
            &attr));
    table_id = attr.value.oid;

    return SAI_STATUS_SUCCESS;
}

#define TRY_SAI_METADATA_GET_ATTR_BY_ID(ATTR, ATTR_ID, ATTR_COUNT, ATTR_LIST)   \
    {                                                                           \
        ATTR = sai_metadata_get_attr_by_id(ATTR_ID, ATTR_COUNT, ATTR_LIST);     \
        if (ATTR == NULL)                                                       \
        {                                                                       \
            SWSS_LOG_ERROR("attr "#ATTR_ID" was not passed");                   \
            return SAI_STATUS_FAILURE;                                          \
        }                                                                       \
    } while(0);

sai_status_t SwitchStateBase::loadMACsecSAAttr(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _Out_ MACsecSAAttr &macsec_sa_attr)
{
    SWSS_LOG_ENTER();

    if (attr_list == nullptr || attr_count == 0)
    {
        SWSS_LOG_WARN("Attribute is empty");
        return SAI_STATUS_FAILURE;
    }

    const sai_attribute_t *attr = nullptr;
    std::vector<sai_attribute_t> attrs;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_SC_ID, attr_count, attr_list);

    // Find SCI and MACsec flow
    attrs.clear();
    attrs.emplace_back();
    attrs.back().id = SAI_MACSEC_SC_ATTR_FLOW_ID;
    attrs.emplace_back();
    attrs.back().id = SAI_MACSEC_SC_ATTR_MACSEC_SCI;
    CHECK_STATUS(
        get(
            SAI_OBJECT_TYPE_MACSEC_SC,
            attr->value.oid,
            static_cast<uint32_t>(attrs.size()),
            attrs.data()));
    auto flow_id = attrs[0].value.oid;
    macsec_sa_attr.sci = attrs[1].value.u64;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_MACSEC_DIRECTION, attr_count, attr_list);
    macsec_sa_attr.direction = attr->value.s32;

    // Find veth name
    // MACsec flow => ACL entry => ACL table => line port => veth name
    // Find ACL Entry
    attrs.clear();
    attrs.emplace_back();
    attrs.back().id = SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW;
    attrs.back().value.aclaction.parameter.oid = flow_id;
    attrs.back().value.aclaction.enable = true;
    std::vector<sai_object_id_t> acl_entry_ids;
    findObjects(SAI_OBJECT_TYPE_ACL_ENTRY, attrs.back(), acl_entry_ids);
    if (acl_entry_ids.empty())
    {
        SWSS_LOG_WARN(
            "Cannot find corresponding ACL entry for the flow %s",
            sai_serialize_object_id(flow_id).c_str());
        return SAI_STATUS_FAILURE;
    }
    // Find ACL Table
    sai_object_id_t acl_table_id = SAI_NULL_OBJECT_ID;
    // All entries with same MACsec flow correspond one ACL Table
    // Because an ACL Table corresponds one MACsec Port
    // And a flow never belongs to two MACsec Port
    if (getACLTable(acl_entry_ids[0], acl_table_id) != SAI_STATUS_SUCCESS
        || acl_table_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR(
            "Cannot find corresponding ACL table for the entry %s",
            sai_serialize_object_id(acl_entry_ids[0]).c_str());
        return SAI_STATUS_FAILURE;
    }
    // Find line port
    attrs.clear();
    attrs.emplace_back();
    if (macsec_sa_attr.direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        attrs.back().id = SAI_PORT_ATTR_EGRESS_MACSEC_ACL;
    }
    else
    {
        attrs.back().id = SAI_PORT_ATTR_INGRESS_MACSEC_ACL;
    }
    attrs.back().value.oid = acl_table_id;
    std::vector<sai_object_id_t> line_port_ids;
    findObjects(SAI_OBJECT_TYPE_PORT, attrs.back(), line_port_ids);
    if (line_port_ids.empty())
    {
        SWSS_LOG_WARN(
            "Cannot find corresponding line port for the ACL table %s",
            sai_serialize_object_id(acl_table_id).c_str());
        return SAI_STATUS_FAILURE;
    }
    else if (line_port_ids.size() > 1)
    {
        SWSS_LOG_ERROR(
            "Duplicated line ports for the ACL table %s",
            sai_serialize_object_id(acl_table_id).c_str());
        return SAI_STATUS_FAILURE;
    }
    // Find veth name
    // TODO, right now the line port is same as port
    for (auto& kvp: m_hostif_info_map)
    {
        if (kvp.second->m_portId == line_port_ids[0])
        {
            macsec_sa_attr.veth_name = vs_get_veth_name(kvp.first, line_port_ids[0]);
            auto index = if_nametoindex(macsec_sa_attr.veth_name.c_str());
            if (index == 0)
            {
                SWSS_LOG_ERROR(
                    "failed to get interface index for %s",
                    macsec_sa_attr.veth_name.c_str());
                return SAI_STATUS_FAILURE;
            }
            macsec_sa_attr.macsec_name = SAI_VS_MACSEC_PREFIX + std::to_string(index);
        }
    }

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_AN, attr_count, attr_list);
    macsec_sa_attr.an = attr->value.u8;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_ENCRYPTION_ENABLE, attr_count, attr_list);
    macsec_sa_attr.encryption_enable = attr->value.booldata;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_SAK, attr_count, attr_list);
    // macsec_sa_attr.sak.resize(sizeof(attr_list->value.macsecsak) * 2);
    // sai_serialize_macsec_sak(&macsec_sa_attr.sak[0], attr_list->value.macsecsak);
    macsec_sa_attr.sak = sai_serialize_hex_binary(attr_list->value.macsecsak);

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_SAK_256_BITS, attr_count, attr_list);
    if (!attr->value.booldata)
    {
        macsec_sa_attr.sak.resize(macsec_sa_attr.sak.length() / 2);
    }

    if (macsec_sa_attr.direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_XPN, attr_count, attr_list);
    }
    else
    {
        TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_MINIMUM_XPN, attr_count, attr_list);
    }
    macsec_sa_attr.pn = attr->value.u64;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecSAAttrs(
        _In_ sai_object_id_t entry_id,
        _In_ const sai_attribute_t* entry_attr,
        _Out_ std::vector<MACsecSAAttr> &macsec_sa_attrs)
{
    SWSS_LOG_ENTER();

    if (entry_attr == nullptr
        || entry_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_WARN("entry is empty");
        return SAI_STATUS_FAILURE;
    }

    if (entry_attr->id != SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW)
    {
        SWSS_LOG_WARN("Not include MACsec flow information");
        return SAI_STATUS_FAILURE;
    }
    auto flow_id = entry_attr->value.aclaction.parameter.oid;

    sai_attribute_t attr;

    // Find all MACsec SCs that use this flow
    std::vector<sai_object_id_t> macsec_scs;
    attr.id = SAI_MACSEC_SC_ATTR_FLOW_ID;
    attr.value.oid = flow_id;
    findObjects(SAI_OBJECT_TYPE_MACSEC_SC, attr, macsec_scs);

    // Find all MACsec SAs that use this entry
    std::vector<sai_object_id_t> macsec_sas;
    for (auto sc_id : macsec_scs)
    {
        attr.id = SAI_MACSEC_SA_ATTR_SC_ID;
        attr.value.oid = sc_id;
        findObjects(SAI_OBJECT_TYPE_MACSEC_SA, attr, macsec_sas);
    }

    macsec_sa_attrs.reserve(macsec_sas.size());
    for (auto sa_id : macsec_sas)
    {
        std::vector<sai_attribute_t> attrs;
        if (dumpObject(SAI_OBJECT_TYPE_MACSEC_SA, sa_id, attrs))
        {
            macsec_sa_attrs.emplace_back();
            if (loadMACsecSAAttr(
                static_cast<uint32_t>(attrs.size()),
                attrs.data(),
                macsec_sa_attrs.back()) != SAI_STATUS_SUCCESS)
            {
                macsec_sa_attrs.pop_back();
                SWSS_LOG_ERROR(
                    "The MACsec SA %s is invalid",
                    sai_serialize_object_id(sa_id).c_str());
            }
        }
        else
        {
            SWSS_LOG_ERROR(
                "The MACsec SA %s is not existed",
                sai_serialize_object_id(sa_id).c_str());
        }
    }

    return SAI_STATUS_SUCCESS;
}
