#include "sai_serialize.h"
#include "meta/sai_meta.h"
#include "swss/tokenize.h"
#include "swss/json.hpp"

#include <vector>
#include <climits>

#include <arpa/inet.h>
#include <errno.h>

using json = nlohmann::json;

int char_to_int(
        _In_ const char c)
{
    SWSS_LOG_ENTER();

    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    SWSS_LOG_THROW("unable to convert char %d to int", c);
}

template<class T, typename U>
T* sai_alloc_n_of_ptr_type(U count, T*)
{
    SWSS_LOG_ENTER();

    return new T[count];
}

template<typename T, typename U>
void sai_alloc_list(
        _In_ T count,
        _In_ U &element)
{
    SWSS_LOG_ENTER();

    element.count = count;
    element.list = sai_alloc_n_of_ptr_type(count, element.list);
}

template<typename T>
void sai_free_list(
        _In_ T &element)
{
    SWSS_LOG_ENTER();

    delete[] element.list;
    element.list = NULL;
}

template<typename T>
void transfer_primitive(
        _In_ const T &src_element,
        _In_ T &dst_element)
{
    SWSS_LOG_ENTER();

    const unsigned char* src_mem = reinterpret_cast<const unsigned char*>(&src_element);
    unsigned char* dst_mem = reinterpret_cast<unsigned char*>(&dst_element);

    memcpy(dst_mem, src_mem, sizeof(T));
}

template<typename T>
sai_status_t transfer_list(
        _In_ const T &src_element,
        _In_ T &dst_element,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    if (countOnly || dst_element.count == 0)
    {
        transfer_primitive(src_element.count, dst_element.count);
        return SAI_STATUS_SUCCESS;
    }

    if (dst_element.list == NULL)
    {
        SWSS_LOG_ERROR("destination list is null, unable to transfer elements");

        return SAI_STATUS_FAILURE;
    }

    if (dst_element.count >= src_element.count)
    {
        if (src_element.list == NULL && src_element.count > 0)
        {
            SWSS_LOG_THROW("source list is NULL when count is %u, wrong db insert?", src_element.count);
        }

        transfer_primitive(src_element.count, dst_element.count);

        for (size_t i = 0; i < src_element.count; i++)
        {
            transfer_primitive(src_element.list[i], dst_element.list[i]);
        }

        return SAI_STATUS_SUCCESS;
    }

    // input buffer is too small to get all list elements, so return count only
    transfer_primitive(src_element.count, dst_element.count);

    return SAI_STATUS_BUFFER_OVERFLOW;
}

#define RETURN_ON_ERROR(x)\
{\
    sai_status_t s = (x);\
    if (s != SAI_STATUS_SUCCESS)\
        return s;\
}

sai_status_t transfer_attribute(
        _In_ sai_attr_value_type_t serialization_type,
        _In_ const sai_attribute_t &src_attr,
        _In_ sai_attribute_t &dst_attr,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    switch (serialization_type)
    {
        case SAI_ATTR_VALUE_TYPE_BOOL:
            transfer_primitive(src_attr.value.booldata, dst_attr.value.booldata);
            break;

        case SAI_ATTR_VALUE_TYPE_CHARDATA:
            transfer_primitive(src_attr.value.chardata, dst_attr.value.chardata);
            break;

        case SAI_ATTR_VALUE_TYPE_UINT8:
            transfer_primitive(src_attr.value.u8, dst_attr.value.u8);
            break;

        case SAI_ATTR_VALUE_TYPE_INT8:
            transfer_primitive(src_attr.value.s8, dst_attr.value.s8);
            break;

        case SAI_ATTR_VALUE_TYPE_UINT16:
            transfer_primitive(src_attr.value.u16, dst_attr.value.u16);
            break;

        case SAI_ATTR_VALUE_TYPE_INT16:
            transfer_primitive(src_attr.value.s16, dst_attr.value.s16);
            break;

        case SAI_ATTR_VALUE_TYPE_UINT32:
            transfer_primitive(src_attr.value.u32, dst_attr.value.u32);
            break;

        case SAI_ATTR_VALUE_TYPE_INT32:
            transfer_primitive(src_attr.value.s32, dst_attr.value.s32);
            break;

        case SAI_ATTR_VALUE_TYPE_UINT64:
            transfer_primitive(src_attr.value.u64, dst_attr.value.u64);
            break;

        case SAI_ATTR_VALUE_TYPE_INT64:
            transfer_primitive(src_attr.value.s64, dst_attr.value.s64);
            break;

        case SAI_ATTR_VALUE_TYPE_MAC:
            transfer_primitive(src_attr.value.mac, dst_attr.value.mac);
            break;

        case SAI_ATTR_VALUE_TYPE_IPV4:
            transfer_primitive(src_attr.value.ip4, dst_attr.value.ip4);
            break;

        case SAI_ATTR_VALUE_TYPE_IPV6:
            transfer_primitive(src_attr.value.ip6, dst_attr.value.ip6);
            break;

        case SAI_ATTR_VALUE_TYPE_POINTER:
            transfer_primitive(src_attr.value.ptr, dst_attr.value.ptr);
            break;

        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS:
            transfer_primitive(src_attr.value.ipaddr, dst_attr.value.ipaddr);
            break;

        case SAI_ATTR_VALUE_TYPE_IP_PREFIX:
            transfer_primitive(src_attr.value.ipprefix, dst_attr.value.ipprefix);
            break;

        case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
            transfer_primitive(src_attr.value.oid, dst_attr.value.oid);
            break;

        case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.objlist, dst_attr.value.objlist, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_UINT8_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.u8list, dst_attr.value.u8list, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_INT8_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.s8list, dst_attr.value.s8list, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_UINT16_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.u16list, dst_attr.value.u16list, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_INT16_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.s16list, dst_attr.value.s16list, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_UINT32_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.u32list, dst_attr.value.u32list, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_INT32_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.s32list, dst_attr.value.s32list, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_UINT32_RANGE:
            transfer_primitive(src_attr.value.u32range, dst_attr.value.u32range);
            break;

        case SAI_ATTR_VALUE_TYPE_INT32_RANGE:
            transfer_primitive(src_attr.value.s32range, dst_attr.value.s32range);
            break;

        case SAI_ATTR_VALUE_TYPE_VLAN_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.vlanlist, dst_attr.value.vlanlist, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.qosmap, dst_attr.value.qosmap, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_RESOURCE_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.aclresource, dst_attr.value.aclresource, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.ipaddrlist, dst_attr.value.ipaddrlist, countOnly));
            break;

            /* ACL FIELD DATA */

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.data.booldata, dst_attr.value.aclfield.data.booldata);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.u8, dst_attr.value.aclfield.mask.u8);
            transfer_primitive(src_attr.value.aclfield.data.u8, dst_attr.value.aclfield.data.u8);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.s8, dst_attr.value.aclfield.mask.s8);
            transfer_primitive(src_attr.value.aclfield.data.s8, dst_attr.value.aclfield.data.s8);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.u16, dst_attr.value.aclfield.mask.u16);
            transfer_primitive(src_attr.value.aclfield.data.u16, dst_attr.value.aclfield.data.u16);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.s16, dst_attr.value.aclfield.mask.s16);
            transfer_primitive(src_attr.value.aclfield.data.s16, dst_attr.value.aclfield.data.s16);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.u16, dst_attr.value.aclfield.mask.u16);
            transfer_primitive(src_attr.value.aclfield.data.u16, dst_attr.value.aclfield.data.u16);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.s32, dst_attr.value.aclfield.mask.s32);
            transfer_primitive(src_attr.value.aclfield.data.s32, dst_attr.value.aclfield.data.s32);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.mac, dst_attr.value.aclfield.mask.mac);
            transfer_primitive(src_attr.value.aclfield.data.mac, dst_attr.value.aclfield.data.mac);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.ip4, dst_attr.value.aclfield.mask.ip4);
            transfer_primitive(src_attr.value.aclfield.data.ip4, dst_attr.value.aclfield.data.ip4);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.ip6, dst_attr.value.aclfield.mask.ip6);
            transfer_primitive(src_attr.value.aclfield.data.ip6, dst_attr.value.aclfield.data.ip6);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.data.oid, dst_attr.value.aclfield.data.oid);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            RETURN_ON_ERROR(transfer_list(src_attr.value.aclfield.data.objlist, dst_attr.value.aclfield.data.objlist, countOnly));
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            RETURN_ON_ERROR(transfer_list(src_attr.value.aclfield.mask.u8list, dst_attr.value.aclfield.mask.u8list, countOnly));
            transfer_list(src_attr.value.aclfield.data.u8list, dst_attr.value.aclfield.data.u8list, countOnly);
            break;

            /* ACL ACTION DATA */

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.booldata, dst_attr.value.aclaction.parameter.booldata);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.u8, dst_attr.value.aclaction.parameter.u8);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.s8, dst_attr.value.aclaction.parameter.s8);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.u16, dst_attr.value.aclaction.parameter.u16);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.s16, dst_attr.value.aclaction.parameter.s16);
            break;


        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.u32, dst_attr.value.aclaction.parameter.u32);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.s32, dst_attr.value.aclaction.parameter.s32);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.mac, dst_attr.value.aclaction.parameter.mac);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.ip4, dst_attr.value.aclaction.parameter.ip4);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.ip6, dst_attr.value.aclaction.parameter.ip6);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.oid, dst_attr.value.aclaction.parameter.oid);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            RETURN_ON_ERROR(transfer_list(src_attr.value.aclaction.parameter.objlist, dst_attr.value.aclaction.parameter.objlist, countOnly));
            break;

        default:
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t transfer_attributes(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *src_attr_list,
        _In_ sai_attribute_t *dst_attr_list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < attr_count; i++)
    {
        const sai_attribute_t &src_attr = src_attr_list[i];
        sai_attribute_t &dst_attr = dst_attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, src_attr.id);

        if (src_attr.id != dst_attr.id)
        {
            SWSS_LOG_THROW("src vs dst attr id don't match GET mismatch");
        }

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(object_type).c_str(),
                    src_attr.id);
        }

        RETURN_ON_ERROR(transfer_attribute(meta->attrvaluetype, src_attr, dst_attr, countOnly));
    }

    return SAI_STATUS_SUCCESS;
}

