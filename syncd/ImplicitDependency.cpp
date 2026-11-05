/*
 * SPDX-FileCopyrightText: Nexthop Systems Inc.
 * Copyright (c) 2026 Nexthop Systems Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ImplicitDependency.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace syncd;

static const sai_attribute_t* findAttr(
        _In_ sai_attr_id_t id,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < attrCount; idx++)
    {
        if (attrList[idx].id == id)
        {
            return &attrList[idx];
        }
    }

    return nullptr;
}

static sai_int32_t getEnumOrDefault(
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t id,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    if (auto attr = findAttr(id, attrCount, attrList))
    {
        return attr->value.s32;
    }

    auto meta = sai_metadata_get_attr_metadata(objectType, id);

    if (meta == nullptr || meta->defaultvalue == nullptr)
    {
        SWSS_LOG_THROW("no default value for %s attribute %d",
                sai_serialize_object_type(objectType).c_str(), id);
    }

    return meta->defaultvalue->s32;
}

/*
 * Key of a local VXLAN endpoint IP.
 */
static std::string getVxlanEndpointKey(
        _In_ const sai_ip_address_t& ip)
{
    SWSS_LOG_ENTER();

    return "SAI_TUNNEL_TYPE_VXLAN:" + sai_serialize_ip_address(ip);
}

/*
 * A VXLAN tunnel's local endpoint: its source IP. Empty for other tunnel
 * types, when the tunnel has no source IP, or when its peer mode is not the
 * given one.
 */
static std::string getTunnelEndpointKey(
        _In_ sai_tunnel_peer_mode_t peerMode,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    auto type = findAttr(SAI_TUNNEL_ATTR_TYPE, attrCount, attrList);

    if (type == nullptr || type->value.s32 != SAI_TUNNEL_TYPE_VXLAN)
    {
        return "";
    }

    if (getEnumOrDefault(SAI_OBJECT_TYPE_TUNNEL, SAI_TUNNEL_ATTR_PEER_MODE, attrCount, attrList) != peerMode)
    {
        return "";
    }

    auto srcIp = findAttr(SAI_TUNNEL_ATTR_ENCAP_SRC_IP, attrCount, attrList);

    if (srcIp == nullptr)
    {
        return "";
    }

    return getVxlanEndpointKey(srcIp->value.ipaddr);
}

/*
 * A P2MP VXLAN tunnel termination entry's local endpoint: its destination IP.
 * Empty for other entries.
 */
static std::string getTunnelTermEndpointKey(
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    auto type = findAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE, attrCount, attrList);
    auto tunnelType = findAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE, attrCount, attrList);
    auto dstIp = findAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP, attrCount, attrList);

    if (type == nullptr || type->value.s32 != SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2MP ||
            tunnelType == nullptr || tunnelType->value.s32 != SAI_TUNNEL_TYPE_VXLAN ||
            dstIp == nullptr)
    {
        return "";
    }

    return getVxlanEndpointKey(dstIp->value.ipaddr);
}

bool ImplicitDependency::hasImplicitDependencies(
        _In_ sai_object_type_t objectType)
{
    SWSS_LOG_ENTER();

    return objectType == SAI_OBJECT_TYPE_TUNNEL ||
        objectType == SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY;
}

const std::vector<sai_object_type_t>& ImplicitDependency::getObjectTypes()
{
    SWSS_LOG_ENTER();

    static const std::vector<sai_object_type_t> types = {
        SAI_OBJECT_TYPE_TUNNEL,
        SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY,
    };

    return types;
}

std::string ImplicitDependency::getProviderKey(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    switch (objectType)
    {
        case SAI_OBJECT_TYPE_TUNNEL:
            return getTunnelEndpointKey(SAI_TUNNEL_PEER_MODE_P2MP, attrCount, attrList);

        case SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY:
            return getTunnelTermEndpointKey(attrCount, attrList);

        default:
            return "";
    }
}

std::string ImplicitDependency::getDependantKey(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    if (objectType == SAI_OBJECT_TYPE_TUNNEL)
    {
        return getTunnelEndpointKey(SAI_TUNNEL_PEER_MODE_P2P, attrCount, attrList);
    }

    return "";
}
