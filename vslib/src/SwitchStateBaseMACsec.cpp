#include "MACsec.h"
#include "SwitchStateBase.h"
#include "meta/sai_serialize.h"

#include <swss/exec.h>
#include <swss/logger.h>

#include <cinttypes>
#include <string>
#include <vector>
#include <regex>

#include <net/if.h>

using namespace saivs;

#define SAI_VS_MACSEC_PREFIX "macsec_"

#define TRY_SAI_METADATA_GET_ATTR_BY_ID(ATTR, ATTR_ID, ATTR_COUNT, ATTR_LIST) \
    {                                                                         \
        ATTR = sai_metadata_get_attr_by_id(ATTR_ID, ATTR_COUNT, ATTR_LIST);   \
        if (ATTR == NULL)                                                     \
        {                                                                     \
            SWSS_LOG_ERROR("attr " #ATTR_ID " was not passed");               \
            return SAI_STATUS_FAILURE;                                        \
        }                                                                     \
    }                                                                         \
    while (0)                                                                 \
        ;

static std::string shellquote(const std::string &str)
{
    static const std::regex re("([$`\"\\\n])");
    return "\"" + std::regex_replace(str, re, "\\$1") + "\"";
}

static sai_status_t exec(const std::string &command)
{
    std::string res;
    if (swss::exec(command, res) != 0)
    {
        SWSS_LOG_DEBUG("FAIL %s", command.c_str());
        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}

// Create MACsec Port
// $ ip link add link <VETH_NAME> name <MACSEC_NAME> type macsec sci <SCI>
static sai_status_t create_macsec_port(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip link add link "
        << shellquote(attr.m_veth_name)
        << " name "
        << shellquote(attr.m_macsec_name)
        << " type macsec sci "
        << attr.m_sci;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return ::exec(ostream.str());
}

// Create MACsec Egress SA
// $ ip macsec add macsec0 tx sa 0 pn 1 on key 00 <SAK>
static sai_status_t create_macsec_egress_sa(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec add "
        << shellquote(attr.m_macsec_name)
        << " tx sa "
        << attr.m_an
        << " pn "
        << attr.m_pn
        << " on key 00 "
        << attr.m_sak;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return ::exec(ostream.str());
}

// Create MACsec Ingress SC
// $ ip macsec add macsec0 rx sci <SCI>
static sai_status_t create_macsec_ingress_sc(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec add "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " on";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return ::exec(ostream.str());
}

// Create MACsec Ingress SA
// $ ip macsec add macsec0 rx sci <SCI> sa <SA> pn <PN> on key 00 <SAK>
static sai_status_t create_macsec_ingress_sa(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();
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
        << " on key 00 "
        << attr.m_sak;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return ::exec(ostream.str());
}

static sai_status_t enable_macsec(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();
    create_macsec_port(attr);
    if (attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        create_macsec_egress_sa(attr);
    }
    else
    {
        create_macsec_ingress_sc(attr);
        create_macsec_ingress_sa(attr);
    }

    return SAI_STATUS_SUCCESS;
}

// Delete MACsec Port
// $ ip link del link <VETH_NAME> name <MACSEC_NAME> type macsec
static sai_status_t delete_macsec_port(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip link del link "
        << shellquote(attr.m_veth_name)
        << " name "
        << shellquote(attr.m_macsec_name)
        << " type macsec";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return ::exec(ostream.str());
}

// Delete MACsec Egress SA
// $ ip macsec set macsec0 tx sa 0 off
// $ ip macsec del macsec0 tx sa 0
static sai_status_t delete_macsec_egress_sa(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec set "
        << shellquote(attr.m_macsec_name)
        << " tx sa "
        << attr.m_an
        << " off";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    ::exec(ostream.str());
    ostream.str("");
    ostream
        << "ip macsec del "
        << shellquote(attr.m_macsec_name)
        << " tx sa "
        << attr.m_an;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return ::exec(ostream.str());
}

// Delete MACsec Ingress SC
// $ ip macsec set macsec0 rx sci <SCI> off
// $ ip macsec del macsec0 rx sci <SCI>
static sai_status_t delete_macsec_ingress_sc(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec add "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " off";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    ::exec(ostream.str());
    ostream.str("");
    ostream
        << "ip macsec del "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return ::exec(ostream.str());
}

// Delete MACsec Ingress SA
// $ ip macsec set macsec0 rx sci <SCI> sa <SA> off
// $ ip macsec del macsec0 rx sci <SCI> sa <SA>
static sai_status_t delete_macsec_ingress_sa(const MACsecAttr &attr)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream
        << "ip macsec set "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an
        << " off";
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    ::exec(ostream.str());
    ostream.str("");
    ostream
        << "ip macsec del "
        << shellquote(attr.m_macsec_name)
        << " rx sci "
        << attr.m_sci
        << " sa "
        << attr.m_an;
    SWSS_LOG_NOTICE("%s", ostream.str().c_str());
    return ::exec(ostream.str());
}

sai_status_t SwitchStateBase::setAclEntryMACsecFlowActive(
    _In_ sai_object_id_t entry_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr == nullptr || entry_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("attr or entry_id is null");
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (attr->id != SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW)
    {
        SWSS_LOG_ERROR("Attribute type isn't correct");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr->value.aclaction.enable)
    {
        SWSS_LOG_DEBUG("Enable MACsec on entry %s", sai_serialize_object_id(entry_id).c_str());
        auto sid = sai_serialize_object_id(entry_id);
        CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, attr));
        std::vector<MACsecAttr> macsec_attrs;
        if (loadMACsecAttrsFromACLEntry(
                entry_id,
                attr,
                SAI_OBJECT_TYPE_MACSEC_SA,
                macsec_attrs) == SAI_STATUS_SUCCESS)
        {
            for (auto &sa_attr : macsec_attrs)
            {
                enable_macsec(sa_attr);
            }
        }
        else
        {
            SWSS_LOG_ERROR(
                "Cannot load MACsec SA from the entry %s",
                sai_serialize_object_id(entry_id).c_str());
        }
    }
    else
    {
        SWSS_LOG_DEBUG("Disable MACsec on entry %s", sai_serialize_object_id(entry_id).c_str());
        std::vector<MACsecAttr> macsec_attrs;
        if (loadMACsecAttrsFromACLEntry(
                entry_id,
                attr,
                SAI_OBJECT_TYPE_MACSEC_PORT,
                macsec_attrs) == SAI_STATUS_SUCCESS)
        {
            if (!macsec_attrs.empty())
            {
                if (macsec_attrs.size() > 1)
                {
                    SWSS_LOG_ERROR(
                        "Duplicated line ports for the ACL entry %s",
                        sai_serialize_object_id(entry_id).c_str());
                    auto sid = sai_serialize_object_id(entry_id);
                    CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, attr));
                    return SAI_STATUS_FAILURE;
                }
                delete_macsec_port(macsec_attrs.back());
            }
        }
        else
        {
            SWSS_LOG_DEBUG(
                "Cannot load MACsec Port from the entry %s",
                sai_serialize_object_id(entry_id).c_str());
        }
        auto sid = sai_serialize_object_id(entry_id);
        CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, attr));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::createMACsecSA(
    _In_ sai_object_id_t macsec_sa_id,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsec_attr;
    if (loadMACsecAttr(
            SAI_OBJECT_TYPE_MACSEC_SA,
            macsec_sa_id,
            attr_count,
            attr_list,
            macsec_attr) == SAI_STATUS_SUCCESS)
    {
        enable_macsec(macsec_attr);
    }

    auto sid = sai_serialize_object_id(macsec_sa_id);
    return create_internal(SAI_OBJECT_TYPE_MACSEC_SA, sid, switch_id, attr_count, attr_list);
}