// util

uint8_t get_ip_mask(
        _In_ const uint8_t* mask,
        _In_ bool ipv6)
{
    SWSS_LOG_ENTER();

    uint8_t ones = 0;
    bool zeros = false;

    uint8_t count = ipv6 ? 128 : 32;

    for (uint8_t i = 0; i < count; i++)
    {
        bool bit = (mask[i/8]) & (1 << (7 - (i%8)));

        if (zeros && bit)
        {
            SWSS_LOG_THROW("FATAL: invalid ipv%d mask", ipv6 ? 6 : 4);
        }

        zeros |= !bit;

        if (bit)
        {
            ones++;
        }
    }

    return ones;
}

uint8_t get_ipv4_mask(
        _In_ const sai_ip4_t mask)
{
    SWSS_LOG_ENTER();

    return get_ip_mask((const uint8_t*)&mask, false);
}

int get_ipv6_mask(
        _In_ const sai_ip6_t &mask)
{
    SWSS_LOG_ENTER();

    return get_ip_mask(mask, true);
}

void sai_populate_ip_mask(
        _In_ uint8_t bits,
        _Out_ uint8_t *mask,
        _In_ bool ipv6)
{
    SWSS_LOG_ENTER();

    if (mask == NULL)
    {
        SWSS_LOG_THROW("mask pointer is null");
    }

    if ((ipv6 && (bits > 128)) || (!ipv6 && (bits > 32)))
    {
        SWSS_LOG_THROW("invalid ip mask bits %u", bits);
    }

    // zero all mask
    memset(mask, 0, ipv6 ? 16 : 4);

    // set full bytes to all ones
    for (uint8_t i = 0; i < bits/8; i++)
    {
        mask[i] = 0xFF;
    }

    int rem = bits % 8;

    if (rem)
    {
        // set last non zero byte
        mask[bits/8] =(uint8_t)(0xFF << (8 - rem));
    }
}


// new methods

std::string sai_serialize_bool(
        _In_ bool b)
{
    SWSS_LOG_ENTER();

    return b ? "true" : "false";
}

#define CHAR_LEN 32

std::string sai_serialize_chardata(
        _In_ const char data[CHAR_LEN])
{
    SWSS_LOG_ENTER();

    std::string s;

    char buf[8];

    size_t len = strnlen(data, CHAR_LEN);

    for (size_t i = 0; i < len; ++i)
    {
        unsigned char c = (unsigned char)data[i];

        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        {
            s += c;
            continue;
        }

        if (c == '\\')
        {
            s += "\\\\";
            continue;
        }

        snprintf(buf, sizeof(buf), "\\x%02X", (int)c);

        s += buf;
    }

    return s;
}

std::string sai_serialize_mac(
        _In_ const sai_mac_t mac)
{
    SWSS_LOG_ENTER();

    char buf[32];

    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return buf;
}

template <typename T>
std::string sai_serialize_number(
        _In_ const T number,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    if (hex)
    {
        char buf[32];

        snprintf(buf, sizeof(buf), "0x%lx", (uint64_t)number);

        return buf;
    }

    return std::to_string(number);
}

std::string sai_serialize_enum(
        _In_ const int32_t value,
        _In_ const sai_enum_metadata_t* meta)
{
    SWSS_LOG_ENTER();

    if (meta == NULL)
    {
        return sai_serialize_number(value);
    }

    for (size_t i = 0; i < meta->valuescount; ++i)
    {
        if (meta->values[i] == value)
        {
            return meta->valuesnames[i];
        }
    }

    SWSS_LOG_WARN("enum value %d not found in enum %s", value, meta->name);

    return sai_serialize_number(value);
}

std::string sai_serialize_number(
        _In_ const uint32_t number,
        _In_ bool hex)
{
    SWSS_LOG_ENTER();

    return sai_serialize_number<uint32_t>(number, hex);
}

std::string sai_serialize_attr_id(
        _In_ const sai_attr_metadata_t& meta)
{
    SWSS_LOG_ENTER();

    return meta.attridname;
}

std::string sai_serialize_status(
        _In_ const sai_status_t status)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(status, &sai_metadata_enum_sai_status_t);
}

std::string sai_serialize_common_api(
        _In_ const sai_common_api_t common_api)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(common_api, &sai_metadata_enum_sai_common_api_t);
}

std::string sai_serialize_object_type(
        _In_ const sai_object_type_t object_type)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(object_type, &sai_metadata_enum_sai_object_type_t);
}

std::string sai_serialize_attr_value_type(
        _In_ const sai_attr_value_type_t attr_value_type)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(attr_value_type, &sai_metadata_enum_sai_attr_value_type_t);
}

std::string sai_serialize_packet_color(
        _In_ sai_packet_color_t color)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(color, &sai_metadata_enum_sai_packet_color_t);
}

std::string sai_serialize_vlan_id(
        _In_ sai_vlan_id_t vlan_id)
{
    SWSS_LOG_ENTER();

    return sai_serialize_number(vlan_id);
}

std::string sai_serialize_neighbor_entry(
        _In_ const sai_neighbor_entry_t &ne)
{
    SWSS_LOG_ENTER();

    json j;

    j["switch_id"] = sai_serialize_object_id(ne.switch_id);
    j["rif"] = sai_serialize_object_id(ne.rif_id);
    j["ip"] = sai_serialize_ip_address(ne.ip_address);

    return j.dump();
}

std::string sai_serialize_route_entry(
        _In_ const sai_route_entry_t& route_entry)
{
    SWSS_LOG_ENTER();

    json j;

    j["switch_id"] = sai_serialize_object_id(route_entry.switch_id);
    j["vr"] = sai_serialize_object_id(route_entry.vr_id);
    j["dest"] = sai_serialize_ip_prefix(route_entry.destination);

    return j.dump();
}

std::string sai_serialize_ipmc_entry(
        _In_ const sai_ipmc_entry_t& ipmc_entry)
{
    SWSS_LOG_ENTER();

    json j;

    j["switch_id"] = sai_serialize_object_id(ipmc_entry.switch_id);
    j["vr_id"] = sai_serialize_object_id(ipmc_entry.vr_id);
    j["type"] = sai_serialize_ipmc_entry_type(ipmc_entry.type);
    j["destination"] = sai_serialize_ip_address(ipmc_entry.destination);
    j["sourte"] = sai_serialize_ip_address(ipmc_entry.source);

    return j.dump();
}

std::string sai_serialize_l2mc_entry(
        _In_ const sai_l2mc_entry_t& l2mc_entry)
{
    SWSS_LOG_ENTER();

    json j;

    j["switch_id"] = sai_serialize_object_id(l2mc_entry.switch_id);
    j["bv_id"] = sai_serialize_object_id(l2mc_entry.bv_id);
    j["type"] = sai_serialize_l2mc_entry_type(l2mc_entry.type);
    j["destination"] = sai_serialize_ip_address(l2mc_entry.destination);
    j["sourte"] = sai_serialize_ip_address(l2mc_entry.source);

    return j.dump();
}

std::string sai_serialize_mcast_fdb_entry(
        _In_ const sai_mcast_fdb_entry_t& mcast_fdb_entry)
{
    SWSS_LOG_ENTER();

    json j;

    j["switch_id"] = sai_serialize_object_id(mcast_fdb_entry.switch_id);
    j["bv_id"] = sai_serialize_object_id(mcast_fdb_entry.bv_id);
    j["mac_address"] = sai_serialize_mac(mcast_fdb_entry.mac_address);

    return j.dump();
}

std::string sai_serialize_inseg_entry(
        _In_ const sai_inseg_entry_t& inseg_entry)
{
    SWSS_LOG_ENTER();

    json j;

    j["switch_id"] = sai_serialize_object_id(inseg_entry.switch_id);
    j["label"] = sai_serialize_number(inseg_entry.label);

    return j.dump();
}

std::string sai_serialize_fdb_entry(
        _In_ const sai_fdb_entry_t& fdb_entry)
{
    SWSS_LOG_ENTER();

    json j;

    j["switch_id"] = sai_serialize_object_id(fdb_entry.switch_id);
    j["mac"] = sai_serialize_mac(fdb_entry.mac_address);
    j["bvid"] = sai_serialize_object_id(fdb_entry.bv_id);

    return j.dump();
}

std::string sai_serialize_l2mc_entry_type(
        _In_ const sai_l2mc_entry_type_t type)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(type, &sai_metadata_enum_sai_l2mc_entry_type_t);
}

std::string sai_serialize_ipmc_entry_type(
        _In_ const sai_ipmc_entry_type_t type)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(type, &sai_metadata_enum_sai_ipmc_entry_type_t);
}

std::string sai_serialize_port_stat(
        _In_ const sai_port_stat_t counter)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(counter, &sai_metadata_enum_sai_port_stat_t);
}

std::string sai_serialize_port_pool_stat(
        _In_ const sai_port_pool_stat_t counter)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(counter, &sai_metadata_enum_sai_port_pool_stat_t);
}

std::string sai_serialize_queue_stat(
        _In_ const sai_queue_stat_t counter)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(counter, &sai_metadata_enum_sai_queue_stat_t);
}

std::string sai_serialize_router_interface_stat(
        _In_ const sai_router_interface_stat_t counter)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(counter, &sai_metadata_enum_sai_router_interface_stat_t);
}

std::string sai_serialize_ingress_priority_group_stat(
        _In_ const sai_ingress_priority_group_stat_t counter)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(counter, &sai_metadata_enum_sai_ingress_priority_group_stat_t);
}

std::string sai_serialize_ingress_priority_group_attr(
        _In_ const sai_ingress_priority_group_attr_t attr)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(attr, &sai_metadata_enum_sai_ingress_priority_group_attr_t);
}

std::string sai_serialize_tunnel_stat(
        _In_ const sai_tunnel_stat_t counter)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(counter, &sai_metadata_enum_sai_tunnel_stat_t);
}

std::string sai_serialize_queue_attr(
        _In_ const sai_queue_attr_t attr)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(attr, &sai_metadata_enum_sai_queue_attr_t);
}

std::string sai_serialize_switch_oper_status(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t status)
{
    SWSS_LOG_ENTER();

    json j;

    j["switch_id"] = sai_serialize_object_id(switch_id);
    j["status"] = sai_serialize_enum(status, &sai_metadata_enum_sai_switch_oper_status_t);

    return j.dump();
}

std::string sai_serialize_switch_shutdown_request(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    json j;

    j["switch_id"] = sai_serialize_object_id(switch_id);

    return j.dump();
}

std::string sai_serialize_ipv4(
        _In_ sai_ip4_t ip)
{
    SWSS_LOG_ENTER();

    char buf[INET_ADDRSTRLEN];

    struct sockaddr_in sa;

    memcpy(&sa.sin_addr, &ip, 4);

    if (inet_ntop(AF_INET, &(sa.sin_addr), buf, INET_ADDRSTRLEN) == NULL)
    {
        SWSS_LOG_THROW("FATAL: failed to convert IPv4 address, errno: %s", strerror(errno));
    }

    return buf;
}

std::string sai_serialize_pointer(
        _In_ sai_pointer_t ptr)
{
    SWSS_LOG_ENTER();

    return sai_serialize_number((uint64_t)ptr, true);
}

std::string sai_serialize_ipv6(
        _In_ const sai_ip6_t& ip)
{
    SWSS_LOG_ENTER();

    char buf[INET6_ADDRSTRLEN];

    struct sockaddr_in6 sa6;

    memcpy(&sa6.sin6_addr, ip, 16);

    if (inet_ntop(AF_INET6, &(sa6.sin6_addr), buf, INET6_ADDRSTRLEN) == NULL)
    {
        SWSS_LOG_THROW("FATAL: failed to convert IPv6 address, errno: %s", strerror(errno));
    }

    return buf;
}

std::string sai_serialize_ip_address(
        _In_ const sai_ip_address_t& ipaddress)
{
    SWSS_LOG_ENTER();

    switch (ipaddress.addr_family)
    {
        case SAI_IP_ADDR_FAMILY_IPV4:

            return sai_serialize_ipv4(ipaddress.addr.ip4);

        case SAI_IP_ADDR_FAMILY_IPV6:

            return sai_serialize_ipv6(ipaddress.addr.ip6);

        default:

            SWSS_LOG_THROW("FATAL: invalid ip address family: %d", ipaddress.addr_family);
    }
}

std::string sai_serialize_object_id(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    char buf[32];

    snprintf(buf, sizeof(buf), "oid:0x%lx", oid);

    return buf;
}

template<typename T, typename F>
std::string sai_serialize_list(
        _In_ const T& list,
        _In_ bool countOnly,
        F serialize_item)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_number(list.count);

    if (countOnly)
    {
        return s;
    }

    if (list.list == NULL || list.count == 0)
    {
        return s + ":null";
    }

    std::string l;

    for (uint32_t i = 0; i < list.count; ++i)
    {
        l += serialize_item(list.list[i]);

        if (i != list.count -1)
        {
            l += ",";
        }
    }

    return s + ":" + l;

}

std::string sai_serialize_ip_address_list(
        _In_ const sai_ip_address_list_t& list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    return sai_serialize_list(list, countOnly, [&](sai_ip_address_t item) { return sai_serialize_ip_address(item);} );
}

std::string sai_serialize_enum_list(
        _In_ const sai_s32_list_t& list,
        _In_ const sai_enum_metadata_t* meta,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    return sai_serialize_list(list, countOnly, [&](int32_t item) { return sai_serialize_enum(item, meta);} );
}

std::string sai_serialize_oid_list(
        _In_ const sai_object_list_t &list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    return sai_serialize_list(list, countOnly, [&](sai_object_id_t item) { return sai_serialize_object_id(item);} );
}

template <typename T>
std::string sai_serialize_number_list(
        _In_ const T& list,
        _In_ bool countOnly,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    return sai_serialize_list(list, countOnly, [&](decltype(*list.list)& item) { return sai_serialize_number(item, hex);} );
}

json sai_serialize_qos_map_params(
        _In_ const sai_qos_map_params_t& params)
{
    SWSS_LOG_ENTER();

    json j;

    j["tc"]     = params.tc;
    j["dscp"]   = params.dscp;
    j["dot1p"]  = params.dot1p;
    j["prio"]   = params.prio;
    j["pg"]     = params.pg;
    j["qidx"]   = params.queue_index;
    j["color"]  = sai_serialize_packet_color(params.color);

    return j;
}

json sai_serialize_qos_map(
        _In_ const sai_qos_map_t& qosmap)
{
    SWSS_LOG_ENTER();

    json j;

    j["key"]    = sai_serialize_qos_map_params(qosmap.key);
    j["value"]  = sai_serialize_qos_map_params(qosmap.value);;

    return j;
}

std::string sai_serialize_qos_map_item(
        _In_ const sai_qos_map_t& qosmap)
{
    SWSS_LOG_ENTER();

    json j;

    j["key"]    = sai_serialize_qos_map_params(qosmap.key);
    j["value"]  = sai_serialize_qos_map_params(qosmap.value);;

    return j.dump();
}

std::string sai_serialize_qos_map_list(
        _In_ const sai_qos_map_list_t& qosmap,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    json j;

    j["count"] = qosmap.count;

    if (qosmap.list == NULL || countOnly)
    {
        j["list"] = nullptr;

        return j.dump();
    }

    json arr = json::array();

    for (uint32_t i = 0; i < qosmap.count; ++i)
    {
        json item = sai_serialize_qos_map(qosmap.list[i]);

        arr.push_back(item);
    }

    j["list"] = arr;

    return j.dump();
}

json sai_serialize_acl_resource(
        _In_ const sai_acl_resource_t& aclresource)
{
    SWSS_LOG_ENTER();

    json j;

    j["stage"]    = sai_serialize_enum(aclresource.stage, &sai_metadata_enum_sai_acl_stage_t);
    j["bind_point"]  = sai_serialize_enum(aclresource.bind_point, &sai_metadata_enum_sai_acl_bind_point_type_t);
    j["avail_num"]  = sai_serialize_number(aclresource.avail_num);

    return j;
}

std::string sai_serialize_acl_resource_list(
        _In_ const sai_acl_resource_list_t& aclresource,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    json j;

    j["count"] = aclresource.count;

    if (aclresource.list == NULL || countOnly)
    {
        j["list"] = nullptr;

        return j.dump();
    }

    json arr = json::array();

    for (uint32_t i = 0; i < aclresource.count; ++i)
    {
        json item = sai_serialize_acl_resource(aclresource.list[i]);

        arr.push_back(item);
    }

    j["list"] = arr;

    return j.dump();
}

template <typename T>
std::string sai_serialize_range(
        _In_ const T& range)
{
    SWSS_LOG_ENTER();

    return sai_serialize_number(range.min) + "," + sai_serialize_number(range.max);
}