sai_status_t SwitchStateBase::removeMACsecPort(
    _In_ sai_object_id_t macsec_port_id)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsec_attr;
    if (loadMACsecAttr(
            SAI_OBJECT_TYPE_MACSEC_PORT,
            macsec_port_id,
            macsec_attr) == SAI_STATUS_SUCCESS)
    {
        delete_macsec_port(macsec_attr);
    }

    auto sid = sai_serialize_object_id(macsec_port_id);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_PORT, sid);
}

sai_status_t SwitchStateBase::removeMACsecSC(
    _In_ sai_object_id_t macsec_sc_id)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsec_attr;
    if (loadMACsecAttr(
            SAI_OBJECT_TYPE_MACSEC_SC,
            macsec_sc_id,
            macsec_attr) == SAI_STATUS_SUCCESS)
    {
        // We cannot only remove egress SC instead of removing Linux MACsec port
        // Because one Linux MACsec port
        // has to include one and can only include one SC
        if (macsec_attr.m_direction == SAI_MACSEC_DIRECTION_INGRESS)
        {
            delete_macsec_ingress_sc(macsec_attr);
        }
    }

    auto sid = sai_serialize_object_id(macsec_sc_id);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_SC, sid);
}

sai_status_t SwitchStateBase::removeMACsecSA(
    _In_ sai_object_id_t macsec_sa_id)
{
    SWSS_LOG_ENTER();

    MACsecAttr macsec_attr;
    if (loadMACsecAttr(
            SAI_OBJECT_TYPE_MACSEC_SA,
            macsec_sa_id,
            macsec_attr) == SAI_STATUS_SUCCESS)
    {
        if (macsec_attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
        {
            delete_macsec_egress_sa(macsec_attr);
        }
        else
        {
            delete_macsec_ingress_sa(macsec_attr);
        }
        
    }

    auto sid = sai_serialize_object_id(macsec_sa_id);
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

sai_status_t SwitchStateBase::findLinePortByMACsecFlow(
        _In_ sai_object_id_t macsec_flow_id,
        _Out_ sai_object_id_t &line_port_id)
{
    SWSS_LOG_ENTER();

    std::vector<sai_attribute_t> attrs;
    sai_attribute_t attr;
    // MACsec flow => ACL entry => ACL table
    // MACsec flow => MACsec SC
    // (ACL table & MACsec SC) => line port
    // Find ACL Entry
    attr.id = SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW;
    attr.value.aclaction.parameter.oid = macsec_flow_id;
    attr.value.aclaction.enable = true;
    std::vector<sai_object_id_t> acl_entry_ids;
    findObjects(SAI_OBJECT_TYPE_ACL_ENTRY, attr, acl_entry_ids);
    if (acl_entry_ids.empty())
    {
        SWSS_LOG_DEBUG(
            "Cannot find corresponding ACL entry for the flow %s",
            sai_serialize_object_id(macsec_flow_id).c_str());
        return SAI_STATUS_FAILURE;
    }
    // Find ACL Table
    sai_object_id_t acl_table_id = SAI_NULL_OBJECT_ID;
    // All entries with same MACsec flow correspond one ACL Table
    // Because an ACL Table corresponds one MACsec Port
    // And a flow never belongs to two MACsec Port
    if (getACLTable(acl_entry_ids.front(), acl_table_id) != SAI_STATUS_SUCCESS || acl_table_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR(
            "Cannot find corresponding ACL table for the entry %s",
            sai_serialize_object_id(acl_entry_ids.front()).c_str());
        return SAI_STATUS_FAILURE;
    }
    // Find MACsec SC
    attr.id = SAI_MACSEC_SC_ATTR_FLOW_ID;
    attr.value.oid = macsec_flow_id;
    std::vector<sai_object_id_t> macsec_sc_ids;
    findObjects(SAI_OBJECT_TYPE_MACSEC_SC, attr, macsec_sc_ids);
    // One MACsec SC will only belong to one MACsec flow
    // Meanwhile one MACsec flow will just belong to one port
    if (macsec_sc_ids.empty())
    {
        SWSS_LOG_ERROR(
            "Cannot find corresponding MACsec SC for the flow %s",
            sai_serialize_object_id(macsec_flow_id).c_str());
        return SAI_STATUS_FAILURE;
    }
    attrs.clear();
    attrs.emplace_back();
    attrs.back().id = SAI_MACSEC_SC_ATTR_MACSEC_DIRECTION;
    CHECK_STATUS(
        get(
            SAI_OBJECT_TYPE_MACSEC_SC,
            macsec_sc_ids.front(),
            static_cast<uint32_t>(attrs.size()),
            attrs.data()));
    auto direction = attrs.back().value.s32;

    // Find line port
    attrs.clear();
    attrs.emplace_back();
    if (direction == SAI_MACSEC_DIRECTION_EGRESS)
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
        SWSS_LOG_ERROR(
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
    line_port_id = line_port_ids.front();
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::findVethNameByLinePort(
        _In_ sai_object_id_t &line_port_id,
        _Out_ std::string &veth_name)
{
    SWSS_LOG_ENTER();
    veth_name.clear();
    for (auto &kvp : m_hostif_info_map)
    {
        if (kvp.second->m_portId == line_port_id)
        {
            veth_name = vs_get_veth_name(kvp.first, line_port_id);
        }
    }
    if (veth_name.empty())
    {
        SWSS_LOG_ERROR(
            "Cannot find corresponding line port %s",
            sai_serialize_object_id(line_port_id).c_str());
        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecAttrFromMACsecPort(
    _In_ sai_object_id_t object_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list,
    _Out_ MACsecAttr &macsec_attr)
{
    SWSS_LOG_ENTER();

    if (attr_list == nullptr || attr_count == 0)
    {
        SWSS_LOG_ERROR("Attribute is empty");
        return SAI_STATUS_FAILURE;
    }

    const sai_attribute_t *attr = nullptr;
    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_PORT_ATTR_MACSEC_DIRECTION, attr_count, attr_list);
    macsec_attr.m_direction = attr->value.s32;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_PORT_ATTR_PORT_ID, attr_count, attr_list);
    auto line_port_id = attr->value.oid;
    CHECK_STATUS(findVethNameByLinePort(line_port_id, macsec_attr.m_veth_name));
    macsec_attr.m_macsec_name = SAI_VS_MACSEC_PREFIX + macsec_attr.m_veth_name;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecAttrFromMACsecSC(
    _In_ sai_object_id_t object_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list,
    _Out_ MACsecAttr &macsec_attr)
{
    SWSS_LOG_ENTER();
    if (attr_list == nullptr || attr_count == 0)
    {
        SWSS_LOG_ERROR("Attribute is empty");
        return SAI_STATUS_FAILURE;
    }

    const sai_attribute_t *attr = nullptr;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SC_ATTR_MACSEC_DIRECTION, attr_count, attr_list);
    macsec_attr.m_direction = attr->value.s32;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SC_ATTR_MACSEC_SCI, attr_count, attr_list);
    auto sci = attr->value.u64;
    std::stringstream sci_convert;
    sci_convert << std::hex << __builtin_bswap64(sci);
    macsec_attr.m_sci = sci_convert.str();

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SC_ATTR_FLOW_ID, attr_count, attr_list);
    auto flow_id = attr->value.oid;

    sai_object_id_t line_port_id = SAI_NULL_OBJECT_ID;
    CHECK_STATUS(findLinePortByMACsecFlow(flow_id, line_port_id));
    CHECK_STATUS(findVethNameByLinePort(line_port_id, macsec_attr.m_veth_name));
    macsec_attr.m_macsec_name = SAI_VS_MACSEC_PREFIX + macsec_attr.m_veth_name;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecAttrFromMACsecSA(
    _In_ sai_object_id_t object_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list,
    _Out_ MACsecAttr &macsec_attr)
{
    SWSS_LOG_ENTER();

    if (attr_list == nullptr || attr_count == 0)
    {
        SWSS_LOG_ERROR("Attribute is empty");
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
    auto sci = attrs[1].value.u64;
    std::stringstream sci_convert;
    sci_convert << std::hex << __builtin_bswap64(sci);
    macsec_attr.m_sci = sci_convert.str();

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_MACSEC_DIRECTION, attr_count, attr_list);
    macsec_attr.m_direction = attr->value.s32;

    // Find veth name
    // TODO, right now the line port is same as port
    sai_object_id_t line_port_id = SAI_NULL_OBJECT_ID;
    CHECK_STATUS(findLinePortByMACsecFlow(flow_id, line_port_id));
    CHECK_STATUS(findVethNameByLinePort(line_port_id, macsec_attr.m_veth_name));
    macsec_attr.m_macsec_name = SAI_VS_MACSEC_PREFIX + macsec_attr.m_veth_name;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_AN, attr_count, attr_list);
    macsec_attr.m_an = attr->value.u8;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_ENCRYPTION_ENABLE, attr_count, attr_list);
    macsec_attr.m_encryption_enable = attr->value.booldata;

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_SAK, attr_count, attr_list);
    macsec_attr.m_sak = sai_serialize_hex_binary(attr->value.macsecsak);

    TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_SAK_256_BITS, attr_count, attr_list);
    if (!attr->value.booldata)
    {
        macsec_attr.m_sak.resize(macsec_attr.m_sak.length() / 2);
    }

    if (macsec_attr.m_direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_XPN, attr_count, attr_list);
    }
    else
    {
        TRY_SAI_METADATA_GET_ATTR_BY_ID(attr, SAI_MACSEC_SA_ATTR_MINIMUM_XPN, attr_count, attr_list);
    }
    macsec_attr.m_pn = attr->value.u64;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecAttr(
    _In_ sai_object_type_t object_type,
    _In_ sai_object_id_t object_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list,
    _Out_ MACsecAttr &macsec_attr)
{
    switch (object_type)
    {
    case SAI_OBJECT_TYPE_MACSEC_PORT:
        return loadMACsecAttrFromMACsecPort(
            object_id,
            attr_count,
            attr_list,
            macsec_attr);
    case SAI_OBJECT_TYPE_MACSEC_SC:
        return loadMACsecAttrFromMACsecSC(
            object_id,
            attr_count,
            attr_list,
            macsec_attr);
    case SAI_OBJECT_TYPE_MACSEC_SA:
        return loadMACsecAttrFromMACsecSA(
            object_id,
            attr_count,
            attr_list,
            macsec_attr);
    default:
        SWSS_LOG_ERROR("Wrong type %s", sai_serialize_object_type(object_type));
        break;
    }
    return SAI_STATUS_FAILURE;
}