std::string sai_serialize_acl_action(
        _In_ const sai_attr_metadata_t& meta,
        _In_ const sai_acl_action_data_t& action,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    if (action.enable == false)
    {
        // parameter is not needed when action is disabled
        return "disabled";
    }

    switch (meta.attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
            return sai_serialize_bool(action.parameter.booldata);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
            return sai_serialize_number(action.parameter.u8);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
            return sai_serialize_number(action.parameter.s8);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
            return sai_serialize_number(action.parameter.u16);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
            return sai_serialize_number(action.parameter.s16);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
            return sai_serialize_number(action.parameter.u32);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
            return sai_serialize_enum(action.parameter.s32, meta.enummetadata);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
            return sai_serialize_mac(action.parameter.mac);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
            return sai_serialize_ipv4(action.parameter.ip4);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
            return sai_serialize_ipv6(action.parameter.ip6);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            return sai_serialize_object_id(action.parameter.oid);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            return sai_serialize_oid_list(action.parameter.objlist, countOnly);

        default:
            SWSS_LOG_THROW("FATAIL: invalid serialization type %d", meta.attrvaluetype);
    }
}

std::string sai_serialize_acl_field(
        _In_ const sai_attr_metadata_t& meta,
        _In_ const sai_acl_field_data_t& field,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    if (field.enable == false)
    {
        // parameter is not needed when field is disabled
        return "disabled";
    }

    switch (meta.attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
            return sai_serialize_bool(field.data.booldata);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
            return sai_serialize_number(field.data.u8) + "&mask:" + sai_serialize_number(field.mask.u8, true);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
            return sai_serialize_number(field.data.s8) + "&mask:" + sai_serialize_number(field.mask.s8, true);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
            return sai_serialize_number(field.data.u16) + "&mask:" + sai_serialize_number(field.mask.u16, true);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
            return sai_serialize_number(field.data.s16) + "&mask:" + sai_serialize_number(field.mask.s16, true);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
            return sai_serialize_number(field.data.u32) + "&mask:" + sai_serialize_number(field.mask.u32, true);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
            return sai_serialize_enum(field.data.s32, meta.enummetadata) + "&mask:" + sai_serialize_number(field.mask.s32, true);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
            return sai_serialize_mac(field.data.mac) +"&mask:" + sai_serialize_mac(field.mask.mac);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
            return sai_serialize_ipv4(field.data.ip4) +"&mask:" + sai_serialize_ipv4(field.mask.ip4);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
            return sai_serialize_ipv6(field.data.ip6) +"&mask:" + sai_serialize_ipv6(field.mask.ip6);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            return sai_serialize_object_id(field.data.oid);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            return sai_serialize_oid_list(field.data.objlist, countOnly);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
            return sai_serialize_number_list(field.data.u8list, countOnly) + "&mask:" + sai_serialize_number_list(field.mask.u8list, countOnly, true);

        default:
            SWSS_LOG_THROW("FATAIL: invalid serialization type %d", meta.attrvaluetype);
    }
}

std::string sai_serialize_acl_capability(
        _In_ const sai_attr_metadata_t &meta,
        _In_ const sai_acl_capability_t &cap,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    auto mandatory = sai_serialize_bool(cap.is_action_list_mandatory);

    auto list = sai_serialize_enum_list(cap.action_list, &sai_metadata_enum_sai_acl_action_type_t, countOnly);

    return mandatory + ":" + list;
}

std::string sai_serialize_attr_value(
        _In_ const sai_attr_metadata_t& meta,
        _In_ const sai_attribute_t &attr,
        _In_ const bool countOnly)
{
    SWSS_LOG_ENTER();

    switch (meta.attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_BOOL:
            return sai_serialize_bool(attr.value.booldata);

        case SAI_ATTR_VALUE_TYPE_CHARDATA:
            return sai_serialize_chardata(attr.value.chardata);

        case SAI_ATTR_VALUE_TYPE_UINT8:
            return sai_serialize_number(attr.value.u8);

        case SAI_ATTR_VALUE_TYPE_INT8:
            return sai_serialize_number(attr.value.s8);

        case SAI_ATTR_VALUE_TYPE_UINT16:
            return sai_serialize_number(attr.value.u16);

        case SAI_ATTR_VALUE_TYPE_INT16:
            return sai_serialize_number(attr.value.s16);

        case SAI_ATTR_VALUE_TYPE_UINT32:
            return sai_serialize_number(attr.value.u32);

        case SAI_ATTR_VALUE_TYPE_INT32:
            return sai_serialize_enum(attr.value.s32, meta.enummetadata);

        case SAI_ATTR_VALUE_TYPE_UINT64:
            return sai_serialize_number(attr.value.u64);

        case SAI_ATTR_VALUE_TYPE_INT64:
            return sai_serialize_number(attr.value.s64);

        case SAI_ATTR_VALUE_TYPE_MAC:
            return sai_serialize_mac(attr.value.mac);

        case SAI_ATTR_VALUE_TYPE_IPV4:
            return sai_serialize_ipv4(attr.value.ip4);

        case SAI_ATTR_VALUE_TYPE_IPV6:
            return sai_serialize_ipv6(attr.value.ip6);

        case SAI_ATTR_VALUE_TYPE_POINTER:
            return sai_serialize_pointer(attr.value.ptr);

        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS:
            return sai_serialize_ip_address(attr.value.ipaddr);

        case SAI_ATTR_VALUE_TYPE_IP_PREFIX:
            return sai_serialize_ip_prefix(attr.value.ipprefix);

        case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
            return sai_serialize_object_id(attr.value.oid);

        case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            return sai_serialize_oid_list(attr.value.objlist, countOnly);

        case SAI_ATTR_VALUE_TYPE_UINT8_LIST:
            return sai_serialize_number_list(attr.value.u8list, countOnly);

        case SAI_ATTR_VALUE_TYPE_INT8_LIST:
            return sai_serialize_number_list(attr.value.s8list, countOnly);

        case SAI_ATTR_VALUE_TYPE_UINT16_LIST:
            return sai_serialize_number_list(attr.value.u16list, countOnly);

        case SAI_ATTR_VALUE_TYPE_INT16_LIST:
            return sai_serialize_number_list(attr.value.s16list, countOnly);

        case SAI_ATTR_VALUE_TYPE_UINT32_LIST:
            return sai_serialize_number_list(attr.value.u32list, countOnly);

        case SAI_ATTR_VALUE_TYPE_INT32_LIST:
            return sai_serialize_enum_list(attr.value.s32list, meta.enummetadata, countOnly);

        case SAI_ATTR_VALUE_TYPE_UINT32_RANGE:
            return sai_serialize_range(attr.value.u32range);

        case SAI_ATTR_VALUE_TYPE_INT32_RANGE:
            return sai_serialize_range(attr.value.s32range);

        case SAI_ATTR_VALUE_TYPE_VLAN_LIST:
            return sai_serialize_number_list(attr.value.vlanlist, countOnly);

        case SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
            return sai_serialize_qos_map_list(attr.value.qosmap, countOnly);

        case SAI_ATTR_VALUE_TYPE_ACL_RESOURCE_LIST:
            return sai_serialize_acl_resource_list(attr.value.aclresource, countOnly);

        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
            return sai_serialize_ip_address_list(attr.value.ipaddrlist, countOnly);

            // ACL FIELD DATA

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
            return sai_serialize_acl_field(meta, attr.value.aclfield, countOnly);

            // ACL ACTION DATA

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            return sai_serialize_acl_action(meta, attr.value.aclaction, countOnly);

        case SAI_ATTR_VALUE_TYPE_ACL_CAPABILITY:
            return sai_serialize_acl_capability(meta, attr.value.aclcapability, countOnly);

        default:
            SWSS_LOG_THROW("FATAL: invalid serialization type %d", meta.attrvaluetype);
    }
}

std::string sai_serialize_ip_prefix(
        _In_ const sai_ip_prefix_t& prefix)
{
    SWSS_LOG_ENTER();

    switch (prefix.addr_family)
    {
        case SAI_IP_ADDR_FAMILY_IPV4:

            return sai_serialize_ipv4(prefix.addr.ip4) + "/" + sai_serialize_number((get_ipv4_mask(prefix.mask.ip4)));

        case SAI_IP_ADDR_FAMILY_IPV6:

            return sai_serialize_ipv6(prefix.addr.ip6) + "/" + sai_serialize_number(get_ipv6_mask(prefix.mask.ip6));

        default:

            SWSS_LOG_THROW("FATAL: invalid ip prefix address family: %d", prefix.addr_family);
    }
}

std::string sai_serialize_port_oper_status(
        _In_ sai_port_oper_status_t status)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(status, &sai_metadata_enum_sai_port_oper_status_t);
}

std::string sai_serialize_queue_deadlock_event(
        _In_ sai_queue_pfc_deadlock_event_type_t event)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(event, &sai_metadata_enum_sai_queue_pfc_deadlock_event_type_t);
}

std::string sai_serialize_fdb_event(
        _In_ sai_fdb_event_t event)
{
    SWSS_LOG_ENTER();

    return sai_serialize_enum(event, &sai_metadata_enum_sai_fdb_event_t);
}

json sai_serialize_json_fdb_event_notification_data(
        _In_ const sai_fdb_event_notification_data_t& fdb_event)
{
    SWSS_LOG_ENTER();

    json j;

    j["fdb_event"] = sai_serialize_fdb_event(fdb_event.event_type);
    j["fdb_entry"] = sai_serialize_fdb_entry(fdb_event.fdb_entry);

    json arr = json::array();

    for (uint32_t i = 0; i < fdb_event.attr_count; ++i)
    {
        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_FDB_ENTRY, fdb_event.attr[i].id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_ENTRY).c_str(),
                    fdb_event.attr[i].id);
        }

        json item;

        item["id"] = meta->attridname;
        item["value"] = sai_serialize_attr_value(*meta, fdb_event.attr[i]);

        arr.push_back(item);
    }

    j["list"] = arr;

    // we don't need count since it can be deduced
    return j;
}

std::string sai_serialize_fdb_event_ntf(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t* fdb_event)
{
    SWSS_LOG_ENTER();

    if (fdb_event == NULL)
    {
        SWSS_LOG_THROW("fdb_event pointer is null");
    }

    json j = json::array();

    for (uint32_t i = 0; i < count; ++i)
    {
        json item = sai_serialize_json_fdb_event_notification_data(fdb_event[i]);

        j.push_back(item);
    }

    // we don't need count since it can be deduced
    return j.dump();
}

std::string sai_serialize_port_oper_status_ntf(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t* port_oper_status)
{
    SWSS_LOG_ENTER();

    if (port_oper_status == NULL)
    {
        SWSS_LOG_THROW("port_oper_status pointer is null");
    }

    json j = json::array();

    for (uint32_t i = 0; i < count; ++i)
    {
        json item;

        item["port_id"] = sai_serialize_object_id(port_oper_status[i].port_id);
        item["port_state"] = sai_serialize_port_oper_status(port_oper_status[i].port_state);

        j.push_back(item);
    }

    // we don't need count since it can be deduced
    return j.dump();
}

std::string sai_serialize_queue_deadlock_ntf(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t* deadlock_data)
{
    SWSS_LOG_ENTER();

    if (deadlock_data == NULL)
    {
        SWSS_LOG_THROW("deadlock_data pointer is null");
    }

    json j = json::array();

    for (uint32_t i = 0; i < count; ++i)
    {
        json item;

        item["queue_id"] = sai_serialize_object_id(deadlock_data[i].queue_id);
        item["event"] = sai_serialize_queue_deadlock_event(deadlock_data[i].event);

        j.push_back(item);
    }

    // we don't need count since it can be deduced
    return j.dump();
}

std::string sai_serialize_object_meta_key(
        _In_ const sai_object_meta_key_t& meta_key)
{
    SWSS_LOG_ENTER();

    std::string key;

    if (meta_key.objecttype == SAI_OBJECT_TYPE_NULL || meta_key.objecttype >= SAI_OBJECT_TYPE_EXTENSIONS_MAX)
    {
        SWSS_LOG_THROW("invalid object type value %s", sai_serialize_object_type(meta_key.objecttype).c_str());
    }

    const auto& meta = sai_metadata_get_object_type_info(meta_key.objecttype);

    switch (meta_key.objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            key = sai_serialize_fdb_entry(meta_key.objectkey.key.fdb_entry);
            break;

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            key = sai_serialize_route_entry(meta_key.objectkey.key.route_entry);
            break;

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            key = sai_serialize_neighbor_entry(meta_key.objectkey.key.neighbor_entry);
            break;

        default:

            if (meta->isnonobjectid)
            {
                SWSS_LOG_THROW("object %s is non object id, not supported yet, FIXME",
                        sai_serialize_object_type(meta->objecttype).c_str());
            }

            key = sai_serialize_object_id(meta_key.objectkey.key.object_id);
            break;
    }

    key = sai_serialize_object_type(meta_key.objecttype) + ":" + key;

    SWSS_LOG_DEBUG("%s", key.c_str());

    return key;
}

// deserialize

void sai_deserialize_bool(
        _In_ const std::string& s,
        _Out_ bool& b)
{
    SWSS_LOG_ENTER();

    if (s == "true")
    {
        b = true;
        return;
    }

    if (s == "false")
    {
        b = false;
        return;
    }

    SWSS_LOG_THROW("failed to deserialize '%s' as bool", s.c_str());
}

void sai_deserialize_chardata(
        _In_ const std::string& s,
        _Out_ char chardata[CHAR_LEN])
{
    SWSS_LOG_ENTER();

    memset(chardata, 0, CHAR_LEN);

    std::string deserialized;

    size_t len = s.length();

    for (size_t i = 0; i < len; ++i)
    {
        unsigned char c = (unsigned char)s[i];

        if (c == '\\')
        {
            if (i+1 >= len || ((s[i+1] != '\\') && (s[i+1] != 'x')))
            {
                SWSS_LOG_THROW("invalid chardata %s", s.c_str());
            }

            if (s[i+1] == '\\')
            {
                deserialized += "\\";
                i++;
                continue;
            }

            i++;

            if (i + 2 >= len)
            {
                SWSS_LOG_THROW("invalid chardata %s", s.c_str());
            }

            int h = char_to_int(s[i+1]);
            int l = char_to_int(s[i+2]);

            int r = (h << 4) | l;

            c = (unsigned char)r;

            i += 2;
        }

        deserialized += c;
    }

    len = deserialized.length();

    if (len > CHAR_LEN)
    {
        SWSS_LOG_THROW("invalid chardata %s", s.c_str());
    }

    memcpy(chardata, deserialized.data(), len);
}

template<typename T>
void sai_deserialize_number(
        _In_ const std::string& s,
        _Out_ T& number,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    errno = 0;

    char *endptr = NULL;

    number = (T)strtoull(s.c_str(), &endptr, hex ? 16 : 10);

    if (errno != 0 || endptr != s.c_str() + s.length())
    {
        SWSS_LOG_THROW("invalid number %s", s.c_str());
    }
}

void sai_deserialize_number(
        _In_ const std::string& s,
        _Out_ uint32_t& number,
        _In_ bool hex)
{
    SWSS_LOG_ENTER();

    sai_deserialize_number<uint32_t>(s, number, hex);
}

void sai_deserialize_enum(
        _In_ const std::string& s,
        _In_ const sai_enum_metadata_t *meta,
        _Out_ int32_t& value)
{
    SWSS_LOG_ENTER();

    if (meta == NULL)
    {
        return sai_deserialize_number(s, value);
    }

    for (size_t i = 0; i < meta->valuescount; ++i)
    {
        if (strcmp(s.c_str(), meta->valuesnames[i]) == 0)
        {
            value = meta->values[i];
            return;
        }
    }

    SWSS_LOG_WARN("enum %s not found in enum %s", s.c_str(), meta->name);

    sai_deserialize_number(s, value);
}

void sai_deserialize_mac(
        _In_ const std::string& s,
        _Out_ sai_mac_t& mac)
{
    SWSS_LOG_ENTER();

    if (s.length() != (6*2+5))
    {
        SWSS_LOG_THROW("invalid mac address %s", s.c_str());
    }

    int i = 0;

    for (int j = 0; j < 6 ; j++, i += 3)
    {
        int h = char_to_int(s[i+0]);
        int l = char_to_int(s[i+1]);

        int r = (h << 4) | l;

        mac[j] = (unsigned char)r;
    }
}

void sai_deserialize_object_id(
        _In_ const std::string& s,
        _Out_ sai_object_id_t& oid)
{
    SWSS_LOG_ENTER();

    if (s.find("oid:0x") != 0)
    {
        SWSS_LOG_THROW("invalid oid %s", s.c_str());
    }

    errno = 0;

    char *endptr = NULL;

    oid = (sai_object_id_t)strtoull(s.c_str()+4, &endptr, 16);

    if (errno != 0 || endptr != s.c_str() + s.length())
    {
        SWSS_LOG_THROW("invalid oid %s", s.c_str());
    }
}

template<typename T, typename F>
void sai_deserialize_list(
        _In_ const std::string& s,
        _Out_ T& list,
        _In_ bool countOnly,
        F deserialize_item)
{
    SWSS_LOG_ENTER();

    if (countOnly)
    {
        sai_deserialize_number(s, list.count);
        return;
    }

    auto pos = s.find(":");

    if (pos == std::string::npos)
    {
        SWSS_LOG_THROW("invalid list %s", s.c_str());
    }

    std::string scount = s.substr(0, pos);

    sai_deserialize_number(scount, list.count);

    std::string slist = s.substr(pos + 1);

    if (slist == "null")
    {
        list.list = NULL;
        return;
    }

    auto tokens = swss::tokenize(slist, ',');

    if (tokens.size() != list.count)
    {
        SWSS_LOG_THROW("invalid list count %lu != %u", tokens.size(), list.count);
    }

    // list.list = sai_alloc_list(list.count, list);
    list.list = sai_alloc_n_of_ptr_type(list.count, list.list);

    for (uint32_t i = 0; i < list.count; ++i)
    {
        deserialize_item(tokens[i], list.list[i]);
    }
}

void sai_deserialize_oid_list(
        _In_ const std::string& s,
        _Out_ sai_object_list_t& objlist,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    sai_deserialize_list(s, objlist, countOnly, [&](const std::string sitem, sai_object_id_t& item) { sai_deserialize_object_id(sitem, item);} );
}

void sai_deserialize_enum_list(
        _In_ const std::string& s,
        _In_ const sai_enum_metadata_t* meta,
        _Out_ sai_s32_list_t& list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    sai_deserialize_list(s, list, countOnly, [&](const std::string sitem, int32_t& item) { sai_deserialize_enum(sitem, meta, item);} );
}