sai_status_t SwitchStateBase::loadMACsecAttr(
    _In_ sai_object_type_t object_type,
    _In_ sai_object_id_t object_id,
    _Out_ MACsecAttr &macsec_attr)
{
    SWSS_LOG_ENTER();
    std::vector<sai_attribute_t> attrs;
    if (dumpObject(object_type, object_id, attrs))
    {
        if (loadMACsecAttr(
                object_type,
                object_id,
                static_cast<uint32_t>(attrs.size()),
                attrs.data(),
                macsec_attr) != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_DEBUG(
                "The %s %s is invalid",
                sai_serialize_object_type(object_type).c_str(),
                sai_serialize_object_id(object_id).c_str());
            return SAI_STATUS_FAILURE;
        }
    }
    else
    {
        SWSS_LOG_WARN(
            "The %s %s is not existed",
            sai_serialize_object_type(object_type).c_str(),
            sai_serialize_object_id(object_id).c_str());
        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::loadMACsecAttrsFromACLEntry(
    _In_ sai_object_id_t entry_id,
    _In_ const sai_attribute_t *entry_attr,
    _In_ sai_object_type_t object_type,
    _Out_ std::vector<MACsecAttr> &macsec_attrs)
{
    SWSS_LOG_ENTER();

    if (entry_attr == nullptr || entry_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("entry is empty");
        return SAI_STATUS_FAILURE;
    }

    if (entry_attr->id != SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW)
    {
        SWSS_LOG_ERROR("Not include MACsec flow information");
        return SAI_STATUS_FAILURE;
    }
    auto flow_id = entry_attr->value.aclaction.parameter.oid;

    macsec_attrs.clear();
    sai_attribute_t attr;

    if (object_type == SAI_OBJECT_TYPE_MACSEC_PORT)
    {
        sai_object_id_t line_port_id = SAI_NULL_OBJECT_ID;
        CHECK_STATUS(findLinePortByMACsecFlow(flow_id, line_port_id));
        macsec_attrs.emplace_back();
        if (findVethNameByLinePort(
                line_port_id,
                macsec_attrs.back().m_veth_name) != SAI_STATUS_SUCCESS)
        {
            // The fail log has been record in findVethNameByLinePort
            macsec_attrs.clear();
            return SAI_STATUS_FAILURE;
        }
        macsec_attrs.back().m_macsec_name = SAI_VS_MACSEC_PREFIX + macsec_attrs.back().m_veth_name;
        return SAI_STATUS_SUCCESS;
    }

    // Find all MACsec SCs that use this flow
    std::vector<sai_object_id_t> macsec_scs;
    attr.id = SAI_MACSEC_SC_ATTR_FLOW_ID;
    attr.value.oid = flow_id;
    findObjects(SAI_OBJECT_TYPE_MACSEC_SC, attr, macsec_scs);
    if (object_type == SAI_OBJECT_TYPE_MACSEC_SC)
    {
        if (macsec_scs.empty())
        {
            SWSS_LOG_DEBUG(
                "No one MACsec SC is using the ACL entry %s",
                sai_serialize_object_id(entry_id).c_str());
        }
        macsec_attrs.reserve(macsec_scs.size());
        for (auto sc_id : macsec_scs)
        {
            macsec_attrs.emplace_back();
            if (loadMACsecAttr(
                    SAI_OBJECT_TYPE_MACSEC_SC,
                    sc_id,
                    macsec_attrs.back()) != SAI_STATUS_SUCCESS)
            {
                // The fail log has been recorded at loadMACsecAttr
                macsec_attrs.pop_back();
            }
        }
        return SAI_STATUS_SUCCESS;
    }

    if (object_type == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        // Find all MACsec SAs that use this entry
        std::vector<sai_object_id_t> macsec_sas;
        for (auto sc_id : macsec_scs)
        {
            attr.id = SAI_MACSEC_SA_ATTR_SC_ID;
            attr.value.oid = sc_id;
            findObjects(SAI_OBJECT_TYPE_MACSEC_SA, attr, macsec_sas);
        }
        if (macsec_sas.empty())
        {
            SWSS_LOG_DEBUG(
                "No one MACsec SA is using the ACL entry %s",
                sai_serialize_object_id(entry_id).c_str());
        }
        macsec_attrs.reserve(macsec_sas.size());
        for (auto sa_id : macsec_sas)
        {
            macsec_attrs.emplace_back();
            if (loadMACsecAttr(
                    SAI_OBJECT_TYPE_MACSEC_SA,
                    sa_id,
                    macsec_attrs.back()) != SAI_STATUS_SUCCESS)
            {
                // The fail log has been recorded at loadMACsecAttr
                macsec_attrs.pop_back();
            }
        }
        return SAI_STATUS_SUCCESS;
    }
    return SAI_STATUS_NOT_IMPLEMENTED;
}