template <typename T>
void sai_deserialize_number_list(
        _In_ const std::string& s,
        _Out_ T& list,
        _In_ bool countOnly,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    sai_deserialize_list(s, list, countOnly, [&](const std::string sitem, decltype(*list.list)& item) { sai_deserialize_number(sitem, item, hex);} );
}

void sai_deserialize_packet_color(
        _In_ const std::string& s,
        _Out_ sai_packet_color_t& color)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_packet_color_t, (int32_t&)color);
}

void sai_deserialize_qos_map_params(
        _In_ const json& j,
        _Out_ sai_qos_map_params_t& params)
{
    SWSS_LOG_ENTER();

    params.tc             = j["tc"];
    params.dscp           = j["dscp"];
    params.dot1p          = j["dot1p"];
    params.prio           = j["prio"];
    params.pg             = j["pg"];
    params.queue_index    = j["qidx"];

    sai_deserialize_packet_color(j["color"], params.color);
}

void sai_deserialize_qos_map(
        _In_ const json& j,
        _Out_ sai_qos_map_t& qosmap)
{
    SWSS_LOG_ENTER();

    sai_deserialize_qos_map_params(j["key"], qosmap.key);
    sai_deserialize_qos_map_params(j["value"], qosmap.value);
}

void sai_deserialize_qos_map_list(
        _In_ const std::string& s,
        _Out_ sai_qos_map_list_t& qosmap,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    qosmap.count = j["count"];

    if (countOnly)
    {
        return;
    }

    if (j["list"] == nullptr)
    {
        qosmap.list = NULL;
        return;
    }

    json arr = j["list"];

    if (arr.size() != (size_t)qosmap.count)
    {
        SWSS_LOG_THROW("qos map count mismatch %lu vs %u", arr.size(), qosmap.count);
    }

    qosmap.list = sai_alloc_n_of_ptr_type(qosmap.count, qosmap.list);

    for (uint32_t i = 0; i < qosmap.count; ++i)
    {
        const json& item = arr[i];

        sai_deserialize_qos_map(item, qosmap.list[i]);
    }
}

void sai_deserialize_acl_stage(
        _In_ const std::string& s,
        _Out_ sai_acl_stage_t& stage)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_acl_stage_t, (int32_t&)stage);
}

void sai_deserialize_acl_bind_point(
        _In_ const std::string& s,
        _Out_ sai_acl_bind_point_type_t& bind_point)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_acl_bind_point_type_t, (int32_t&)bind_point);
}

void sai_deserialize_acl_resource(
        _In_ const json& j,
        _Out_ sai_acl_resource_t& aclresource)
{
    SWSS_LOG_ENTER();

    sai_deserialize_acl_stage(j["stage"], aclresource.stage);
    sai_deserialize_acl_bind_point(j["bind_point"], aclresource.bind_point);
    sai_deserialize_number(j["avail_num"], aclresource.avail_num);
}

void sai_deserialize_acl_resource_list(
        _In_ const std::string& s,
        _Out_ sai_acl_resource_list_t& aclresource,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    aclresource.count = j["count"];

    if (countOnly)
    {
        return;
    }

    if (j["list"] == nullptr)
    {
        aclresource.list = NULL;
        return;
    }

    json arr = j["list"];

    if (arr.size() != (size_t)aclresource.count)
    {
        SWSS_LOG_ERROR("acl resource count mismatch %lu vs %u", arr.size(), aclresource.count);
        throw std::runtime_error("acl resource count mismatch");
    }

    aclresource.list = sai_alloc_n_of_ptr_type(aclresource.count, aclresource.list);

    for (uint32_t i = 0; i < aclresource.count; ++i)
    {
        const json& item = arr[i];

        sai_deserialize_acl_resource(item, aclresource.list[i]);
    }
}

void sai_deserialize_ipv6(
        _In_ const std::string& s,
        _Out_ sai_ip6_t& ipaddr)
{
    SWSS_LOG_ENTER();

    if (inet_pton(AF_INET6, s.c_str(), ipaddr) != 1)
    {
        SWSS_LOG_THROW("invalid ip address %s", s.c_str());
    }
}

void sai_deserialize_ipv4(
        _In_ const std::string& s,
        _Out_ sai_ip4_t& ipaddr)
{
    SWSS_LOG_ENTER();

    if (inet_pton(AF_INET, s.c_str(), &ipaddr) != 1)
    {
        SWSS_LOG_THROW("invalid ip address %s", s.c_str());
    }
}

void sai_deserialize_pointer(
        _In_ const std::string& s,
        _Out_ sai_pointer_t& ptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_number(s, (uint64_t&)ptr, true);
}

void sai_deserialize_ip_address(
        _In_ const std::string& s,
        _Out_ sai_ip_address_t& ipaddr)
{
    SWSS_LOG_ENTER();

    if (inet_pton(AF_INET, s.c_str(), &ipaddr.addr.ip4) == 1)
    {
        ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        return;
    }

    if (inet_pton(AF_INET6, s.c_str(), ipaddr.addr.ip6) == 1)
    {
        ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
        return;
    }

    SWSS_LOG_THROW("invalid ip address %s", s.c_str());
}

void sai_deserialize_ip_prefix(
        _In_ const std::string &s,
        _Out_ sai_ip_prefix_t &ip_prefix)
{
    SWSS_LOG_ENTER();

    auto tokens = swss::tokenize(s, '/');

    if (tokens.size() != 2)
    {
        SWSS_LOG_THROW("invalid ip prefix %s", s.c_str());
    }

    uint8_t mask;
    sai_deserialize_number(tokens[1], mask);

    const std::string &ip = tokens[0];

    if (inet_pton(AF_INET, ip.c_str(), &ip_prefix.addr.ip4) == 1)
    {
        ip_prefix.addr_family = SAI_IP_ADDR_FAMILY_IPV4;

        sai_populate_ip_mask(mask, (uint8_t*)&ip_prefix.mask.ip4, false);
    }
    else if (inet_pton(AF_INET6, ip.c_str(), ip_prefix.addr.ip6) == 1)
    {
        ip_prefix.addr_family = SAI_IP_ADDR_FAMILY_IPV6;

        sai_populate_ip_mask(mask, ip_prefix.mask.ip6, true);
    }
    else
    {
        SWSS_LOG_THROW("invalid ip prefix %s", s.c_str());
    }
}

void sai_deserialize_ip_address_list(
        _In_ const std::string& s,
        _Out_ sai_ip_address_list_t& list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    sai_deserialize_list(s, list, countOnly, [&](const std::string sitem, sai_ip_address_t& item) { sai_deserialize_ip_address(sitem, item);} );
}

template <typename T>
void sai_deserialize_range(
        _In_ const std::string& s,
        _Out_ T& range)
{
    SWSS_LOG_ENTER();

    auto tokens = swss::tokenize(s, ',');

    if (tokens.size() != 2)
    {
        SWSS_LOG_THROW("invalid range %s", s.c_str());
    }

    sai_deserialize_number(tokens[0], range.min);
    sai_deserialize_number(tokens[1], range.max);
}

void sai_deserialize_acl_field(
        _In_ const std::string& s,
        _In_ const sai_attr_metadata_t& meta,
        _In_ sai_acl_field_data_t& field,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    if (s == "disabled")
    {
        field.enable = false;
        return;
    }

    field.enable = true;

    auto pos = s.find("&mask:");

    std::string sfield;
    std::string smask;

    if (pos == std::string::npos)
    {
        sfield = s;
    }
    else
    {
        sfield = s.substr(0, pos);
        smask = s.substr(pos + 6); // 6 = "&mask:" length
    }

    switch (meta.attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
            return sai_deserialize_bool(sfield, field.data.booldata);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
            sai_deserialize_number(sfield, field.data.u8);
            sai_deserialize_number(smask, field.mask.u8, true);
            return;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
            sai_deserialize_number(sfield, field.data.s8);
            sai_deserialize_number(smask, field.mask.s8, true);
            return;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
            sai_deserialize_number(sfield, field.data.u16);
            sai_deserialize_number(smask, field.mask.u16, true);
            return;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
            sai_deserialize_number(sfield, field.data.s16);
            sai_deserialize_number(smask, field.mask.s16, true);
            return;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
            sai_deserialize_number(sfield, field.data.u32);
            sai_deserialize_number(smask, field.mask.u32, true);
            return;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
            sai_deserialize_enum(sfield, meta.enummetadata, field.data.s32);
            sai_deserialize_number(smask, field.mask.s32, true);
            return;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
            sai_deserialize_mac(sfield, field.data.mac);
            sai_deserialize_mac(smask, field.mask.mac);
            return;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
            sai_deserialize_ipv4(sfield, field.data.ip4);
            sai_deserialize_ipv4(smask, field.mask.ip4);
            return;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
            sai_deserialize_ipv6(sfield, field.data.ip6);
            sai_deserialize_ipv6(smask, field.mask.ip6);
            return;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            return sai_deserialize_object_id(sfield, field.data.oid);

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            return sai_deserialize_oid_list(sfield, field.data.objlist, countOnly);
            /*
               case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
               return sai_deserialize_number_list(field.data.u8list, countOnly) + "&mask:" + sai_deserialize_uint8_hex_list(field.mask.u8list, countOnly);
               */
        default:
            SWSS_LOG_THROW("FATAIL: invalid serialization type %d", meta.attrvaluetype);
    }
}

void sai_deserialize_acl_action(
        _In_ const std::string& s,
        _In_ const sai_attr_metadata_t& meta,
        _In_ sai_acl_action_data_t& action,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    if (s == "disabled")
    {
        action.enable = false;
        return;
    }

    action.enable = true;

    switch (meta.attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
            return sai_deserialize_bool(s, action.parameter.booldata);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
            return sai_deserialize_number(s, action.parameter.u8);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
            return sai_deserialize_number(s, action.parameter.s8);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
            return sai_deserialize_number(s, action.parameter.u16);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
            return sai_deserialize_number(s, action.parameter.s16);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
            return sai_deserialize_number(s, action.parameter.u32);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
            return sai_deserialize_enum(s, meta.enummetadata, action.parameter.s32);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
            return sai_deserialize_mac(s, action.parameter.mac);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
            return sai_deserialize_ipv4(s, action.parameter.ip4);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
            return sai_deserialize_ipv6(s, action.parameter.ip6);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            return sai_deserialize_object_id(s, action.parameter.oid);

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            return sai_deserialize_oid_list(s, action.parameter.objlist, countOnly);

        default:
            SWSS_LOG_THROW("FATAIL: invalid serialization type %d", meta.attrvaluetype);
    }
}

void sai_deserialize_attr_value(
        _In_ const std::string& s,
        _In_ const sai_attr_metadata_t& meta,
        _Out_ sai_attribute_t &attr,
        _In_ const bool countOnly)
{
    SWSS_LOG_ENTER();

    memset(&attr.value, 0, sizeof(attr.value));

    switch (meta.attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_BOOL:
            return sai_deserialize_bool(s, attr.value.booldata);

        case SAI_ATTR_VALUE_TYPE_CHARDATA:
            return sai_deserialize_chardata(s, attr.value.chardata);

        case SAI_ATTR_VALUE_TYPE_UINT8:
            return sai_deserialize_number(s, attr.value.u8);

        case SAI_ATTR_VALUE_TYPE_INT8:
            return sai_deserialize_number(s, attr.value.s8);

        case SAI_ATTR_VALUE_TYPE_UINT16:
            return sai_deserialize_number(s, attr.value.u16);

        case SAI_ATTR_VALUE_TYPE_INT16:
            return sai_deserialize_number(s, attr.value.s16);

        case SAI_ATTR_VALUE_TYPE_UINT32:
            return sai_deserialize_number(s, attr.value.u32);

        case SAI_ATTR_VALUE_TYPE_INT32:
            return sai_deserialize_enum(s, meta.enummetadata, attr.value.s32);

        case SAI_ATTR_VALUE_TYPE_UINT64:
            return sai_deserialize_number(s, attr.value.u64);

        case SAI_ATTR_VALUE_TYPE_INT64:
            return sai_deserialize_number(s, attr.value.s64);

        case SAI_ATTR_VALUE_TYPE_MAC:
            return sai_deserialize_mac(s, attr.value.mac);

        case SAI_ATTR_VALUE_TYPE_IPV4:
            return sai_deserialize_ipv4(s, attr.value.ip4);

        case SAI_ATTR_VALUE_TYPE_IPV6:
            return sai_deserialize_ipv6(s, attr.value.ip6);

        case SAI_ATTR_VALUE_TYPE_POINTER:
            return sai_deserialize_pointer(s, attr.value.ptr);

        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS:
            return sai_deserialize_ip_address(s, attr.value.ipaddr);

        case SAI_ATTR_VALUE_TYPE_IP_PREFIX:
            return sai_deserialize_ip_prefix(s, attr.value.ipprefix);

        case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
            return sai_deserialize_object_id(s, attr.value.oid);

        case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            return sai_deserialize_oid_list(s, attr.value.objlist, countOnly);

        case SAI_ATTR_VALUE_TYPE_UINT8_LIST:
            return sai_deserialize_number_list(s, attr.value.u8list, countOnly);

        case SAI_ATTR_VALUE_TYPE_INT8_LIST:
            return sai_deserialize_number_list(s, attr.value.s8list, countOnly);

        case SAI_ATTR_VALUE_TYPE_UINT16_LIST:
            return sai_deserialize_number_list(s, attr.value.u16list, countOnly);

        case SAI_ATTR_VALUE_TYPE_INT16_LIST:
            return sai_deserialize_number_list(s, attr.value.s16list, countOnly);

        case SAI_ATTR_VALUE_TYPE_UINT32_LIST:
            return sai_deserialize_number_list(s, attr.value.u32list, countOnly);

        case SAI_ATTR_VALUE_TYPE_INT32_LIST:
            return sai_deserialize_enum_list(s, meta.enummetadata, attr.value.s32list, countOnly);

        case SAI_ATTR_VALUE_TYPE_UINT32_RANGE:
            return sai_deserialize_range(s, attr.value.u32range);

        case SAI_ATTR_VALUE_TYPE_INT32_RANGE:
            return sai_deserialize_range(s, attr.value.s32range);

        case SAI_ATTR_VALUE_TYPE_VLAN_LIST:
            return sai_deserialize_number_list(s, attr.value.vlanlist, countOnly);

        case SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
            return sai_deserialize_qos_map_list(s, attr.value.qosmap, countOnly);

        case SAI_ATTR_VALUE_TYPE_ACL_RESOURCE_LIST:
            return sai_deserialize_acl_resource_list(s, attr.value.aclresource, countOnly);

        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
            return sai_deserialize_ip_address_list(s, attr.value.ipaddrlist, countOnly);

            // ACL FIELD DATA

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
            return sai_deserialize_acl_field(s, meta, attr.value.aclfield, countOnly);

            // ACL ACTION DATA

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            return sai_deserialize_acl_action(s, meta, attr.value.aclaction, countOnly);

        default:
            SWSS_LOG_THROW("deserialize type %d is not supportd yet FIXME", meta.attrvaluetype);
    }
}

void sai_deserialize_status(
        _In_ const std::string& s,
        _Out_ sai_status_t& status)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_status_t, status);
}

void sai_deserialize_port_oper_status(
        _In_ const std::string& s,
        _Out_ sai_port_oper_status_t& status)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_port_oper_status_t, (int32_t&)status);
}

void sai_deserialize_queue_deadlock(
        _In_ const std::string& s,
        _Out_ sai_queue_pfc_deadlock_event_type_t& event)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_queue_pfc_deadlock_event_type_t, (int32_t&)event);
}

void sai_deserialize_fdb_event(
        _In_ const std::string& s,
        _Out_ sai_fdb_event_t& event)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_fdb_event_t, (int32_t&)event);
}

void sai_deserialize_switch_oper_status(
        _In_ const std::string& s,
        _Out_ sai_object_id_t &switch_id,
        _Out_ sai_switch_oper_status_t& status)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    sai_deserialize_object_id(j["switch_id"], switch_id);
    sai_deserialize_enum(j["status"], &sai_metadata_enum_sai_switch_oper_status_t, (int32_t&)status);
}

void sai_deserialize_switch_shutdown_request(
        _In_ const std::string& s,
        _Out_ sai_object_id_t &switch_id)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    sai_deserialize_object_id(j["switch_id"], switch_id);
}

void sai_deserialize_object_type(
        _In_ const std::string& s,
        _Out_ sai_object_type_t& object_type)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_object_type_t, (int32_t&)object_type);
}

void sai_deserialize_vlan_id(
        _In_ const std::string& s,
        _In_ sai_vlan_id_t& vlan_id)
{
    SWSS_LOG_ENTER();

    sai_deserialize_number(s, vlan_id);
}

void sai_deserialize_fdb_entry(
        _In_ const std::string &s,
        _Out_ sai_fdb_entry_t &fdb_entry)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    sai_deserialize_object_id(j["switch_id"], fdb_entry.switch_id);
    sai_deserialize_mac(j["mac"], fdb_entry.mac_address);
    sai_deserialize_object_id(j["bvid"], fdb_entry.bv_id);
}

void sai_deserialize_neighbor_entry(
        _In_ const std::string& s,
        _In_ sai_neighbor_entry_t &ne)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    sai_deserialize_object_id(j["switch_id"], ne.switch_id);
    sai_deserialize_object_id(j["rif"], ne.rif_id);
    sai_deserialize_ip_address(j["ip"], ne.ip_address);
}

void sai_deserialize_route_entry(
        _In_ const std::string &s,
        _Out_ sai_route_entry_t& route_entry)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    sai_deserialize_object_id(j["switch_id"], route_entry.switch_id);
    sai_deserialize_object_id(j["vr"], route_entry.vr_id);
    sai_deserialize_ip_prefix(j["dest"], route_entry.destination);
}

void sai_deserialize_attr_id(
        _In_ const std::string& s,
        _Out_ const sai_attr_metadata_t** meta)
{
    SWSS_LOG_ENTER();

    if (meta == NULL)
    {
        SWSS_LOG_THROW("meta pointer is null");
    }

    auto m = sai_metadata_get_attr_metadata_by_attr_id_name(s.c_str());

    if (m == NULL)
    {
        SWSS_LOG_THROW("invalid attr id: %s", s.c_str());
    }

    *meta = m;
}

void sai_deserialize_attr_id(
        _In_ const std::string& s,
        _Out_ sai_attr_id_t& attrid)
{
    SWSS_LOG_ENTER();

    const sai_attr_metadata_t *meta = NULL;

    sai_deserialize_attr_id(s, &meta);

    attrid = meta->attrid;
}

void sai_deserialize_object_meta_key(
        _In_ const std::string &s,
        _Out_ sai_object_meta_key_t& meta_key)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("%s", s.c_str());

    const std::string &str_object_type = s.substr(0, s.find(":"));
    const std::string &str_object_id = s.substr(s.find(":") + 1);

    sai_deserialize_object_type(str_object_type, meta_key.objecttype);

    if (meta_key.objecttype == SAI_OBJECT_TYPE_NULL || meta_key.objecttype >= SAI_OBJECT_TYPE_EXTENSIONS_MAX)
    {
        SWSS_LOG_THROW("invalid object type value %s", sai_serialize_object_type(meta_key.objecttype).c_str());
    }

    const auto& meta = sai_metadata_get_object_type_info(meta_key.objecttype);

    switch (meta_key.objecttype)
    {
        case SAI_OBJECT_TYPE_FDB_ENTRY:
            sai_deserialize_fdb_entry(str_object_id, meta_key.objectkey.key.fdb_entry);
            break;

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            sai_deserialize_route_entry(str_object_id, meta_key.objectkey.key.route_entry);
            break;

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            sai_deserialize_neighbor_entry(str_object_id, meta_key.objectkey.key.neighbor_entry);
            break;

        default:

            if (meta->isnonobjectid)
            {
                SWSS_LOG_THROW("object %s is non object id, not supported yet, FIXME",
                        sai_serialize_object_type(meta->objecttype).c_str());
            }

            sai_deserialize_object_id(str_object_id, meta_key.objectkey.key.object_id);
            break;
    }
}


// deserialize notifications

void sai_deserialize_json_fdb_event_notification_data(
        _In_ const json& j,
        _Out_ sai_fdb_event_notification_data_t& fdb)
{
    SWSS_LOG_ENTER();

    sai_deserialize_fdb_event(j["fdb_event"], fdb.event_type);
    sai_deserialize_fdb_entry(j["fdb_entry"], fdb.fdb_entry);

    json arr = j["list"];

    fdb.attr_count = (uint32_t)arr.size();
    fdb.attr = new sai_attribute_t[fdb.attr_count];

    for (uint32_t i = 0; i < fdb.attr_count; ++i)
    {
        const json &item = arr[i];

        const sai_attr_metadata_t *meta = NULL;

        sai_deserialize_attr_id(item["id"], &meta);

        fdb.attr[i].id = meta->attrid;

        sai_deserialize_attr_value(item["value"], *meta, fdb.attr[i]);
    }
}

void sai_deserialize_fdb_event_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_fdb_event_notification_data_t** fdb_event)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    count = (uint32_t)j.size();

    auto data = new sai_fdb_event_notification_data_t[count];

    for (uint32_t i = 0; i < count; ++i)
    {
        sai_deserialize_json_fdb_event_notification_data(j[i], data[i]);
    }

    *fdb_event = data;
}

void sai_deserialize_port_oper_status_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_port_oper_status_notification_t** port_oper_status)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    count = (uint32_t)j.size();

    auto data = new sai_port_oper_status_notification_t[count];

    for (uint32_t i = 0; i < count; ++i)
    {
        sai_deserialize_object_id(j[i]["port_id"], data[i].port_id);
        sai_deserialize_port_oper_status(j[i]["port_state"], data[i].port_state);
    }

    *port_oper_status = data;
}

void sai_deserialize_queue_deadlock_ntf(
        _In_ const std::string& s,
        _Out_ uint32_t &count,
        _Out_ sai_queue_deadlock_notification_data_t** deadlock_data)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    count = (uint32_t)j.size();

    auto data = new sai_queue_deadlock_notification_data_t[count];

    for (uint32_t i = 0; i < count; ++i)
    {
        sai_deserialize_object_id(j[i]["queue_id"], data[i].queue_id);
        sai_deserialize_queue_deadlock(j[i]["event"], data[i].event);
    }

    *deadlock_data = data;
}

// deserialize free

void sai_deserialize_free_attribute_value(
        _In_ const sai_attr_value_type_t type,
        _In_ sai_attribute_t &attr)
{
    SWSS_LOG_ENTER();

    // if we allocated list, then we need to free it

    switch (type)
    {
        case SAI_ATTR_VALUE_TYPE_BOOL:
        case SAI_ATTR_VALUE_TYPE_CHARDATA:
        case SAI_ATTR_VALUE_TYPE_UINT8:
        case SAI_ATTR_VALUE_TYPE_INT8:
        case SAI_ATTR_VALUE_TYPE_UINT16:
        case SAI_ATTR_VALUE_TYPE_INT16:
        case SAI_ATTR_VALUE_TYPE_UINT32:
        case SAI_ATTR_VALUE_TYPE_INT32:
        case SAI_ATTR_VALUE_TYPE_UINT64:
        case SAI_ATTR_VALUE_TYPE_INT64:
        case SAI_ATTR_VALUE_TYPE_MAC:
        case SAI_ATTR_VALUE_TYPE_IPV4:
        case SAI_ATTR_VALUE_TYPE_IPV6:
        case SAI_ATTR_VALUE_TYPE_POINTER:
        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS:
        case SAI_ATTR_VALUE_TYPE_IP_PREFIX:
        case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
            break;

        case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            sai_free_list(attr.value.objlist);
            break;

        case SAI_ATTR_VALUE_TYPE_UINT8_LIST:
            sai_free_list(attr.value.u8list);
            break;

        case SAI_ATTR_VALUE_TYPE_INT8_LIST:
            sai_free_list(attr.value.s8list);
            break;

        case SAI_ATTR_VALUE_TYPE_UINT16_LIST:
            sai_free_list(attr.value.u16list);
            break;

        case SAI_ATTR_VALUE_TYPE_INT16_LIST:
            sai_free_list(attr.value.s16list);
            break;

        case SAI_ATTR_VALUE_TYPE_UINT32_LIST:
            sai_free_list(attr.value.u32list);
            break;

        case SAI_ATTR_VALUE_TYPE_INT32_LIST:
            sai_free_list(attr.value.s32list);
            break;

        case SAI_ATTR_VALUE_TYPE_UINT32_RANGE:
        case SAI_ATTR_VALUE_TYPE_INT32_RANGE:
            break;

        case SAI_ATTR_VALUE_TYPE_VLAN_LIST:
            sai_free_list(attr.value.vlanlist);
            break;

        case SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
            sai_free_list(attr.value.qosmap);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_RESOURCE_LIST:
            sai_free_list(attr.value.aclresource);
            break;

        case SAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
            sai_free_list(attr.value.ipaddrlist);
            break;

            /* ACL FIELD DATA */

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            break;
        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            sai_free_list(attr.value.aclfield.data.objlist);
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
            sai_free_list(attr.value.aclfield.mask.u8list);
            sai_free_list(attr.value.aclfield.data.u8list);
            break;

            /* ACL ACTION DATA */

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            sai_free_list(attr.value.aclaction.parameter.objlist);
            break;

        default:
            SWSS_LOG_THROW("unsupported type %d on deserialize free, FIXME", type);
    }
}

// deserialize free notifications

void sai_deserialize_free_fdb_event(
        _In_ sai_fdb_event_notification_data_t& fdb_event)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < fdb_event.attr_count; ++i)
    {
        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_FDB_ENTRY, fdb_event.attr[i].id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_ENTRY).c_str(),
                    fdb_event.attr[i].id);
        }

        sai_deserialize_free_attribute_value(meta->attrvaluetype, fdb_event.attr[i]);
    }

    delete[] fdb_event.attr;
}

void sai_deserialize_free_fdb_event_ntf(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t* fdb_event)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < count; ++i)
    {
        sai_deserialize_free_fdb_event(fdb_event[i]);
    }

    delete[] fdb_event;
}

void sai_deserialize_free_port_oper_status_ntf(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t* port_oper_status)
{
    SWSS_LOG_ENTER();

    delete[] port_oper_status;
}

void sai_deserialize_free_queue_deadlock_ntf(
        _In_ uint32_t count,
        _In_ sai_queue_deadlock_notification_data_t* queue_deadlock)
{
    SWSS_LOG_ENTER();

    delete[] queue_deadlock;
}

void sai_deserialize_ingress_priority_group_attr(
        _In_ const std::string& s,
        _Out_ sai_ingress_priority_group_attr_t& attr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_ingress_priority_group_attr_t, (int32_t&)attr);
}

void sai_deserialize_queue_attr(
        _In_ const std::string& s,
        _Out_ sai_queue_attr_t& attr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_enum(s, &sai_metadata_enum_sai_queue_attr_t, (int32_t&)attr);
}
