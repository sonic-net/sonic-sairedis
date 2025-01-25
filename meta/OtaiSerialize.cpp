#include "otai_serialize.h"
#include "otairediscommon.h"

#include "swss/tokenize.h"

#include <nlohmann/json.hpp>

#include <inttypes.h>
#include <vector>
#include <climits>
#include <unordered_map>

#include <arpa/inet.h>
#include <errno.h>

using json = nlohmann::json;

static const std::unordered_map<otai_redis_link_event_damping_algorithm_t, std::string> otai_redis_link_event_damping_algorithm_to_name_map = {
        {OTAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_DISABLED, "OTAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_DISABLED"},
        {OTAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED, "OTAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED"}};

static const std::unordered_map<otai_redis_port_attr_t, std::string> otai_redis_port_attr_to_name_map = {
        {OTAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM, "OTAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM"},
        {OTAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG, "OTAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG"}};

static int char_to_int(
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
T* otai_alloc_n_of_ptr_type(U count, T*)
{
    SWSS_LOG_ENTER();

    return new T[count];
}

template<typename T, typename U>
void otai_alloc_list(
        _In_ T count,
        _In_ U &element)
{
    SWSS_LOG_ENTER();

    element.count = count;
    element.list = otai_alloc_n_of_ptr_type(count, element.list);
}

template<typename T>
void otai_free_list(
        _In_ T &element)
{
    SWSS_LOG_ENTER();

    delete[] element.list;
    element.list = NULL;
}

template<typename T>
static void transfer_primitive(
        _In_ const T &src_element,
        _In_ T &dst_element)
{
    SWSS_LOG_ENTER();

    const unsigned char* src_mem = reinterpret_cast<const unsigned char*>(&src_element);
    unsigned char* dst_mem = reinterpret_cast<unsigned char*>(&dst_element);

    memcpy(dst_mem, src_mem, sizeof(T));
}

template<typename T>
static otai_status_t transfer_list(
        _In_ const T &src_element,
        _In_ T &dst_element,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    if (countOnly || dst_element.count == 0)
    {
        transfer_primitive(src_element.count, dst_element.count);
        return OTAI_STATUS_SUCCESS;
    }

    if (dst_element.list == NULL)
    {
        SWSS_LOG_ERROR("destination list is null, unable to transfer elements");

        return OTAI_STATUS_FAILURE;
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

        return OTAI_STATUS_SUCCESS;
    }

    // input buffer is too small to get all list elements, so return count only
    transfer_primitive(src_element.count, dst_element.count);

    return OTAI_STATUS_BUFFER_OVERFLOW;
}

#define RETURN_ON_ERROR(x)\
{\
    otai_status_t s = (x);\
    if (s != OTAI_STATUS_SUCCESS)\
        return s;\
}

otai_status_t transfer_attribute(
        _In_ otai_attr_value_type_t serialization_type,
        _In_ const otai_attribute_t &src_attr,
        _In_ otai_attribute_t &dst_attr,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    switch (serialization_type)
    {
        case OTAI_ATTR_VALUE_TYPE_BOOL:
            transfer_primitive(src_attr.value.booldata, dst_attr.value.booldata);
            break;

        case OTAI_ATTR_VALUE_TYPE_CHARDATA:
            transfer_primitive(src_attr.value.chardata, dst_attr.value.chardata);
            break;

        case OTAI_ATTR_VALUE_TYPE_UINT8:
            transfer_primitive(src_attr.value.u8, dst_attr.value.u8);
            break;

        case OTAI_ATTR_VALUE_TYPE_INT8:
            transfer_primitive(src_attr.value.s8, dst_attr.value.s8);
            break;

        case OTAI_ATTR_VALUE_TYPE_UINT16:
            transfer_primitive(src_attr.value.u16, dst_attr.value.u16);
            break;

        case OTAI_ATTR_VALUE_TYPE_INT16:
            transfer_primitive(src_attr.value.s16, dst_attr.value.s16);
            break;

        case OTAI_ATTR_VALUE_TYPE_UINT32:
            transfer_primitive(src_attr.value.u32, dst_attr.value.u32);
            break;

        case OTAI_ATTR_VALUE_TYPE_INT32:
            transfer_primitive(src_attr.value.s32, dst_attr.value.s32);
            break;

        case OTAI_ATTR_VALUE_TYPE_UINT64:
            transfer_primitive(src_attr.value.u64, dst_attr.value.u64);
            break;

//        case OTAI_ATTR_VALUE_TYPE_INT64:
//            transfer_primitive(src_attr.value.s64, dst_attr.value.s64);
//            break;

        //case OTAI_ATTR_VALUE_TYPE_MAC:
        //    transfer_primitive(src_attr.value.mac, dst_attr.value.mac);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_IPV4:
        //    transfer_primitive(src_attr.value.ip4, dst_attr.value.ip4);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_IPV6:
        //    transfer_primitive(src_attr.value.ip6, dst_attr.value.ip6);
        //    break;

        case OTAI_ATTR_VALUE_TYPE_POINTER:
            transfer_primitive(src_attr.value.ptr, dst_attr.value.ptr);
            break;

        //case OTAI_ATTR_VALUE_TYPE_IP_ADDRESS:
        //    transfer_primitive(src_attr.value.ipaddr, dst_attr.value.ipaddr);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_IP_PREFIX:
        //    transfer_primitive(src_attr.value.ipprefix, dst_attr.value.ipprefix);
        //    break;

        case OTAI_ATTR_VALUE_TYPE_OBJECT_ID:
            transfer_primitive(src_attr.value.oid, dst_attr.value.oid);
            break;

        case OTAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.objlist, dst_attr.value.objlist, countOnly));
            break;

        case OTAI_ATTR_VALUE_TYPE_UINT8_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.u8list, dst_attr.value.u8list, countOnly));
            break;

        case OTAI_ATTR_VALUE_TYPE_INT8_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.s8list, dst_attr.value.s8list, countOnly));
            break;

//        case OTAI_ATTR_VALUE_TYPE_UINT16_LIST:
//            RETURN_ON_ERROR(transfer_list(src_attr.value.u16list, dst_attr.value.u16list, countOnly));
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_INT16_LIST:
//            RETURN_ON_ERROR(transfer_list(src_attr.value.s16list, dst_attr.value.s16list, countOnly));
//            break;

        case OTAI_ATTR_VALUE_TYPE_UINT32_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.u32list, dst_attr.value.u32list, countOnly));
            break;

        case OTAI_ATTR_VALUE_TYPE_INT32_LIST:
            RETURN_ON_ERROR(transfer_list(src_attr.value.s32list, dst_attr.value.s32list, countOnly));
            break;

        case OTAI_ATTR_VALUE_TYPE_UINT32_RANGE:
            transfer_primitive(src_attr.value.u32range, dst_attr.value.u32range);
            break;

//        case OTAI_ATTR_VALUE_TYPE_INT32_RANGE:
//            transfer_primitive(src_attr.value.s32range, dst_attr.value.s32range);
//            break;

        //case OTAI_ATTR_VALUE_TYPE_UINT16_RANGE_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.u16rangelist, dst_attr.value.u16rangelist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_TIMESPEC:
        //    transfer_primitive(src_attr.value.timespec, dst_attr.value.timespec);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_JSON:
        //    transfer_primitive(src_attr.value.json, dst_attr.value.json);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_PORT_LANE_LATCH_STATUS_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.portlanelatchstatuslist, dst_attr.value.portlanelatchstatuslist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_LATCH_STATUS:
        //    transfer_primitive(src_attr.value.latchstatus, dst_attr.value.latchstatus);
            break;

        //case OTAI_ATTR_VALUE_TYPE_VLAN_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.vlanlist, dst_attr.value.vlanlist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.qosmap, dst_attr.value.qosmap, countOnly));
        //    break;

//        case OTAI_ATTR_VALUE_TYPE_ACL_RESOURCE_LIST:
//            RETURN_ON_ERROR(transfer_list(src_attr.value.aclresource, dst_attr.value.aclresource, countOnly));
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
//            RETURN_ON_ERROR(transfer_list(src_attr.value.ipaddrlist, dst_attr.value.ipaddrlist, countOnly));
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_SEGMENT_LIST:
//            RETURN_ON_ERROR(transfer_list(src_attr.value.segmentlist, dst_attr.value.segmentlist, countOnly));
//            break;
//
//            /* ACL FIELD DATA */
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.data.booldata, dst_attr.value.aclfield.data.booldata);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.mask.u8, dst_attr.value.aclfield.mask.u8);
//            transfer_primitive(src_attr.value.aclfield.data.u8, dst_attr.value.aclfield.data.u8);
//            break;
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
////            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
////            transfer_primitive(src_attr.value.aclfield.mask.s8, dst_attr.value.aclfield.mask.s8);
////            transfer_primitive(src_attr.value.aclfield.data.s8, dst_attr.value.aclfield.data.s8);
////            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.mask.u16, dst_attr.value.aclfield.mask.u16);
//            transfer_primitive(src_attr.value.aclfield.data.u16, dst_attr.value.aclfield.data.u16);
//            break;
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
////            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
////            transfer_primitive(src_attr.value.aclfield.mask.s16, dst_attr.value.aclfield.mask.s16);
////            transfer_primitive(src_attr.value.aclfield.data.s16, dst_attr.value.aclfield.data.s16);
////            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.mask.u32, dst_attr.value.aclfield.mask.u32);
//            transfer_primitive(src_attr.value.aclfield.data.u32, dst_attr.value.aclfield.data.u32);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.mask.s32, dst_attr.value.aclfield.mask.s32);
//            transfer_primitive(src_attr.value.aclfield.data.s32, dst_attr.value.aclfield.data.s32);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT64:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.mask.u64, dst_attr.value.aclfield.mask.u64);
//            transfer_primitive(src_attr.value.aclfield.data.u64, dst_attr.value.aclfield.data.u64);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.mask.mac, dst_attr.value.aclfield.mask.mac);
//            transfer_primitive(src_attr.value.aclfield.data.mac, dst_attr.value.aclfield.data.mac);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.mask.ip4, dst_attr.value.aclfield.mask.ip4);
//            transfer_primitive(src_attr.value.aclfield.data.ip4, dst_attr.value.aclfield.data.ip4);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.mask.ip6, dst_attr.value.aclfield.mask.ip6);
//            transfer_primitive(src_attr.value.aclfield.data.ip6, dst_attr.value.aclfield.data.ip6);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            transfer_primitive(src_attr.value.aclfield.data.oid, dst_attr.value.aclfield.data.oid);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            RETURN_ON_ERROR(transfer_list(src_attr.value.aclfield.data.objlist, dst_attr.value.aclfield.data.objlist, countOnly));
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
//            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
//            RETURN_ON_ERROR(transfer_list(src_attr.value.aclfield.mask.u8list, dst_attr.value.aclfield.mask.u8list, countOnly));
//            transfer_list(src_attr.value.aclfield.data.u8list, dst_attr.value.aclfield.data.u8list, countOnly);
//            break;
//
//            /* ACL ACTION DATA */
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.booldata, dst_attr.value.aclaction.parameter.booldata);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.u8, dst_attr.value.aclaction.parameter.u8);
//            break;
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
////            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
////            transfer_primitive(src_attr.value.aclaction.parameter.s8, dst_attr.value.aclaction.parameter.s8);
////            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.u16, dst_attr.value.aclaction.parameter.u16);
//            break;
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
////            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
////            transfer_primitive(src_attr.value.aclaction.parameter.s16, dst_attr.value.aclaction.parameter.s16);
////            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.u32, dst_attr.value.aclaction.parameter.u32);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.s32, dst_attr.value.aclaction.parameter.s32);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.mac, dst_attr.value.aclaction.parameter.mac);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.ip4, dst_attr.value.aclaction.parameter.ip4);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.ip6, dst_attr.value.aclaction.parameter.ip6);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IP_ADDRESS:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.ipaddr, dst_attr.value.aclaction.parameter.ipaddr);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            transfer_primitive(src_attr.value.aclaction.parameter.oid, dst_attr.value.aclaction.parameter.oid);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
//            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
//            RETURN_ON_ERROR(transfer_list(src_attr.value.aclaction.parameter.objlist, dst_attr.value.aclaction.parameter.objlist, countOnly));
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_CAPABILITY:
//            transfer_primitive(src_attr.value.aclcapability.is_action_list_mandatory, dst_attr.value.aclcapability.is_action_list_mandatory);
//            RETURN_ON_ERROR(transfer_list(src_attr.value.aclcapability.action_list, dst_attr.value.aclcapability.action_list, countOnly));
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_SYSTEM_PORT_CONFIG:
//            transfer_primitive(src_attr.value.sysportconfig, dst_attr.value.sysportconfig);
//            break;

        //case OTAI_ATTR_VALUE_TYPE_AUTH_KEY:
        //    transfer_primitive(src_attr.value.authkey, dst_attr.value.authkey);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_AUTH_KEY:
        //    transfer_primitive(src_attr.value.macsecauthkey, dst_attr.value.macsecauthkey);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SALT:
        //    transfer_primitive(src_attr.value.macsecsalt, dst_attr.value.macsecsalt);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SAK:
        //    transfer_primitive(src_attr.value.macsecsak, dst_attr.value.macsecsak);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_ENCRYPT_KEY:
        //    transfer_primitive(src_attr.value.encrypt_key, dst_attr.value.encrypt_key);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_PORT_ERR_STATUS_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.porterror, dst_attr.value.porterror, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_PORT_EYE_VALUES_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.porteyevalues, dst_attr.value.porteyevalues, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_FABRIC_PORT_REACHABILITY:
        //    transfer_primitive(src_attr.value.reachability, dst_attr.value.reachability);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_PRBS_RX_STATE:
        //    transfer_primitive(src_attr.value.rx_state, dst_attr.value.rx_state);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_MAP_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.maplist, dst_attr.value.maplist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_TLV_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.tlvlist, dst_attr.value.tlvlist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_SYSTEM_PORT_CONFIG_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.sysportconfiglist, dst_attr.value.sysportconfiglist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_IP_PREFIX_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.ipprefixlist, dst_attr.value.ipprefixlist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_PORT_FREQUENCY_OFFSET_PPM_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.portfrequencyoffsetppmlist, dst_attr.value.portfrequencyoffsetppmlist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_PORT_SNR_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.portsnrlist, dst_attr.value.portsnrlist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_ACL_CHAIN_LIST:
        //    RETURN_ON_ERROR(transfer_list(src_attr.value.aclchainlist, dst_attr.value.aclchainlist, countOnly));
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_POE_PORT_POWER_CONSUMPTION:
        //    transfer_primitive(src_attr.value.portpowerconsumption, dst_attr.value.portpowerconsumption);
        //    break;

        default:
            SWSS_LOG_THROW("otai attr value %s is not implemented, FIXME", otai_serialize_attr_value_type(serialization_type).c_str());
    }

    return OTAI_STATUS_SUCCESS;
}

otai_status_t transfer_attributes(
        _In_ otai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ const otai_attribute_t *src_attr_list,
        _In_ otai_attribute_t *dst_attr_list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < attr_count; i++)
    {
        const otai_attribute_t &src_attr = src_attr_list[i];
        otai_attribute_t &dst_attr = dst_attr_list[i];

        auto meta = otai_metadata_get_attr_metadata(object_type, src_attr.id);

        if (src_attr.id != dst_attr.id)
        {
            SWSS_LOG_THROW("src (%d) vs dst (%d) attr id don't match GET mismatch", src_attr.id, dst_attr.id);
        }

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    otai_serialize_object_type(object_type).c_str(),
                    src_attr.id);
        }

        RETURN_ON_ERROR(transfer_attribute(meta->attrvaluetype, src_attr, dst_attr, countOnly));
    }

    return OTAI_STATUS_SUCCESS;
}

// util

//static uint8_t get_ip_mask(
//        _In_ const uint8_t* mask,
//        _In_ bool ipv6)
//{
//    SWSS_LOG_ENTER();
//
//    uint8_t ones = 0;
//    bool zeros = false;
//
//    uint8_t count = ipv6 ? 128 : 32;
//
//    for (uint8_t i = 0; i < count; i++)
//    {
//        bool bit = (mask[i/8]) & (1 << (7 - (i%8)));
//
//        if (zeros && bit)
//        {
//            SWSS_LOG_THROW("FATAL: invalid ipv%d mask", ipv6 ? 6 : 4);
//        }
//
//        zeros |= !bit;
//
//        if (bit)
//        {
//            ones++;
//        }
//    }
//
//    return ones;
//}
//
//static uint8_t get_ipv4_mask(
//        _In_ const otai_ip4_t mask)
//{
//    SWSS_LOG_ENTER();
//
//    return get_ip_mask((const uint8_t*)&mask, false);
//}
//
//static int get_ipv6_mask(
//        _In_ const otai_ip6_t &mask)
//{
//    SWSS_LOG_ENTER();
//
//    return get_ip_mask(mask, true);
//}
//
//static void otai_populate_ip_mask(
//        _In_ uint8_t bits,
//        _Out_ uint8_t *mask,
//        _In_ bool ipv6)
//{
//    SWSS_LOG_ENTER();
//
//    if (mask == NULL)
//    {
//        SWSS_LOG_THROW("mask pointer is null");
//    }
//
//    if ((ipv6 && (bits > 128)) || (!ipv6 && (bits > 32)))
//    {
//        SWSS_LOG_THROW("invalid ip mask bits %u", bits);
//    }
//
//    // zero all mask
//    memset(mask, 0, ipv6 ? 16 : 4);
//
//    // set full bytes to all ones
//    for (uint8_t i = 0; i < bits/8; i++)
//    {
//        mask[i] = 0xFF;
//    }
//
//    int rem = bits % 8;
//
//    if (rem)
//    {
//        // set last non zero byte
//        mask[bits/8] =(uint8_t)(0xFF << (8 - rem));
//    }
//}


// new methods

std::string otai_serialize_bool(
        _In_ bool b)
{
    SWSS_LOG_ENTER();

    return b ? "true" : "false";
}

#define CHAR_LEN 32

std::string otai_serialize_chardata(
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

//std::string otai_serialize_mac(
//        _In_ const otai_mac_t mac)
//{
//    SWSS_LOG_ENTER();
//
//    char buf[32];
//
//    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
//            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
//
//    return buf;
//}

template <typename T>
std::string otai_serialize_number(
        _In_ const T number,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    if (hex)
    {
        char buf[32];

        snprintf(buf, sizeof(buf), "0x%" PRIx64, (uint64_t)number);

        return buf;
    }

    return std::to_string(number);
}

std::string otai_serialize_enum(
        _In_ const int32_t value,
        _In_ const otai_enum_metadata_t* meta)
{
    SWSS_LOG_ENTER();

    if (meta == NULL)
    {
        return otai_serialize_number(value);
    }

    for (size_t i = 0; i < meta->valuescount; ++i)
    {
        if (meta->values[i] == value)
        {
            return meta->valuesnames[i];
        }
    }

    SWSS_LOG_WARN("enum value %d not found in enum %s", value, meta->name);

    return otai_serialize_number(value);
}

std::string otai_serialize_number(
        _In_ const uint32_t number,
        _In_ bool hex)
{
    SWSS_LOG_ENTER();

    return otai_serialize_number<uint32_t>(number, hex);
}

std::string otai_serialize_attr_id(
        _In_ const otai_attr_metadata_t& meta)
{
    SWSS_LOG_ENTER();

    return meta.attridname;
}

std::string otai_serialize_status(
        _In_ const otai_status_t status)
{
    SWSS_LOG_ENTER();

    return otai_serialize_enum(status, &otai_metadata_enum_otai_status_t);
}

std::string otai_serialize_common_api(
        _In_ const otai_common_api_t common_api)
{
    SWSS_LOG_ENTER();

    return otai_serialize_enum(common_api, &otai_metadata_enum_otai_common_api_t);
}

std::string otai_serialize_object_type(
        _In_ const otai_object_type_t object_type)
{
    SWSS_LOG_ENTER();

    return otai_serialize_enum(object_type, &otai_metadata_enum_otai_object_type_t);
}

std::string otai_serialize_log_level(
        _In_ otai_log_level_t log_level)
{
    SWSS_LOG_ENTER();

    return otai_serialize_enum(log_level, &otai_metadata_enum_otai_log_level_t);
}

std::string otai_serialize_api(
        _In_ otai_api_t api)
{
    SWSS_LOG_ENTER();

    return otai_serialize_enum(api, &otai_metadata_enum_otai_api_t);
}

std::string otai_serialize_attr_value_type(
        _In_ const otai_attr_value_type_t attr_value_type)
{
    SWSS_LOG_ENTER();

    return otai_serialize_enum(attr_value_type, &otai_metadata_enum_otai_attr_value_type_t);
}

//std::string otai_serialize_packet_color(
//        _In_ otai_packet_color_t color)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(color, &otai_metadata_enum_otai_packet_color_t);
//}
//
//std::string otai_serialize_vlan_id(
//        _In_ otai_vlan_id_t vlan_id)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_number(vlan_id);
//}

//std::string otai_serialize_neighbor_entry(
//        _In_ const otai_neighbor_entry_t &ne)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(ne.switch_id);
//    j["rif"] = otai_serialize_object_id(ne.rif_id);
//    j["ip"] = otai_serialize_ip_address(ne.ip_address);
//
//    return j.dump();
//}

#define EMIT(x)        buf += sprintf(buf, x)
#define EMIT_QUOTE     EMIT("\"")
#define EMIT_KEY(k)    EMIT("\"" k "\":")
#define EMIT_NEXT_KEY(k) { EMIT(","); EMIT_KEY(k); }
#define EMIT_CHECK(expr, suffix) {                              \
    ret = (expr);                                               \
    if (ret < 0) {                                              \
        SWSS_LOG_THROW("failed to serialize " #suffix ""); }    \
    buf += ret; }
#define EMIT_QUOTE_CHECK(expr, suffix) {\
    EMIT_QUOTE; EMIT_CHECK(expr, suffix); EMIT_QUOTE; }

//std::string otai_serialize_route_entry(
//        _In_ const otai_route_entry_t& route_entry)
//{
//    SWSS_LOG_ENTER();
//
//    // NOTE: this serialize is copy from OTAI/meta auto generated serialization
//    // but since previously we used json.hpp, then order of serialized item is
//    // different, so we copy actual serialize method and reorder names
//
//    // {"dest":"0.0.0.0/0","switch_id":"oid:0x21000000000000","vr":"oid:0x3000000000022"}
//
//    char buffer[256];
//    char *buf = buffer;
//
//    char *begin_buf = buf;
//    int ret;
//
//    EMIT("{");
//
//    EMIT_KEY("dest");
//
//    EMIT_QUOTE_CHECK(otai_serialize_ip_prefix(buf, &route_entry.destination), ip_prefix);
//
//    EMIT_NEXT_KEY("switch_id");
//
//    EMIT_QUOTE_CHECK(otai_serialize_object_id(buf, route_entry.switch_id), object_id);
//
//    EMIT_NEXT_KEY("vr");
//
//    EMIT_QUOTE_CHECK(otai_serialize_object_id(buf, route_entry.vr_id), object_id);
//
//    EMIT("}");
//
//    *buf = 0;
//
//    return std::string(begin_buf, (int)(buf - begin_buf));
//}

//std::string otai_serialize_ipmc_entry(
//        _In_ const otai_ipmc_entry_t& ipmc_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(ipmc_entry.switch_id);
//    j["vr_id"] = otai_serialize_object_id(ipmc_entry.vr_id);
//    j["type"] = otai_serialize_ipmc_entry_type(ipmc_entry.type);
//    j["destination"] = otai_serialize_ip_address(ipmc_entry.destination);
//    j["source"] = otai_serialize_ip_address(ipmc_entry.source);
//
//    return j.dump();
//}
//
//std::string otai_serialize_l2mc_entry(
//        _In_ const otai_l2mc_entry_t& l2mc_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(l2mc_entry.switch_id);
//    j["bv_id"] = otai_serialize_object_id(l2mc_entry.bv_id);
//    j["type"] = otai_serialize_l2mc_entry_type(l2mc_entry.type);
//    j["destination"] = otai_serialize_ip_address(l2mc_entry.destination);
//    j["source"] = otai_serialize_ip_address(l2mc_entry.source);
//
//    return j.dump();
//}
//
//std::string otai_serialize_mcast_fdb_entry(
//        _In_ const otai_mcast_fdb_entry_t& mcast_fdb_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(mcast_fdb_entry.switch_id);
//    j["bv_id"] = otai_serialize_object_id(mcast_fdb_entry.bv_id);
//    j["mac_address"] = otai_serialize_mac(mcast_fdb_entry.mac_address);
//
//    return j.dump();
//}
//
//std::string otai_serialize_inseg_entry(
//        _In_ const otai_inseg_entry_t& inseg_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(inseg_entry.switch_id);
//    j["label"] = otai_serialize_number(inseg_entry.label);
//
//    return j.dump();
//}
//
//std::string otai_serialize_fdb_entry(
//        _In_ const otai_fdb_entry_t& fdb_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(fdb_entry.switch_id);
//    j["mac"] = otai_serialize_mac(fdb_entry.mac_address);
//    j["bvid"] = otai_serialize_object_id(fdb_entry.bv_id);
//
//    return j.dump();
//}
//
//std::string otai_serialize_meter_bucket_entry(
//        _In_ const otai_meter_bucket_entry_t &meter_bucket_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(meter_bucket_entry.switch_id);
//    j["eni_id"] = otai_serialize_object_id(meter_bucket_entry.eni_id);
//    j["meter_class"] = otai_serialize_number<uint32_t>(meter_bucket_entry.meter_class);
//
//    return j.dump();
//}
//
//std::string otai_serialize_flow_entry(
//        _In_ const otai_flow_entry_t &flow_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(flow_entry.switch_id);
//    j["eni_mac"] = otai_serialize_mac(flow_entry.eni_mac);
//    j["vnet_id"] = otai_serialize_number<uint16_t>(flow_entry.vnet_id);
//    j["ip_proto"] = otai_serialize_number<uint8_t>(flow_entry.ip_proto);
//    j["src_ip"] = otai_serialize_ip_address(flow_entry.src_ip);
//    j["dst_ip"] = otai_serialize_ip_address(flow_entry.dst_ip);
//    j["src_port"] = otai_serialize_number<uint16_t>(flow_entry.src_port);
//    j["dst_port"] = otai_serialize_number<uint16_t>(flow_entry.dst_port);
//
//    return j.dump();
//}

//std::string otai_serialize_l2mc_entry_type(
//        _In_ const otai_l2mc_entry_type_t type)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(type, &otai_metadata_enum_otai_l2mc_entry_type_t);
//}
//
//std::string otai_serialize_ipmc_entry_type(
//        _In_ const otai_ipmc_entry_type_t type)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(type, &otai_metadata_enum_otai_ipmc_entry_type_t);
//}

std::string otai_serialize_port_stat(
        _In_ const otai_port_stat_t counter)
{
    SWSS_LOG_ENTER();

    return otai_serialize_enum(counter, &otai_metadata_enum_otai_port_stat_t);
}

//std::string otai_serialize_switch_stat(
//        _In_ const otai_switch_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_switch_stat_t);
//}
//
//std::string otai_serialize_port_pool_stat(
//        _In_ const otai_port_pool_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_port_pool_stat_t);
//}

//std::string otai_serialize_queue_stat(
//        _In_ const otai_queue_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_queue_stat_t);
//}

//std::string otai_serialize_router_interface_stat(
//        _In_ const otai_router_interface_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_router_interface_stat_t);
//}

//std::string otai_serialize_ingress_priority_group_stat(
//        _In_ const otai_ingress_priority_group_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_ingress_priority_group_stat_t);
//}
//
//std::string otai_serialize_ingress_priority_group_attr(
//        _In_ const otai_ingress_priority_group_attr_t attr)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(attr, &otai_metadata_enum_otai_ingress_priority_group_attr_t);
//}
//
//std::string otai_serialize_buffer_pool_stat(
//        _In_ const otai_buffer_pool_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_buffer_pool_stat_t);
//}
//
//std::string otai_serialize_eni_stat(
//        _In_ const otai_eni_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_eni_stat_t);
//}
//
//std::string otai_serialize_meter_bucket_entry_stat(
//        _In_ const otai_meter_bucket_entry_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_meter_bucket_entry_stat_t);
//}
//
//std::string otai_serialize_tunnel_stat(
//        _In_ const otai_tunnel_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_tunnel_stat_t);
//}

//std::string otai_serialize_counter_stat(
//        _In_ const otai_counter_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_counter_stat_t);
//}

//std::string otai_serialize_queue_attr(
//        _In_ const otai_queue_attr_t attr)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(attr, &otai_metadata_enum_otai_queue_attr_t);
//}

//std::string otai_serialize_macsec_flow_stat(
//        _In_ const otai_macsec_flow_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_macsec_flow_stat_t);
//}
//
//std::string otai_serialize_macsec_sa_stat(
//        _In_ const otai_macsec_sa_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_macsec_sa_stat_t);
//}
//
//std::string otai_serialize_macsec_sa_attr(
//        _In_ const  otai_macsec_sa_attr_t &attr)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(attr, &otai_metadata_enum_otai_macsec_sa_attr_t);
//}
//
//std::string otai_serialize_acl_counter_attr(
//        _In_ const  otai_acl_counter_attr_t &attr)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(attr, &otai_metadata_enum_otai_acl_counter_attr_t);
//}

//std::string otai_serialize_switch_oper_status(
//        _In_ otai_object_id_t switch_id,
//        _In_ otai_switch_oper_status_t status)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(switch_id);
//    j["status"] = otai_serialize_enum(status, &otai_metadata_enum_otai_switch_oper_status_t);
//
//    return j.dump();
//}
//
//std::string otai_serialize_port_host_tx_ready_status(
//        _In_ const otai_port_host_tx_ready_status_t status)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(status, &otai_metadata_enum_otai_port_host_tx_ready_status_t);
//}

//std::string otai_serialize_ingress_drop_reason(
//        _In_ const otai_in_drop_reason_t reason)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(reason, &otai_metadata_enum_otai_in_drop_reason_t);
//}

//std::string otai_serialize_egress_drop_reason(
//        _In_ const otai_out_drop_reason_t reason)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(reason, &otai_metadata_enum_otai_out_drop_reason_t);
//}

//std::string otai_serialize_timespec(
//        _In_ const otai_timespec_t &timespec)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["tv_sec"] = otai_serialize_number<uint64_t>(timespec.tv_sec);
//    j["tv_nsec"] = otai_serialize_number<uint32_t>(timespec.tv_nsec);
//
//    return j.dump();
//}

//std::string otai_serialize_switch_asic_sdk_health_event(
//        _In_ otai_object_id_t switch_id,
//        _In_ otai_switch_asic_sdk_health_severity_t severity,
//        _In_ const otai_timespec_t &timestamp,
//        _In_ otai_switch_asic_sdk_health_category_t category,
//        _In_ const otai_switch_health_data_t &data,
//        _In_ const otai_u8_list_t &description)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(switch_id);
//    j["severity"] = otai_serialize_enum(severity, &otai_metadata_enum_otai_switch_asic_sdk_health_severity_t);
//    j["timestamp"] = otai_serialize_timespec(timestamp);
//    j["category"] = otai_serialize_enum(category, &otai_metadata_enum_otai_switch_asic_sdk_health_category_t);
//    j["data.data_type"] = otai_serialize_enum(data.data_type, &otai_metadata_enum_otai_health_data_type_t);
//    j["description"] = otai_serialize_number_list(description, false);
//
//    return j.dump();
//}

//std::string otai_serialize_switch_shutdown_request(
//        _In_ otai_object_id_t switch_id)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(switch_id);
//
//    return j.dump();
//}
//
//std::string otai_serialize_ipv4(
//        _In_ otai_ip4_t ip)
//{
//    SWSS_LOG_ENTER();
//
//    char buf[INET_ADDRSTRLEN];
//
//    struct sockaddr_in sa;
//
//    memcpy(&sa.sin_addr, &ip, 4);
//
//    if (inet_ntop(AF_INET, &(sa.sin_addr), buf, INET_ADDRSTRLEN) == NULL)
//    {
//        SWSS_LOG_THROW("FATAL: failed to convert IPv4 address, errno: %s", strerror(errno));
//    }
//
//    return buf;
//}

std::string otai_serialize_pointer(
        _In_ otai_pointer_t ptr)
{
    SWSS_LOG_ENTER();

    return otai_serialize_number((uint64_t)ptr, true);
}

//std::string otai_serialize_ipv6(
//        _In_ const otai_ip6_t& ip)
//{
//    SWSS_LOG_ENTER();
//
//    char buf[INET6_ADDRSTRLEN];
//
//    struct sockaddr_in6 sa6;
//
//    memcpy(&sa6.sin6_addr, ip, 16);
//
//    if (inet_ntop(AF_INET6, &(sa6.sin6_addr), buf, INET6_ADDRSTRLEN) == NULL)
//    {
//        SWSS_LOG_THROW("FATAL: failed to convert IPv6 address, errno: %s", strerror(errno));
//    }
//
//    return buf;
//}
//
//std::string otai_serialize_ip_address(
//        _In_ const otai_ip_address_t& ipaddress)
//{
//    SWSS_LOG_ENTER();
//
//    switch (ipaddress.addr_family)
//    {
//        case OTAI_IP_ADDR_FAMILY_IPV4:
//
//            return otai_serialize_ipv4(ipaddress.addr.ip4);
//
//        case OTAI_IP_ADDR_FAMILY_IPV6:
//
//            return otai_serialize_ipv6(ipaddress.addr.ip6);
//
//        default:
//
//            SWSS_LOG_THROW("FATAL: invalid ip address family: %d", ipaddress.addr_family);
//    }
//}

std::string otai_serialize_object_id(
        _In_ otai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    char buf[32];

    snprintf(buf, sizeof(buf), "oid:0x%" PRIx64, oid);

    return buf;
}

template<typename T, typename F>
std::string otai_serialize_list(
        _In_ const T& list,
        _In_ bool countOnly,
        F serialize_item)
{
    SWSS_LOG_ENTER();

    std::string s = otai_serialize_number(list.count);

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

//std::string otai_serialize_ip_address_list(
//        _In_ const otai_ip_address_list_t& list,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_list(list, countOnly, [&](otai_ip_address_t item) { return otai_serialize_ip_address(item);} );
//}
//
//std::string otai_serialize_ip_prefix_list(
//        _In_ const otai_ip_prefix_list_t& list,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_list(list, countOnly, [&](otai_ip_prefix_t item) { return otai_serialize_ip_prefix(item);} );
//}

std::string otai_serialize_enum_list(
        _In_ const otai_s32_list_t& list,
        _In_ const otai_enum_metadata_t* meta,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    return otai_serialize_list(list, countOnly, [&](int32_t item) { return otai_serialize_enum(item, meta);} );
}

std::string otai_serialize_oid_list(
        _In_ const otai_object_list_t &list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    return otai_serialize_list(list, countOnly, [&](otai_object_id_t item) { return otai_serialize_object_id(item);} );
}

template <typename T>
std::string otai_serialize_number_list(
        _In_ const T& list,
        _In_ bool countOnly,
        _In_ bool hex)
{
    SWSS_LOG_ENTER();

    return otai_serialize_list(list, countOnly, [&](decltype(*list.list)& item) { return otai_serialize_number(item, hex);} );
}

//static json otai_serialize_qos_map_params(
//        _In_ const otai_qos_map_params_t& params)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["tc"]       = params.tc;
//    j["dscp"]     = params.dscp;
//    j["dot1p"]    = params.dot1p;
//    j["prio"]     = params.prio;
//    j["pg"]       = params.pg;
//    j["qidx"]     = params.queue_index;
//    j["mpls_exp"] = params.mpls_exp;
//    j["color"]    = otai_serialize_packet_color(params.color);
//    j["fc"]       = params.fc;
//
//    return j;
//}

//json otai_serialize_qos_map(
//        _In_ const otai_qos_map_t& qosmap)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["key"]    = otai_serialize_qos_map_params(qosmap.key);
//    j["value"]  = otai_serialize_qos_map_params(qosmap.value);;
//
//    return j;
//}
//
//std::string otai_serialize_qos_map_item(
//        _In_ const otai_qos_map_t& qosmap)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["key"]    = otai_serialize_qos_map_params(qosmap.key);
//    j["value"]  = otai_serialize_qos_map_params(qosmap.value);;
//
//    return j.dump();
//}

//std::string otai_serialize_qos_map_list(
//        _In_ const otai_qos_map_list_t& qosmap,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["count"] = qosmap.count;
//
//    if (qosmap.list == NULL || countOnly)
//    {
//        j["list"] = nullptr;
//
//        return j.dump();
//    }
//
//    json arr = json::array();
//
//    for (uint32_t i = 0; i < qosmap.count; ++i)
//    {
//        json item = otai_serialize_qos_map(qosmap.list[i]);
//
//        arr.push_back(item);
//    }
//
//    j["list"] = arr;
//
//    return j.dump();
//}

//json otai_serialize_map(
//    _In_ const otai_map_t &map)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["key"] = map.key;
//    j["value"] = map.value;
//
//    return j;
//}
//
//std::string otai_serialize_map_list(
//    _In_ const otai_map_list_t &maplist,
//    _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["count"] = maplist.count;
//
//    if (maplist.list == NULL || countOnly)
//    {
//        j["list"] = nullptr;
//
//        return j.dump();
//    }
//
//    json arr = json::array();
//
//    for (uint32_t i = 0; i < maplist.count; ++i)
//    {
//        json item = otai_serialize_map(maplist.list[i]);
//
//        arr.push_back(item);
//    }
//
//    j["list"] = arr;
//
//    return j.dump();
//}

//json otai_serialize_acl_resource(
//        _In_ const otai_acl_resource_t& aclresource)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["stage"]    = otai_serialize_enum(aclresource.stage, &otai_metadata_enum_otai_acl_stage_t);
//    j["bind_point"]  = otai_serialize_enum(aclresource.bind_point, &otai_metadata_enum_otai_acl_bind_point_type_t);
//    j["avail_num"]  = otai_serialize_number(aclresource.avail_num);
//
//    return j;
//}

//std::string otai_serialize_acl_resource_list(
//        _In_ const otai_acl_resource_list_t& aclresource,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["count"] = aclresource.count;
//
//    if (aclresource.list == NULL || countOnly)
//    {
//        j["list"] = nullptr;
//
//        return j.dump();
//    }
//
//    json arr = json::array();
//
//    for (uint32_t i = 0; i < aclresource.count; ++i)
//    {
//        json item = otai_serialize_acl_resource(aclresource.list[i]);
//
//        arr.push_back(item);
//    }
//
//    j["list"] = arr;
//
//    return j.dump();
//}

//std::string otai_serialize_latch_status(
//        _In_ const otai_latch_status_t& latch_status)
//{
//    SWSS_LOG_ENTER();
//
//    auto changed = otai_serialize_bool(latch_status.changed);
//    auto current_status = otai_serialize_bool(latch_status.current_status);
//
//    return changed + ":" + current_status;
//}
//
//json otai_serialize_port_lane_latch_status_item(
//        _In_ const otai_port_lane_latch_status_t& lane_latch_status)
//{
//    SWSS_LOG_ENTER();
//    json j;
//
//    j["lane"] = lane_latch_status.lane;
//    j["value"] = otai_serialize_latch_status(lane_latch_status.value);
//
//    return j;
//}
//
//std::string otai_serialize_port_lane_latch_status_list(
//        _In_ const otai_port_lane_latch_status_list_t& status_list,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["count"] = status_list.count;
//
//    if (status_list.list == NULL || countOnly)
//    {
//        j["list"] = nullptr;
//
//        return j.dump();
//    }
//
//    json arr = json::array();
//
//    for (uint32_t i = 0; i < status_list.count; ++i)
//    {
//        json item = otai_serialize_port_lane_latch_status_item(status_list.list[i]);
//
//        arr.push_back(item);
//    }
//
//    j["list"] = arr;
//
//    return j.dump();
//}

template <typename T>
std::string otai_serialize_range(
        _In_ const T& range)
{
    SWSS_LOG_ENTER();

    return otai_serialize_number(range.min) + "," + otai_serialize_number(range.max);
}

//std::string otai_serialize_u16_range_list(
//        _In_ const otai_u16_range_list_t& list,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_list(list, countOnly, [&](otai_u16_range_t item) { return otai_serialize_range(item);} );
//}

//std::string otai_serialize_acl_action(
//        _In_ const otai_attr_metadata_t& meta,
//        _In_ const otai_acl_action_data_t& action,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    if (action.enable == false)
//    {
//        // parameter is not needed when action is disabled
//        return "disabled";
//    }
//
//    switch (meta.attrvaluetype)
//    {
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
//            return otai_serialize_bool(action.parameter.booldata);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
//            return otai_serialize_number(action.parameter.u8);
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
////            return otai_serialize_number(action.parameter.s8);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
//            return otai_serialize_number(action.parameter.u16);
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
////            return otai_serialize_number(action.parameter.s16);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
//            return otai_serialize_number(action.parameter.u32);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
//            return otai_serialize_enum(action.parameter.s32, meta.enummetadata);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
//            return otai_serialize_mac(action.parameter.mac);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
//            return otai_serialize_ipv4(action.parameter.ip4);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
//            return otai_serialize_ipv6(action.parameter.ip6);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IP_ADDRESS:
//            return otai_serialize_ip_address(action.parameter.ipaddr);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
//            return otai_serialize_object_id(action.parameter.oid);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
//            return otai_serialize_oid_list(action.parameter.objlist, countOnly);
//
//        default:
//            SWSS_LOG_THROW("otai attr value %s is not implemented, FIXME", otai_serialize_attr_value_type(meta.attrvaluetype).c_str());
//    }
//}

//std::string otai_serialize_acl_field(
//        _In_ const otai_attr_metadata_t& meta,
//        _In_ const otai_acl_field_data_t& field,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    if (field.enable == false)
//    {
//        // parameter is not needed when field is disabled
//        return "disabled";
//    }
//
//    switch (meta.attrvaluetype)
//    {
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
//            return otai_serialize_bool(field.data.booldata);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
//            return otai_serialize_number(field.data.u8) + "&mask:" + otai_serialize_number(field.mask.u8, true);
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
////            return otai_serialize_number(field.data.s8) + "&mask:" + otai_serialize_number(field.mask.s8, true);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
//            return otai_serialize_number(field.data.u16) + "&mask:" + otai_serialize_number(field.mask.u16, true);
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
////            return otai_serialize_number(field.data.s16) + "&mask:" + otai_serialize_number(field.mask.s16, true);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
//            return otai_serialize_number(field.data.u32) + "&mask:" + otai_serialize_number(field.mask.u32, true);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
//            return otai_serialize_enum(field.data.s32, meta.enummetadata) + "&mask:" + otai_serialize_number(field.mask.s32, true);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT64:
//            return otai_serialize_number(field.data.u64) + "&mask:" + otai_serialize_number(field.mask.u64, true);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
//            return otai_serialize_mac(field.data.mac) +"&mask:" + otai_serialize_mac(field.mask.mac);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
//            return otai_serialize_ipv4(field.data.ip4) +"&mask:" + otai_serialize_ipv4(field.mask.ip4);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
//            return otai_serialize_ipv6(field.data.ip6) +"&mask:" + otai_serialize_ipv6(field.mask.ip6);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
//            return otai_serialize_object_id(field.data.oid);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
//            return otai_serialize_oid_list(field.data.objlist, countOnly);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
//            return otai_serialize_number_list(field.data.u8list, countOnly) + "&mask:" + otai_serialize_number_list(field.mask.u8list, countOnly, true);
//
//        default:
//            SWSS_LOG_THROW("otai attr value %s is not implemented, FIXME", otai_serialize_attr_value_type(meta.attrvaluetype).c_str());
//    }
//}

//std::string otai_serialize_acl_capability(
//        _In_ const otai_attr_metadata_t &meta,
//        _In_ const otai_acl_capability_t &cap,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    auto mandatory = otai_serialize_bool(cap.is_action_list_mandatory);
//
//    auto list = otai_serialize_enum_list(cap.action_list, &otai_metadata_enum_otai_acl_action_type_t, countOnly);
//
//    return mandatory + ":" + list;
//}

std::string otai_serialize_hex_binary(
        _In_ const void *buffer,
        _In_ size_t length)
{
    SWSS_LOG_ENTER();

    std::string s;

    if (buffer == NULL || length == 0)
    {
        return s;
    }

    s.resize(2 * length, '0');

    const unsigned char *input = static_cast<const unsigned char *>(buffer);
    char *output = &s[0];

    for (size_t i = 0; i < length; i++)
    {
        snprintf(&output[i * 2], 3, "%02X", input[i]);
    }

    return s;
}

//std::string otai_serialize_direction_lookup_entry(
//        _In_ const otai_direction_lookup_entry_t &direction_lookup_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(direction_lookup_entry.switch_id);
//    j["vni"] = otai_serialize_number(direction_lookup_entry.vni);
//
//    return j.dump();
//}
//
//std::string otai_serialize_eni_ether_address_map_entry(
//        _In_ const otai_eni_ether_address_map_entry_t &eni_ether_address_map_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(eni_ether_address_map_entry.switch_id);
//    j["address"] = otai_serialize_mac(eni_ether_address_map_entry.address);
//
//    return j.dump();
//}
//
//std::string otai_serialize_vip_entry(
//        _In_ const otai_vip_entry_t &vip_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(vip_entry.switch_id);
//    j["vip"] = otai_serialize_ip_address(vip_entry.vip);
//
//    return j.dump();
//}
//
//std::string otai_serialize_inbound_routing_entry(
//        _In_ const otai_inbound_routing_entry_t &inbound_routing_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(inbound_routing_entry.switch_id);
//    j["eni_id"] = otai_serialize_object_id(inbound_routing_entry.eni_id);
//    j["vni"] = otai_serialize_number(inbound_routing_entry.vni);
//    j["sip"] = otai_serialize_ip_address(inbound_routing_entry.sip);
//    j["sip_mask"] = otai_serialize_ip_address(inbound_routing_entry.sip_mask);
//    j["priority"] = otai_serialize_number(inbound_routing_entry.priority);
//
//    return j.dump();
//}
//
//std::string otai_serialize_pa_validation_entry(
//        _In_ const otai_pa_validation_entry_t &pa_validation_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(pa_validation_entry.switch_id);
//    j["vnet_id"] = otai_serialize_object_id(pa_validation_entry.vnet_id);
//    j["sip"] = otai_serialize_ip_address(pa_validation_entry.sip);
//
//    return j.dump();
//}
//
//std::string otai_serialize_outbound_routing_entry(
//        _In_ const otai_outbound_routing_entry_t &outbound_routing_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(outbound_routing_entry.switch_id);
//    j["destination"] = otai_serialize_ip_prefix(outbound_routing_entry.destination);
//    j["outbound_routing_group_id"] = otai_serialize_object_id(outbound_routing_entry.outbound_routing_group_id);
//
//    return j.dump();
//}
//
//std::string otai_serialize_outbound_ca_to_pa_entry(
//        _In_ const otai_outbound_ca_to_pa_entry_t &outbound_ca_to_pa_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(outbound_ca_to_pa_entry.switch_id);
//    j["dst_vnet_id"] = otai_serialize_object_id(outbound_ca_to_pa_entry.dst_vnet_id);
//    j["dip"] = otai_serialize_ip_address(outbound_ca_to_pa_entry.dip);
//
//    return j.dump();
//}
//
//std::string otai_serialize_system_port_config(
//        _In_ const otai_attr_metadata_t &meta,
//        _In_ const otai_system_port_config_t &sysportconfig)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["port_id"] = otai_serialize_number(sysportconfig.port_id, false);
//    j["attached_switch_id"] = otai_serialize_number(sysportconfig.attached_switch_id, false);
//    j["attached_core_index"] = otai_serialize_number(sysportconfig.attached_core_index, false);
//    j["attached_core_port_index"] = otai_serialize_number(sysportconfig.attached_core_port_index, false);
//    j["speed"] = otai_serialize_number(sysportconfig.speed, false);
//    j["num_voq"] = otai_serialize_number(sysportconfig.num_voq, false);
//
//    return j.dump();
//}

//std::string otai_serialize_json(
//    _In_ const otai_json_t js)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["json"] = otai_serialize_number_list(js.json, false);
//
//    return j.dump();
//}

//json otai_serialize_system_port_cfg_list_item(
//        _In_ const otai_system_port_config_t &sysportconfig)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["port_id"] = otai_serialize_number(sysportconfig.port_id, false);
//    j["attached_switch_id"] = otai_serialize_number(sysportconfig.attached_switch_id, false);
//    j["attached_core_index"] = otai_serialize_number(sysportconfig.attached_core_index, false);
//    j["attached_core_port_index"] = otai_serialize_number(sysportconfig.attached_core_port_index, false);
//    j["speed"] = otai_serialize_number(sysportconfig.speed, false);
//    j["num_voq"] = otai_serialize_number(sysportconfig.num_voq, false);
//
//    return j;
//}
//
//std::string otai_serialize_system_port_config_list(
//        _In_ const otai_attr_metadata_t &meta,
//        _In_ const otai_system_port_config_list_t& sysportconfiglist,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["count"] = sysportconfiglist.count;
//
//    if (sysportconfiglist.list == NULL || countOnly)
//    {
//        j["list"] = nullptr;
//
//        return j.dump();
//    }
//
//    json arr = json::array();
//
//    for (uint32_t i = 0; i < sysportconfiglist.count; ++i)
//    {
//        json item = otai_serialize_system_port_cfg_list_item(sysportconfiglist.list[i]);
//
//        arr.push_back(item);
//    }
//
//    j["list"] = arr;
//
//    return j.dump();
//}
//
//std::string otai_serialize_segment_list(
//        _In_ const otai_segment_list_t& segmentlist,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    std::string s = otai_serialize_number(segmentlist.count);
//
//    if (countOnly)
//    {
//        return s;
//    }
//
//    if (segmentlist.list == NULL || segmentlist.count == 0)
//    {
//        return s + ":null";
//    }
//
//    std::string l;
//
//    for (uint32_t i = 0; i < segmentlist.count; ++i)
//    {
//        l += otai_serialize_ipv6(segmentlist.list[i]);
//
//        if (i != segmentlist.count -1)
//        {
//            l += ",";
//        }
//    }
//
//    return s + ":" + l;
//}

std::string otai_serialize_attr_value(
        _In_ const otai_attr_metadata_t& meta,
        _In_ const otai_attribute_t &attr,
        _In_ const bool countOnly)
{
    SWSS_LOG_ENTER();

    switch (meta.attrvaluetype)
    {
        case OTAI_ATTR_VALUE_TYPE_BOOL:
            return otai_serialize_bool(attr.value.booldata);

        case OTAI_ATTR_VALUE_TYPE_CHARDATA:
            return otai_serialize_chardata(attr.value.chardata);

        case OTAI_ATTR_VALUE_TYPE_UINT8:
            return otai_serialize_number(attr.value.u8);

        case OTAI_ATTR_VALUE_TYPE_INT8:
            return otai_serialize_number(attr.value.s8);

        case OTAI_ATTR_VALUE_TYPE_UINT16:
            return otai_serialize_number(attr.value.u16);

        //case OTAI_ATTR_VALUE_TYPE_JSON:
        //    return otai_serialize_json(attr.value.json);

        case OTAI_ATTR_VALUE_TYPE_INT16:
            return otai_serialize_number(attr.value.s16);

        case OTAI_ATTR_VALUE_TYPE_UINT32:
            return otai_serialize_number(attr.value.u32);

        case OTAI_ATTR_VALUE_TYPE_INT32:
            return otai_serialize_enum(attr.value.s32, meta.enummetadata);

        case OTAI_ATTR_VALUE_TYPE_UINT64:
            return otai_serialize_number(attr.value.u64);

//        case OTAI_ATTR_VALUE_TYPE_INT64:
//            return otai_serialize_number(attr.value.s64);

        //case OTAI_ATTR_VALUE_TYPE_MAC:
        //    return otai_serialize_mac(attr.value.mac);

        //case OTAI_ATTR_VALUE_TYPE_IPV4:
        //    return otai_serialize_ipv4(attr.value.ip4);

        //case OTAI_ATTR_VALUE_TYPE_IPV6:
        //    return otai_serialize_ipv6(attr.value.ip6);

        case OTAI_ATTR_VALUE_TYPE_POINTER:
            return otai_serialize_pointer(attr.value.ptr);

        //case OTAI_ATTR_VALUE_TYPE_IP_ADDRESS:
        //    return otai_serialize_ip_address(attr.value.ipaddr);

        //case OTAI_ATTR_VALUE_TYPE_IP_PREFIX:
        //    return otai_serialize_ip_prefix(attr.value.ipprefix);

        case OTAI_ATTR_VALUE_TYPE_OBJECT_ID:
            return otai_serialize_object_id(attr.value.oid);

        case OTAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            return otai_serialize_oid_list(attr.value.objlist, countOnly);

        case OTAI_ATTR_VALUE_TYPE_UINT8_LIST:
            return otai_serialize_number_list(attr.value.u8list, countOnly);

        case OTAI_ATTR_VALUE_TYPE_INT8_LIST:
            return otai_serialize_number_list(attr.value.s8list, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_LATCH_STATUS:
        //    return otai_serialize_latch_status(attr.value.latchstatus);

        //case OTAI_ATTR_VALUE_TYPE_PORT_LANE_LATCH_STATUS_LIST:
        //    return otai_serialize_port_lane_latch_status_list(attr.value.portlanelatchstatuslist, countOnly);

//        case OTAI_ATTR_VALUE_TYPE_UINT16_LIST:
//            return otai_serialize_number_list(attr.value.u16list, countOnly);
//
//        case OTAI_ATTR_VALUE_TYPE_INT16_LIST:
//            return otai_serialize_number_list(attr.value.s16list, countOnly);

        case OTAI_ATTR_VALUE_TYPE_UINT32_LIST:
            return otai_serialize_number_list(attr.value.u32list, countOnly);

        case OTAI_ATTR_VALUE_TYPE_INT32_LIST:
            return otai_serialize_enum_list(attr.value.s32list, meta.enummetadata, countOnly);

        case OTAI_ATTR_VALUE_TYPE_UINT32_RANGE:
            return otai_serialize_range(attr.value.u32range);

//        case OTAI_ATTR_VALUE_TYPE_INT32_RANGE:
//            return otai_serialize_range(attr.value.s32range);

        //case OTAI_ATTR_VALUE_TYPE_UINT16_RANGE_LIST:
        //    return otai_serialize_u16_range_list(attr.value.u16rangelist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_VLAN_LIST:
        //    return otai_serialize_number_list(attr.value.vlanlist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
        //    return otai_serialize_qos_map_list(attr.value.qosmap, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_MAP_LIST:
        //    return otai_serialize_map_list(attr.value.maplist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_ACL_RESOURCE_LIST:
        //    return otai_serialize_acl_resource_list(attr.value.aclresource, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
        //    return otai_serialize_ip_address_list(attr.value.ipaddrlist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_SEGMENT_LIST:
        //    return otai_serialize_segment_list(attr.value.segmentlist, countOnly);

            // ACL FIELD DATA

        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT64:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
        //    return otai_serialize_acl_field(meta, attr.value.aclfield, countOnly);

        //    // ACL ACTION DATA

        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IP_ADDRESS:
        //    return otai_serialize_acl_action(meta, attr.value.aclaction, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_ACL_CAPABILITY:
        //    return otai_serialize_acl_capability(meta, attr.value.aclcapability, countOnly);

            // MACsec Attributions

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SAK:
        //    return otai_serialize_hex_binary(attr.value.macsecsak);

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_AUTH_KEY:
        //    return otai_serialize_hex_binary(attr.value.macsecauthkey);

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SALT:
        //    return otai_serialize_hex_binary(attr.value.macsecsalt);

        //case OTAI_ATTR_VALUE_TYPE_AUTH_KEY:
        //    return otai_serialize_hex_binary(attr.value.authkey);

        //case OTAI_ATTR_VALUE_TYPE_ENCRYPT_KEY:
        //    return otai_serialize_hex_binary(attr.value.encrypt_key);

        //case OTAI_ATTR_VALUE_TYPE_SYSTEM_PORT_CONFIG:
        //    return otai_serialize_system_port_config(meta, attr.value.sysportconfig);

        //case OTAI_ATTR_VALUE_TYPE_SYSTEM_PORT_CONFIG_LIST:
        //    return otai_serialize_system_port_config_list(meta, attr.value.sysportconfiglist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_IP_PREFIX_LIST:
        //    return otai_serialize_ip_prefix_list(attr.value.ipprefixlist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_POE_PORT_POWER_CONSUMPTION:
        //    return otai_serialize_poe_port_power_consumption(attr.value.portpowerconsumption);

        default:
            SWSS_LOG_THROW("otai attr value type %s is not implemented, FIXME", otai_serialize_attr_value_type(meta.attrvaluetype).c_str());
    }
}

//std::string otai_serialize_ip_prefix(
//        _In_ const otai_ip_prefix_t& prefix)
//{
//    SWSS_LOG_ENTER();
//
//    switch (prefix.addr_family)
//    {
//        case OTAI_IP_ADDR_FAMILY_IPV4:
//
//            return otai_serialize_ipv4(prefix.addr.ip4) + "/" + otai_serialize_number((get_ipv4_mask(prefix.mask.ip4)));
//
//        case OTAI_IP_ADDR_FAMILY_IPV6:
//
//            return otai_serialize_ipv6(prefix.addr.ip6) + "/" + otai_serialize_number(get_ipv6_mask(prefix.mask.ip6));
//
//        default:
//
//            SWSS_LOG_THROW("FATAL: invalid ip prefix address family: %d", prefix.addr_family);
//    }
//}

//std::string otai_serialize_port_oper_status(
//        _In_ otai_port_oper_status_t status)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(status, &otai_metadata_enum_otai_port_oper_status_t);
//}

//std::string otai_serialize_port_error_status(
//        _In_ otai_port_error_status_t status)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(status, &otai_metadata_enum_otai_port_error_status_t);
//}

//std::string otai_serialize_port_host_tx_ready(
//        _In_ otai_port_host_tx_ready_status_t host_tx_ready_status)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(host_tx_ready_status, &otai_metadata_enum_otai_port_host_tx_ready_status_t);
//}
//
//std::string otai_serialize_queue_deadlock_event(
//        _In_ otai_queue_pfc_deadlock_event_type_t event)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(event, &otai_metadata_enum_otai_queue_pfc_deadlock_event_type_t);
//}
//
//std::string otai_serialize_fdb_event(
//        _In_ otai_fdb_event_t event)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(event, &otai_metadata_enum_otai_fdb_event_t);
//}

//static json otai_serialize_json_fdb_event_notification_data(
//        _In_ const otai_fdb_event_notification_data_t& fdb_event)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["fdb_event"] = otai_serialize_fdb_event(fdb_event.event_type);
//    j["fdb_entry"] = otai_serialize_fdb_entry(fdb_event.fdb_entry);
//
//    json arr = json::array();
//
//    for (uint32_t i = 0; i < fdb_event.attr_count; ++i)
//    {
//        auto meta = otai_metadata_get_attr_metadata(OTAI_OBJECT_TYPE_FDB_ENTRY, fdb_event.attr[i].id);
//
//        if (meta == NULL)
//        {
//            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
//                    otai_serialize_object_type(OTAI_OBJECT_TYPE_FDB_ENTRY).c_str(),
//                    fdb_event.attr[i].id);
//        }
//
//        json item;
//
//        item["id"] = meta->attridname;
//        item["value"] = otai_serialize_attr_value(*meta, fdb_event.attr[i]);
//
//        arr.push_back(item);
//    }
//
//    j["list"] = arr;
//
//    // we don't need count since it can be deduced
//    return j;
//}

//std::string otai_serialize_nat_event(
//        _In_ otai_nat_event_t event)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(event, &otai_metadata_enum_otai_nat_event_t);
//}
//
//static json otai_serialize_json_nat_event_notification_data(
//        _In_ const otai_nat_event_notification_data_t& nat_event)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["nat_event"] = otai_serialize_nat_event(nat_event.event_type);
//    j["nat_entry"] = otai_serialize_nat_entry(nat_event.nat_entry);
//
//    // we don't need count since it can be deduced
//    return j;
//}
//
//std::string otai_serialize_bfd_session_state(
//        _In_ otai_bfd_session_state_t status)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(status, &otai_metadata_enum_otai_bfd_session_state_t);
//}
//
//std::string otai_serialize_twamp_session_state(
//        _In_ otai_twamp_session_state_t status)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(status, &otai_metadata_enum_otai_twamp_session_state_t);
//}
//
//std::string otai_serialize_twamp_session_stat(
//        _In_ otai_twamp_session_stat_t counter)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(counter, &otai_metadata_enum_otai_twamp_session_stat_t);
//}
//
//static json otai_serialize_json_twamp_session_event_notification_data(
//        _In_ const otai_twamp_session_event_notification_data_t& twamp_session_data)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["twamp_session_id"] = otai_serialize_object_id(twamp_session_data.twamp_session_id);
//    j["session_state"] = otai_serialize_twamp_session_state(twamp_session_data.session_state);
//
//    j["index"] = otai_serialize_number(twamp_session_data.session_stats.index);
//
//    json arr = json::array();
//
//    for (uint32_t i = 0; i < twamp_session_data.session_stats.number_of_counters; ++i)
//    {
//        json item;
//
//        item["counters_ids"] = otai_serialize_twamp_session_stat(twamp_session_data.session_stats.counters_ids[i]);
//        item["counters"] = otai_serialize_number(twamp_session_data.session_stats.counters[i]);
//
//        arr.push_back(item);
//    }
//
//    j["list"] = arr;
//
//    // we don't need count since it can be deduced
//    return j;
//}

//std::string otai_serialize_fdb_event_ntf(
//        _In_ uint32_t count,
//        _In_ const otai_fdb_event_notification_data_t* fdb_event)
//{
//    SWSS_LOG_ENTER();
//
//    if (fdb_event == NULL)
//    {
//        SWSS_LOG_THROW("fdb_event pointer is null");
//    }
//
//    json j = json::array();
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        json item = otai_serialize_json_fdb_event_notification_data(fdb_event[i]);
//
//        j.push_back(item);
//    }
//
//    // we don't need count since it can be deduced
//    return j.dump();
//}

//std::string otai_serialize_nat_event_ntf(
//        _In_ uint32_t count,
//        _In_ const otai_nat_event_notification_data_t* nat_event)
//{
//    SWSS_LOG_ENTER();
//
//    if (nat_event == NULL)
//    {
//        SWSS_LOG_THROW("nat_event pointer is null");
//    }
//
//    json j = json::array();
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        json item = otai_serialize_json_nat_event_notification_data(nat_event[i]);
//
//        j.push_back(item);
//    }
//
//    // we don't need count since it can be deduced
//    return j.dump();
//}
//
//std::string otai_serialize_port_oper_status_ntf(
//        _In_ uint32_t count,
//        _In_ const otai_port_oper_status_notification_t* port_oper_status)
//{
//    SWSS_LOG_ENTER();
//
//    if (port_oper_status == NULL)
//    {
//        SWSS_LOG_THROW("port_oper_status pointer is null");
//    }
//
//    json j = json::array();
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        json item;
//
//        item["port_id"] = otai_serialize_object_id(port_oper_status[i].port_id);
//        item["port_state"] = otai_serialize_port_oper_status(port_oper_status[i].port_state);
//        item["port_error_status"] = otai_serialize_port_error_status(port_oper_status[i].port_error_status);
//
//        j.push_back(item);
//    }
//
//    // we don't need count since it can be deduced
//    return j.dump();
//}
//
//std::string otai_serialize_port_host_tx_ready_ntf(
//        _In_ otai_object_id_t switch_id,
//        _In_ otai_object_id_t port_id,
//        _In_ otai_port_host_tx_ready_status_t host_tx_ready_status)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::array();
//    json item;
//
//    item["port_id"] = otai_serialize_object_id(port_id);
//    item["switch_id"] = otai_serialize_object_id(switch_id);
//    item["host_tx_ready_status"] = otai_serialize_port_host_tx_ready_status(host_tx_ready_status);
//
//    j.push_back(item);
//
//    return j.dump();
//}
//
//std::string otai_serialize_queue_deadlock_ntf(
//        _In_ uint32_t count,
//        _In_ const otai_queue_deadlock_notification_data_t* deadlock_data)
//{
//    SWSS_LOG_ENTER();
//
//    if (deadlock_data == NULL)
//    {
//        SWSS_LOG_THROW("deadlock_data pointer is null");
//    }
//
//    json j = json::array();
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        json item;
//
//        item["queue_id"] = otai_serialize_object_id(deadlock_data[i].queue_id);
//        item["event"] = otai_serialize_queue_deadlock_event(deadlock_data[i].event);
//
//        j.push_back(item);
//    }
//
//    // we don't need count since it can be deduced
//    return j.dump();
//}
//
//std::string otai_serialize_bfd_session_state_ntf(
//        _In_ uint32_t count,
//        _In_ const otai_bfd_session_state_notification_t* bfd_session_state)
//{
//    SWSS_LOG_ENTER();
//
//    if (bfd_session_state == NULL)
//    {
//        SWSS_LOG_THROW("bfd_session_state pointer is null");
//    }
//
//    json j = json::array();
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        json item;
//
//        item["bfd_session_id"] = otai_serialize_object_id(bfd_session_state[i].bfd_session_id);
//        item["session_state"] = otai_serialize_bfd_session_state(bfd_session_state[i].session_state);
//
//        j.push_back(item);
//    }
//
//    // we don't need count since it can be deduced
//    return j.dump();
//}
//
//std::string otai_serialize_twamp_session_event_ntf(
//        _In_ uint32_t count,
//        _In_ const otai_twamp_session_event_notification_data_t* twamp_session_event)
//{
//    SWSS_LOG_ENTER();
//
//    if (twamp_session_event == NULL)
//    {
//        SWSS_LOG_THROW("twamp_session_state pointer is null");
//    }
//
//    json j = json::array();
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        json item = otai_serialize_json_twamp_session_event_notification_data(twamp_session_event[i]);
//
//        j.push_back(item);
//    }
//
//    // we don't need count since it can be deduced
//    return j.dump();
//}
//
//json otai_serialize_nat_entry_key(
//        _In_ const otai_nat_entry_key_t& nat_entry_key)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["src_ip"]      = otai_serialize_ipv4(nat_entry_key.src_ip);
//    j["dst_ip"]      = otai_serialize_ipv4(nat_entry_key.dst_ip);
//    j["proto"]       = otai_serialize_number(nat_entry_key.proto);
//    j["l4_src_port"] = otai_serialize_number(nat_entry_key.l4_src_port);
//    j["l4_dst_port"] = otai_serialize_number(nat_entry_key.l4_dst_port);
//
//    return j;
//}
//
//json otai_serialize_nat_entry_mask(
//        _In_ const otai_nat_entry_mask_t& nat_entry_mask)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["src_ip"]      = otai_serialize_ipv4(nat_entry_mask.src_ip);
//    j["dst_ip"]      = otai_serialize_ipv4(nat_entry_mask.dst_ip);
//    j["proto"]       = otai_serialize_number(nat_entry_mask.proto);
//    j["l4_src_port"] = otai_serialize_number(nat_entry_mask.l4_src_port);
//    j["l4_dst_port"] = otai_serialize_number(nat_entry_mask.l4_dst_port);
//
//    return j;
//}
//
//json otai_serialize_nat_entry_data(
//        _In_ const otai_nat_entry_data_t& nat_entry_data)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["key"]  = otai_serialize_nat_entry_key(nat_entry_data.key);
//    j["mask"] = otai_serialize_nat_entry_mask(nat_entry_data.mask);
//
//    return j;
//}
//
//std::string otai_serialize_nat_entry_type(
//        _In_ const otai_nat_type_t type)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(type, &otai_metadata_enum_otai_nat_type_t);
//}
//
//std::string otai_serialize_nat_entry(
//        _In_ const otai_nat_entry_t& nat_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"] = otai_serialize_object_id(nat_entry.switch_id);
//    j["vr"]        = otai_serialize_object_id(nat_entry.vr_id);
//    j["nat_type"]  = otai_serialize_nat_entry_type(nat_entry.nat_type);
//    j["nat_data"]  = otai_serialize_nat_entry_data(nat_entry.data);
//
//    return j.dump();
//}
//
//std::string otai_serialize_my_sid_entry(
//        _In_ const otai_my_sid_entry_t& my_sid_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["switch_id"]          = otai_serialize_object_id(my_sid_entry.switch_id);
//    j["vr_id"]              = otai_serialize_object_id(my_sid_entry.vr_id);
//    j["locator_block_len"]  = otai_serialize_number(my_sid_entry.locator_block_len);
//    j["locator_node_len"]   = otai_serialize_number(my_sid_entry.locator_node_len);
//    j["function_len"]       = otai_serialize_number(my_sid_entry.function_len);
//    j["args_len"]           = otai_serialize_number(my_sid_entry.args_len);
//    j["sid"]                = otai_serialize_ipv6(my_sid_entry.sid);
//
//    return j.dump();
//}

static bool otai_serialize_object_entry(
        _In_ const otai_object_type_t objecttype,
        _In_ const otai_object_key_entry_t& key_entry,
        _Out_ std::string& key)
{
    SWSS_LOG_ENTER();

    switch (objecttype)
    {
        //case OTAI_OBJECT_TYPE_FDB_ENTRY:
        //    key = otai_serialize_fdb_entry(key_entry.fdb_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_ROUTE_ENTRY:
        //    key = otai_serialize_route_entry(key_entry.route_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
        //    key = otai_serialize_neighbor_entry(key_entry.neighbor_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_NAT_ENTRY:
        //    key = otai_serialize_nat_entry(key_entry.nat_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_INSEG_ENTRY:
        //    key = otai_serialize_inseg_entry(key_entry.inseg_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_MY_SID_ENTRY:
        //    key = otai_serialize_my_sid_entry(key_entry.my_sid_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_L2MC_ENTRY:
        //    key = otai_serialize_l2mc_entry(key_entry.l2mc_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_IPMC_ENTRY:
        //    key = otai_serialize_ipmc_entry(key_entry.ipmc_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_MCAST_FDB_ENTRY:
        //    key = otai_serialize_mcast_fdb_entry(key_entry.mcast_fdb_entry);
        //    return true;

        default:
            return false;
    }
}

//static bool otai_serialize_object_extension_entry(
//        _In_ const otai_object_type_extensions_t objecttype,
//        _In_ const otai_object_key_entry_t& key_entry,
//        _Out_ std::string& key)
//{
//    SWSS_LOG_ENTER();
//
//    switch (objecttype)
//    {
//        case OTAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
//            key = otai_serialize_direction_lookup_entry(key_entry.direction_lookup_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
//            key = otai_serialize_eni_ether_address_map_entry(key_entry.eni_ether_address_map_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_VIP_ENTRY:
//            key = otai_serialize_vip_entry(key_entry.vip_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
//            key = otai_serialize_inbound_routing_entry(key_entry.inbound_routing_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
//            key = otai_serialize_pa_validation_entry(key_entry.pa_validation_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
//            key = otai_serialize_outbound_routing_entry(key_entry.outbound_routing_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
//            key = otai_serialize_outbound_ca_to_pa_entry(key_entry.outbound_ca_to_pa_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_FLOW_ENTRY:
//            key = otai_serialize_flow_entry(key_entry.flow_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_METER_BUCKET_ENTRY:
//            key = otai_serialize_meter_bucket_entry(key_entry.meter_bucket_entry);
//            return true;
//
//        default:
//            return false;
//    }
//}

std::string otai_serialize_object_meta_key(
        _In_ const otai_object_meta_key_t& meta_key)
{
    SWSS_LOG_ENTER();

    std::string key;

    if (!otai_metadata_is_object_type_valid(meta_key.objecttype))
    {
        SWSS_LOG_THROW("invalid object type value %s", otai_serialize_object_type(meta_key.objecttype).c_str());
    }

    if (!otai_serialize_object_entry(meta_key.objecttype, meta_key.objectkey.key, key))// &&
        //!otai_serialize_object_extension_entry((otai_object_type_extensions_t)meta_key.objecttype, meta_key.objectkey.key, key))
    {
        const auto& meta = otai_metadata_get_object_type_info(meta_key.objecttype);
        if (meta->isnonobjectid)
        {
            SWSS_LOG_THROW("object %s is non object id, not supported yet, FIXME",
                    otai_serialize_object_type(meta->objecttype).c_str());
        }

        key = otai_serialize_object_id(meta_key.objectkey.key.object_id);
    }

    key = otai_serialize_object_type(meta_key.objecttype) + ":" + key;

    SWSS_LOG_DEBUG("%s", key.c_str());

    return key;
}

std::string otai_serialize(
        _In_ const otai_redis_notify_syncd_t& value)
{
    SWSS_LOG_ENTER();

    switch (value)
    {
        case OTAI_REDIS_NOTIFY_SYNCD_INIT_VIEW:
            return SYNCD_INIT_VIEW;

        case OTAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW:
            return SYNCD_APPLY_VIEW;

        case OTAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC:
            return SYNCD_INSPECT_ASIC;

        case OTAI_REDIS_NOTIFY_SYNCD_INVOKE_DUMP:
            return SYNCD_INVOKE_DUMP;

        default:

            SWSS_LOG_THROW("unknown value on otai_redis_notify_syncd_t: %d", value);
    }
}

std::string otai_serialize_redis_communication_mode(
        _In_ otai_redis_communication_mode_t value)
{
    SWSS_LOG_ENTER();

    switch (value)
    {
        case OTAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC:
            return REDIS_COMMUNICATION_MODE_REDIS_ASYNC_STRING;

        case OTAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC:
            return REDIS_COMMUNICATION_MODE_REDIS_SYNC_STRING;

        case OTAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC:
            return REDIS_COMMUNICATION_MODE_ZMQ_SYNC_STRING;

        default:

            SWSS_LOG_THROW("unknown value on otai_redis_communication_mode_t: %d", value);
    }
}

std::string otai_serialize_redis_port_attr_id(
        _In_ const otai_redis_port_attr_t value)
{
    SWSS_LOG_ENTER();

    auto it = otai_redis_port_attr_to_name_map.find(value);
    if (it != otai_redis_port_attr_to_name_map.end())
    {
        return it->second;
    }

    SWSS_LOG_WARN("enum value %d not found in enum otai_redis_port_attr_t.", value);

    return otai_serialize_number(value, false);
}

std::string otai_serialize_redis_link_event_damping_algorithm(
        _In_ const otai_redis_link_event_damping_algorithm_t value)
{
    SWSS_LOG_ENTER();

    auto it = otai_redis_link_event_damping_algorithm_to_name_map.find(value);
    if (it != otai_redis_link_event_damping_algorithm_to_name_map.end())
    {
        return it->second;
    }

    SWSS_LOG_WARN("enum value %d not found in enum otai_redis_link_event_damping_algorithm_t.", value);

    return otai_serialize_number(value, false);
}

std::string otai_serialize_redis_link_event_damping_aied_config(
         _In_ const otai_redis_link_event_damping_algo_aied_config_t& value)
{
    SWSS_LOG_ENTER();

    json j;

    j["max_suppress_time"] = otai_serialize_number(value.max_suppress_time, false);
    j["suppress_threshold"] = otai_serialize_number(value.suppress_threshold, false);
    j["reuse_threshold"] = otai_serialize_number(value.reuse_threshold, false);
    j["decay_half_life"] = otai_serialize_number(value.decay_half_life, false);
    j["flap_penalty"] = otai_serialize_number(value.flap_penalty, false);

    return j.dump();
}

//std::string otai_serialize_poe_port_active_channel_type(
//        _In_ const otai_poe_port_active_channel_type_t value)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(value, &otai_metadata_enum_otai_poe_port_active_channel_type_t);
//}

//std::string otai_serialize_poe_port_class_method_type(
//        _In_ const otai_poe_port_class_method_type_t value)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(value, &otai_metadata_enum_otai_poe_port_class_method_type_t);
//}
//
//std::string otai_serialzie_poe_port_signature_type(
//        _In_ const otai_poe_port_signature_type_t value)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_serialize_enum(value, &otai_metadata_enum_otai_poe_port_signature_type_t);
//}
//
//std::string otai_serialize_poe_port_power_consumption(
//        _In_ const otai_poe_port_power_consumption_t& value)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//
//    j["active_channel"] = otai_serialize_poe_port_active_channel_type(value.active_channel);
//    j["voltage"] = otai_serialize_number(value.voltage, false);
//    j["current"] = otai_serialize_number(value.current, false);
//    j["consumption"] = otai_serialize_number(value.consumption, false);
//    j["signature_type"] = otai_serialzie_poe_port_signature_type(value.signature_type);
//    j["class_method"] = otai_serialize_poe_port_class_method_type(value.class_method);
//    j["measured_class_a"] = otai_serialize_number(value.measured_class_a, false);
//    j["assigned_class_a"] = otai_serialize_number(value.assigned_class_a, false);
//    j["measured_class_b"] = otai_serialize_number(value.measured_class_b, false);
//    j["assigned_class_b"] = otai_serialize_number(value.assigned_class_b, false);
//
//    return j.dump();
//}

// deserialize

void otai_deserialize_bool(
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

void otai_deserialize_chardata(
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
void otai_deserialize_number(
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

void otai_deserialize_number(
        _In_ const std::string& s,
        _Out_ uint32_t& number,
        _In_ bool hex)
{
    SWSS_LOG_ENTER();

    otai_deserialize_number<uint32_t>(s, number, hex);
}

void otai_deserialize_enum(
        _In_ const std::string& s,
        _In_ const otai_enum_metadata_t *meta,
        _Out_ int32_t& value)
{
    SWSS_LOG_ENTER();

    if (meta == NULL)
    {
        return otai_deserialize_number(s, value);
    }

    for (size_t i = 0; i < meta->valuescount; ++i)
    {
        if (strcmp(s.c_str(), meta->valuesnames[i]) == 0)
        {
            value = meta->values[i];
            return;
        }
    }

    // check depreacated values if present
    if (meta->ignorevaluesnames)
    {
        // this can happen when we deserialize older OTAI values

        for (size_t i = 0; meta->ignorevaluesnames[i] != NULL; i++)
        {
            if (strcmp(s.c_str(), meta->ignorevaluesnames[i]) == 0)
            {
                SWSS_LOG_NOTICE("translating depreacated/ignored enum value: %s", s.c_str());

                value = meta->ignorevalues[i];
                return;
            }
        }
    }

    SWSS_LOG_WARN("enum %s not found in enum %s", s.c_str(), meta->name);

    otai_deserialize_number(s, value);
}

//void otai_deserialize_mac(
//        _In_ const std::string& s,
//        _Out_ otai_mac_t& mac)
//{
//    SWSS_LOG_ENTER();
//
//    if (s.length() != (6*2+5))
//    {
//        SWSS_LOG_THROW("invalid mac address %s", s.c_str());
//    }
//
//    int i = 0;
//
//    for (int j = 0; j < 6 ; j++, i += 3)
//    {
//        int h = char_to_int(s[i+0]);
//        int l = char_to_int(s[i+1]);
//
//        int r = (h << 4) | l;
//
//        mac[j] = (unsigned char)r;
//    }
//}

void otai_deserialize_object_id(
        _In_ const std::string& s,
        _Out_ otai_object_id_t& oid)
{
    SWSS_LOG_ENTER();

    if (s.find("oid:0x") != 0)
    {
        SWSS_LOG_THROW("invalid oid %s", s.c_str());
    }

    errno = 0;

    char *endptr = NULL;

    oid = (otai_object_id_t)strtoull(s.c_str()+4, &endptr, 16);

    if (errno != 0 || endptr != s.c_str() + s.length())
    {
        SWSS_LOG_THROW("invalid oid %s", s.c_str());
    }
}

template<typename T, typename F>
void otai_deserialize_list(
        _In_ const std::string& s,
        _Out_ T& list,
        _In_ bool countOnly,
        F deserialize_item)
{
    SWSS_LOG_ENTER();

    if (countOnly)
    {
        otai_deserialize_number(s, list.count);
        return;
    }

    auto pos = s.find(":");

    if (pos == std::string::npos)
    {
        SWSS_LOG_THROW("invalid list %s", s.c_str());
    }

    std::string scount = s.substr(0, pos);

    otai_deserialize_number(scount, list.count);

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

    // list.list = otai_alloc_list(list.count, list);
    list.list = otai_alloc_n_of_ptr_type(list.count, list.list);

    for (uint32_t i = 0; i < list.count; ++i)
    {
        deserialize_item(tokens[i], list.list[i]);
    }
}

void otai_deserialize_oid_list(
        _In_ const std::string& s,
        _Out_ otai_object_list_t& objlist,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    otai_deserialize_list(s, objlist, countOnly, [&](const std::string sitem, otai_object_id_t& item) { otai_deserialize_object_id(sitem, item);} );
}

void otai_deserialize_enum_list(
        _In_ const std::string& s,
        _In_ const otai_enum_metadata_t* meta,
        _Out_ otai_s32_list_t& list,
        _In_ bool countOnly)
{
    SWSS_LOG_ENTER();

    otai_deserialize_list(s, list, countOnly, [&](const std::string sitem, int32_t& item) { otai_deserialize_enum(sitem, meta, item);} );
}

template <typename T>
void otai_deserialize_number_list(
        _In_ const std::string& s,
        _Out_ T& list,
        _In_ bool countOnly,
        _In_ bool hex = false)
{
    SWSS_LOG_ENTER();

    otai_deserialize_list(s, list, countOnly, [&](const std::string sitem, decltype(*list.list)& item) { otai_deserialize_number(sitem, item, hex);} );
}

//void otai_deserialize_packet_color(
//        _In_ const std::string& s,
//        _Out_ otai_packet_color_t& color)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_packet_color_t, (int32_t&)color);
//}

//static void otai_deserialize_qos_map_params(
//        _In_ const json& j,
//        _Out_ otai_qos_map_params_t& params)
//{
//    SWSS_LOG_ENTER();
//
//    params.tc             = j["tc"];
//    params.dscp           = j["dscp"];
//    params.dot1p          = j["dot1p"];
//    params.prio           = j["prio"];
//    params.pg             = j["pg"];
//    params.queue_index    = j["qidx"];
//
//    if (j.find("mpls_exp") == j.end())
//    {
//        // for backward compatibility
//        params.mpls_exp       = 0;
//    }
//    else
//    {
//        params.mpls_exp       = j["mpls_exp"];
//    }
//
//    if (j.find("fc") == j.end())
//    {
//        // for backward compatibility
//        params.fc       = 0;
//    }
//    else
//    {
//        params.fc       = j["fc"];
//    }
//
//    otai_deserialize_packet_color(j["color"], params.color);
//}
//
//static void otai_deserialize_qos_map(
//        _In_ const json& j,
//        _Out_ otai_qos_map_t& qosmap)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_qos_map_params(j["key"], qosmap.key);
//    otai_deserialize_qos_map_params(j["value"], qosmap.value);
//}
//
//void otai_deserialize_map(
//    _In_ const json &j,
//    _Out_ otai_map_t &map)
//{
//    SWSS_LOG_ENTER();
//
//    map.key =   j["key"];
//    map.value = j["value"];
//}
//
//void otai_deserialize_qos_map_list(
//        _In_ const std::string& s,
//        _Out_ otai_qos_map_list_t& qosmap,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    qosmap.count = j["count"];
//
//    if (countOnly)
//    {
//        return;
//    }
//
//    if (j["list"] == nullptr)
//    {
//        qosmap.list = NULL;
//        return;
//    }
//
//    json arr = j["list"];
//
//    if (arr.size() != (size_t)qosmap.count)
//    {
//        SWSS_LOG_THROW("qos map count mismatch %lu vs %u", arr.size(), qosmap.count);
//    }
//
//    qosmap.list = otai_alloc_n_of_ptr_type(qosmap.count, qosmap.list);
//
//    for (uint32_t i = 0; i < qosmap.count; ++i)
//    {
//        const json& item = arr[i];
//
//        otai_deserialize_qos_map(item, qosmap.list[i]);
//    }
//}
//
//void otai_deserialize_map_list(
//    _In_ const std::string &s,
//    _Out_ otai_map_list_t &maplist,
//    _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    maplist.count = j["count"];
//
//    if (countOnly)
//    {
//        return;
//    }
//
//    if (j["list"] == nullptr)
//    {
//        maplist.list = NULL;
//        return;
//    }
//
//    json arr = j["list"];
//
//    if (arr.size() != (size_t)maplist.count)
//    {
//        SWSS_LOG_THROW("map list count mismatch %lu vs %u", arr.size(), maplist.count);
//    }
//
//    maplist.list = otai_alloc_n_of_ptr_type(maplist.count, maplist.list);
//
//    for (uint32_t i = 0; i < maplist.count; ++i)
//    {
//        const json &item = arr[i];
//
//        otai_deserialize_map(item, maplist.list[i]);
//    }
//}
//
//void otai_deserialize_acl_stage(
//        _In_ const std::string& s,
//        _Out_ otai_acl_stage_t& stage)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_acl_stage_t, (int32_t&)stage);
//}
//
//void otai_deserialize_acl_bind_point(
//        _In_ const std::string& s,
//        _Out_ otai_acl_bind_point_type_t& bind_point)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_acl_bind_point_type_t, (int32_t&)bind_point);
//}
//
//static void otai_deserialize_acl_resource(
//        _In_ const json& j,
//        _Out_ otai_acl_resource_t& aclresource)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_acl_stage(j["stage"], aclresource.stage);
//    otai_deserialize_acl_bind_point(j["bind_point"], aclresource.bind_point);
//    otai_deserialize_number(j["avail_num"], aclresource.avail_num);
//}
//
//void otai_deserialize_acl_resource_list(
//        _In_ const std::string& s,
//        _Out_ otai_acl_resource_list_t& aclresource,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    aclresource.count = j["count"];
//
//    if (countOnly)
//    {
//        return;
//    }
//
//    if (j["list"] == nullptr)
//    {
//        aclresource.list = NULL;
//        return;
//    }
//
//    json arr = j["list"];
//
//    if (arr.size() != (size_t)aclresource.count)
//    {
//        SWSS_LOG_THROW("acl resource count mismatch %lu vs %u", arr.size(), aclresource.count);
//    }
//
//    aclresource.list = otai_alloc_n_of_ptr_type(aclresource.count, aclresource.list);
//
//    for (uint32_t i = 0; i < aclresource.count; ++i)
//    {
//        const json& item = arr[i];
//
//        otai_deserialize_acl_resource(item, aclresource.list[i]);
//    }
//}
//
//void otai_deserialize_ipv6(
//        _In_ const std::string& s,
//        _Out_ otai_ip6_t& ipaddr)
//{
//    SWSS_LOG_ENTER();
//
//    if (inet_pton(AF_INET6, s.c_str(), ipaddr) != 1)
//    {
//        SWSS_LOG_THROW("invalid ip address %s", s.c_str());
//    }
//}
//
//void otai_deserialize_ipv4(
//        _In_ const std::string& s,
//        _Out_ otai_ip4_t& ipaddr)
//{
//    SWSS_LOG_ENTER();
//
//    if (inet_pton(AF_INET, s.c_str(), &ipaddr) != 1)
//    {
//        SWSS_LOG_THROW("invalid ip address %s", s.c_str());
//    }
//}
//
void otai_deserialize_pointer(
        _In_ const std::string& s,
        _Out_ otai_pointer_t& ptr)
{
    SWSS_LOG_ENTER();

    otai_deserialize_number(s, (uintptr_t &)ptr, true);
}

//void otai_deserialize_ip_address(
//        _In_ const std::string& s,
//        _Out_ otai_ip_address_t& ipaddr)
//{
//    SWSS_LOG_ENTER();
//
//    if (inet_pton(AF_INET, s.c_str(), &ipaddr.addr.ip4) == 1)
//    {
//        ipaddr.addr_family = OTAI_IP_ADDR_FAMILY_IPV4;
//        return;
//    }
//
//    if (inet_pton(AF_INET6, s.c_str(), ipaddr.addr.ip6) == 1)
//    {
//        ipaddr.addr_family = OTAI_IP_ADDR_FAMILY_IPV6;
//        return;
//    }
//
//    SWSS_LOG_THROW("invalid ip address %s", s.c_str());
//}

//void otai_deserialize_ip_prefix(
//        _In_ const std::string &s,
//        _Out_ otai_ip_prefix_t &ip_prefix)
//{
//    SWSS_LOG_ENTER();
//
//    auto tokens = swss::tokenize(s, '/');
//
//    if (tokens.size() != 2)
//    {
//        SWSS_LOG_THROW("invalid ip prefix %s", s.c_str());
//    }
//
//    uint8_t mask;
//    otai_deserialize_number(tokens[1], mask);
//
//    const std::string &ip = tokens[0];
//
//    if (inet_pton(AF_INET, ip.c_str(), &ip_prefix.addr.ip4) == 1)
//    {
//        ip_prefix.addr_family = OTAI_IP_ADDR_FAMILY_IPV4;
//
//        otai_populate_ip_mask(mask, (uint8_t*)&ip_prefix.mask.ip4, false);
//    }
//    else if (inet_pton(AF_INET6, ip.c_str(), ip_prefix.addr.ip6) == 1)
//    {
//        ip_prefix.addr_family = OTAI_IP_ADDR_FAMILY_IPV6;
//
//        otai_populate_ip_mask(mask, ip_prefix.mask.ip6, true);
//    }
//    else
//    {
//        SWSS_LOG_THROW("invalid ip prefix %s", s.c_str());
//    }
//}

//void otai_deserialize_ip_address_list(
//        _In_ const std::string& s,
//        _Out_ otai_ip_address_list_t& list,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_list(s, list, countOnly, [&](const std::string sitem, otai_ip_address_t& item) { otai_deserialize_ip_address(sitem, item);} );
//}
//
//void otai_deserialize_ip_prefix_list(
//        _In_ const std::string& s,
//        _Out_ otai_ip_prefix_list_t& list,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_list(s, list, countOnly, [&](const std::string sitem, otai_ip_prefix_t& item) { otai_deserialize_ip_prefix(sitem, item);} );
//}
//
//void otai_deserialize_segment_list(
//        _In_ const std::string& s,
//        _Out_ otai_segment_list_t& list,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    if (countOnly)
//    {
//        otai_deserialize_number(s, list.count);
//        return;
//    }
//
//    auto pos = s.find(":");
//
//    if (pos == std::string::npos)
//    {
//        SWSS_LOG_THROW("invalid list %s", s.c_str());
//    }
//
//    std::string scount = s.substr(0, pos);
//
//    otai_deserialize_number(scount, list.count);
//
//    std::string slist = s.substr(pos + 1);
//
//    if (slist == "null")
//    {
//        list.list = NULL;
//        return;
//    }
//
//    auto tokens = swss::tokenize(slist, ',');
//
//    if (tokens.size() != list.count)
//    {
//        SWSS_LOG_THROW("invalid list count %lu != %u", tokens.size(), list.count);
//    }
//
//    list.list = otai_alloc_n_of_ptr_type(list.count, list.list);
//
//    for (uint32_t i = 0; i < list.count; ++i)
//    {
//        otai_deserialize_ipv6(tokens[i], list.list[i]);
//    }
//}

template <typename T>
void otai_deserialize_range(
        _In_ const std::string& s,
        _Out_ T& range)
{
    SWSS_LOG_ENTER();

    auto tokens = swss::tokenize(s, ',');

    if (tokens.size() != 2)
    {
        SWSS_LOG_THROW("invalid range %s", s.c_str());
    }

    otai_deserialize_number(tokens[0], range.min);
    otai_deserialize_number(tokens[1], range.max);
}

//void otai_deserialize_u16_range_list(
//        _In_ const std::string& s,
//        _Out_ otai_u16_range_list_t& list,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    if (countOnly)
//    {
//        otai_deserialize_number(s, list.count);
//        return;
//    }
//
//    auto pos = s.find(":");
//
//    if (pos == std::string::npos)
//    {
//        SWSS_LOG_THROW("invalid list %s", s.c_str());
//    }
//
//    std::string scount = s.substr(0, pos);
//
//    otai_deserialize_number(scount, list.count);
//
//    std::string slist = s.substr(pos + 1);
//
//    if (slist == "null")
//    {
//        list.list = NULL;
//        return;
//    }
//
//    auto tokens = swss::tokenize(slist, ',');
//
//    if (tokens.size() != list.count * 2)
//    {
//        SWSS_LOG_THROW("invalid u16_range_list count %lu != %u", tokens.size(), list.count * 2);
//    }
//
//    list.list = otai_alloc_n_of_ptr_type(list.count, list.list);
//
//    for (uint32_t i = 0; i < list.count * 2; i+=2)
//    {
//        std::ostringstream range;
//        range << tokens[i] << "," << tokens[i+1];
//
//        otai_deserialize_range(range.str(), list.list[i/2]);
//    }
//}

//void otai_deserialize_acl_field(
//        _In_ const std::string& s,
//        _In_ const otai_attr_metadata_t& meta,
//        _In_ otai_acl_field_data_t& field,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    if (s == "disabled")
//    {
//        field.enable = false;
//        return;
//    }
//
//    field.enable = true;
//
//    auto pos = s.find("&mask:");
//
//    std::string sfield;
//    std::string smask;
//
//    if (pos == std::string::npos)
//    {
//        sfield = s;
//    }
//    else
//    {
//        sfield = s.substr(0, pos);
//        smask = s.substr(pos + 6); // 6 = "&mask:" length
//    }
//
//    switch (meta.attrvaluetype)
//    {
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
//            return otai_deserialize_bool(sfield, field.data.booldata);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
//            otai_deserialize_number(sfield, field.data.u8);
//            otai_deserialize_number(smask, field.mask.u8, true);
//            return;
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
////            otai_deserialize_number(sfield, field.data.s8);
////            otai_deserialize_number(smask, field.mask.s8, true);
////            return;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
//            otai_deserialize_number(sfield, field.data.u16);
//            otai_deserialize_number(smask, field.mask.u16, true);
//            return;
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
////            otai_deserialize_number(sfield, field.data.s16);
////            otai_deserialize_number(smask, field.mask.s16, true);
////            return;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
//            otai_deserialize_number(sfield, field.data.u32);
//            otai_deserialize_number(smask, field.mask.u32, true);
//            return;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
//            otai_deserialize_enum(sfield, meta.enummetadata, field.data.s32);
//            otai_deserialize_number(smask, field.mask.s32, true);
//            return;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT64:
//            otai_deserialize_number(sfield, field.data.u64);
//            otai_deserialize_number(smask, field.mask.u64, true);
//            return;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
//            otai_deserialize_mac(sfield, field.data.mac);
//            otai_deserialize_mac(smask, field.mask.mac);
//            return;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
//            otai_deserialize_ipv4(sfield, field.data.ip4);
//            otai_deserialize_ipv4(smask, field.mask.ip4);
//            return;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
//            otai_deserialize_ipv6(sfield, field.data.ip6);
//            otai_deserialize_ipv6(smask, field.mask.ip6);
//            return;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
//            return otai_deserialize_object_id(sfield, field.data.oid);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
//            return otai_deserialize_oid_list(sfield, field.data.objlist, countOnly);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
//            otai_deserialize_number_list(sfield, field.data.u8list, countOnly);
//            otai_deserialize_number_list(smask, field.mask.u8list, countOnly, true);
//            return;
//
//        default:
//            SWSS_LOG_THROW("otai attr value %s is not implemented, FIXME", otai_serialize_attr_value_type(meta.attrvaluetype).c_str());
//    }
//}

//void otai_deserialize_acl_action(
//        _In_ const std::string& s,
//        _In_ const otai_attr_metadata_t& meta,
//        _In_ otai_acl_action_data_t& action,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    if (s == "disabled")
//    {
//        action.enable = false;
//        return;
//    }
//
//    action.enable = true;
//
//    switch (meta.attrvaluetype)
//    {
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
//            return otai_deserialize_bool(s, action.parameter.booldata);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
//            return otai_deserialize_number(s, action.parameter.u8);
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
////            return otai_deserialize_number(s, action.parameter.s8);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
//            return otai_deserialize_number(s, action.parameter.u16);
//
////        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
////            return otai_deserialize_number(s, action.parameter.s16);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
//            return otai_deserialize_number(s, action.parameter.u32);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
//            return otai_deserialize_enum(s, meta.enummetadata, action.parameter.s32);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
//            return otai_deserialize_mac(s, action.parameter.mac);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
//            return otai_deserialize_ipv4(s, action.parameter.ip4);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
//            return otai_deserialize_ipv6(s, action.parameter.ip6);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
//            return otai_deserialize_object_id(s, action.parameter.oid);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
//            return otai_deserialize_oid_list(s, action.parameter.objlist, countOnly);
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IP_ADDRESS:
//            otai_deserialize_ip_address(s, action.parameter.ipaddr);
//            return;
//
//        default:
//            SWSS_LOG_THROW("otai attr value %s is not implemented, FIXME", otai_serialize_attr_value_type(meta.attrvaluetype).c_str());
//    }
//}

//void otai_deserialize_acl_capability(
//        _In_ const std::string& s,
//        _Out_ otai_acl_capability_t& cap)
//{
//    SWSS_LOG_ENTER();
//
//    auto pos = s.find(":");
//
//    if (pos == std::string::npos)
//    {
//        SWSS_LOG_THROW("Invalid acl capability %s", s.c_str());
//    }
//
//    auto mandatory_on_create_str = s.substr(0, pos);
//    auto list = s.substr(pos + 1);
//
//    otai_deserialize_bool(mandatory_on_create_str, cap.is_action_list_mandatory);
//    otai_deserialize_enum_list(list, &otai_metadata_enum_otai_acl_action_type_t, cap.action_list, false);
//}

void otai_deserialize_hex_binary(
        _In_ const std::string &s,
        _Out_ void *buffer,
        _In_ size_t length)
{
    SWSS_LOG_ENTER();

    if (s.length() % 2 != 0)
    {
        SWSS_LOG_THROW("Invalid hex string %s", s.c_str());
    }

    if (s.length() > (length * 2))
    {
        SWSS_LOG_THROW("Buffer length isn't sufficient");
    }

    size_t buffer_cur = 0;
    size_t hex_cur = 0;
    unsigned char *output = static_cast<unsigned char *>(buffer);

    while (hex_cur < s.length())
    {
        const char temp_buffer[] = { s[hex_cur], s[hex_cur + 1], 0 };
        unsigned int value = -1;

        if (sscanf(temp_buffer, "%X", &value) <= 0 || value > 0xff)
        {
            SWSS_LOG_THROW("Invalid hex string %s", temp_buffer);
        }

        output[buffer_cur] = static_cast<unsigned char>(value);
        hex_cur += 2;
        buffer_cur += 1;
    }
}

template<typename T>
void otai_deserialize_hex_binary(
        _In_ const std::string &s,
        _Out_ T &value)
{
    SWSS_LOG_ENTER();

    return otai_deserialize_hex_binary(s, &value, sizeof(T));
}

//void otai_deserialize_system_port_config(
//        _In_ const std::string& s,
//        _Out_ otai_system_port_config_t& sysportconfig)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_number(j["port_id"], sysportconfig.port_id, false);
//    otai_deserialize_number(j["attached_switch_id"], sysportconfig.attached_switch_id, false);
//    otai_deserialize_number(j["attached_core_index"], sysportconfig.attached_core_index, false);
//    otai_deserialize_number(j["attached_core_port_index"], sysportconfig.attached_core_port_index, false);
//    otai_deserialize_number(j["speed"], sysportconfig.speed, false);
//    otai_deserialize_number(j["num_voq"], sysportconfig.num_voq, false);
//
//}
//
//void otai_deserialize_json(
//        _In_ const std::string& s,
//        _Out_ otai_json_t& js)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_number_list(j["json"], js.json, false);
//
//}

//void otai_deserialize_latch_status(
//        _In_ const std::string& s,
//        _Out_ otai_latch_status_t& latch_status)
//{
//    SWSS_LOG_ENTER();
//
//    auto pos = s.find(":");
//
//    if (pos == std::string::npos)
//    {
//        SWSS_LOG_THROW("Invalid latch status %s", s.c_str());
//    }
//
//    std::string changed = s.substr(0, pos);
//    std::string current_status = s.substr(pos + 1);
//
//    otai_deserialize_bool(changed, latch_status.changed);
//    otai_deserialize_bool(current_status, latch_status.current_status);
//
//}

//void otai_deserialize_port_lane_latch_status(
//        _In_ const json& j,
//        _Out_ otai_port_lane_latch_status_t& lane_latch_status)
//{
//    SWSS_LOG_ENTER();
//    otai_deserialize_latch_status(j["value"], lane_latch_status.value);
//}
//
//void otai_deserialize_port_lane_latch_status_list(
//        _In_ const std::string& s,
//        _Out_ otai_port_lane_latch_status_list_t& status_list,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    status_list.count = j["count"];
//
//    if (countOnly)
//    {
//        return;
//    }
//
//    if (j["list"] == nullptr)
//    {
//        status_list.list = NULL;
//        return;
//    }
//
//    json arr = j["list"];
//
//    if (arr.size() != (size_t)status_list.count)
//    {
//        SWSS_LOG_THROW("port lane latch status count mismatch %lu vs %u", arr.size(),status_list.count);
//    }
//
//    status_list.list = otai_alloc_n_of_ptr_type(status_list.count, status_list.list);
//
//    for (uint32_t i = 0; i < status_list.count; ++i)
//    {
//        const json &item = arr[i];
//
//        otai_deserialize_port_lane_latch_status(item, status_list.list[i]);
//    }
//}

//static void otai_deserialize_system_port_cfg_list_item(
//        _In_ const json& j,
//        _Out_ otai_system_port_config_t& sysportconfig)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_number(j["port_id"], sysportconfig.port_id, false);
//    otai_deserialize_number(j["attached_switch_id"], sysportconfig.attached_switch_id, false);
//    otai_deserialize_number(j["attached_core_index"], sysportconfig.attached_core_index, false);
//    otai_deserialize_number(j["attached_core_port_index"], sysportconfig.attached_core_port_index, false);
//    otai_deserialize_number(j["speed"], sysportconfig.speed, false);
//    otai_deserialize_number(j["num_voq"], sysportconfig.num_voq, false);
//}
//
//void otai_deserialize_system_port_config_list(
//        _In_ const std::string& s,
//        _Out_ otai_system_port_config_list_t& sysportconfiglist,
//        _In_ bool countOnly)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    sysportconfiglist.count = j["count"];
//
//    if (countOnly)
//    {
//        return;
//    }
//
//    if (j["list"] == nullptr)
//    {
//        sysportconfiglist.list = NULL;
//        return;
//    }
//
//    json arr = j["list"];
//
//    if (arr.size() != (size_t)sysportconfiglist.count)
//    {
//        SWSS_LOG_THROW("system port config list count mismatch %lu vs %u", arr.size(), sysportconfiglist.count);
//    }
//
//    sysportconfiglist.list = otai_alloc_n_of_ptr_type(sysportconfiglist.count, sysportconfiglist.list);
//
//    for (uint32_t i = 0; i < sysportconfiglist.count; ++i)
//    {
//        const json& item = arr[i];
//
//        otai_deserialize_system_port_cfg_list_item(item, sysportconfiglist.list[i]);
//    }
//}

void otai_deserialize_attr_value(
        _In_ const std::string& s,
        _In_ const otai_attr_metadata_t& meta,
        _Out_ otai_attribute_t &attr,
        _In_ const bool countOnly)
{
    SWSS_LOG_ENTER();

    memset(&attr.value, 0, sizeof(attr.value));

    switch (meta.attrvaluetype)
    {
        case OTAI_ATTR_VALUE_TYPE_BOOL:
            return otai_deserialize_bool(s, attr.value.booldata);

        case OTAI_ATTR_VALUE_TYPE_CHARDATA:
            return otai_deserialize_chardata(s, attr.value.chardata);

        case OTAI_ATTR_VALUE_TYPE_UINT8:
            return otai_deserialize_number(s, attr.value.u8);

        case OTAI_ATTR_VALUE_TYPE_INT8:
            return otai_deserialize_number(s, attr.value.s8);

        case OTAI_ATTR_VALUE_TYPE_UINT16:
            return otai_deserialize_number(s, attr.value.u16);

        //case OTAI_ATTR_VALUE_TYPE_JSON:
        //    return otai_deserialize_json(s, attr.value.json);

        case OTAI_ATTR_VALUE_TYPE_INT16:
            return otai_deserialize_number(s, attr.value.s16);

        case OTAI_ATTR_VALUE_TYPE_UINT32:
            return otai_deserialize_number(s, attr.value.u32);

        case OTAI_ATTR_VALUE_TYPE_INT32:
            return otai_deserialize_enum(s, meta.enummetadata, attr.value.s32);

        case OTAI_ATTR_VALUE_TYPE_UINT64:
            return otai_deserialize_number(s, attr.value.u64);

//        case OTAI_ATTR_VALUE_TYPE_INT64:
//            return otai_deserialize_number(s, attr.value.s64);

        //case OTAI_ATTR_VALUE_TYPE_MAC:
        //    return otai_deserialize_mac(s, attr.value.mac);

        //case OTAI_ATTR_VALUE_TYPE_IPV4:
        //    return otai_deserialize_ipv4(s, attr.value.ip4);

        //case OTAI_ATTR_VALUE_TYPE_IPV6:
        //    return otai_deserialize_ipv6(s, attr.value.ip6);

        case OTAI_ATTR_VALUE_TYPE_POINTER:
            return otai_deserialize_pointer(s, attr.value.ptr);

        //case OTAI_ATTR_VALUE_TYPE_IP_ADDRESS:
        //    return otai_deserialize_ip_address(s, attr.value.ipaddr);

        //case OTAI_ATTR_VALUE_TYPE_IP_PREFIX:
        //    return otai_deserialize_ip_prefix(s, attr.value.ipprefix);

        case OTAI_ATTR_VALUE_TYPE_OBJECT_ID:
            return otai_deserialize_object_id(s, attr.value.oid);

        case OTAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            return otai_deserialize_oid_list(s, attr.value.objlist, countOnly);

        case OTAI_ATTR_VALUE_TYPE_UINT8_LIST:
            return otai_deserialize_number_list(s, attr.value.u8list, countOnly);

        case OTAI_ATTR_VALUE_TYPE_INT8_LIST:
            return otai_deserialize_number_list(s, attr.value.s8list, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_LATCH_STATUS:
        //    return otai_deserialize_latch_status(s, attr.value.latchstatus);

        //case OTAI_ATTR_VALUE_TYPE_PORT_LANE_LATCH_STATUS_LIST:
        //    return otai_deserialize_port_lane_latch_status_list(s, attr.value.portlanelatchstatuslist, countOnly);

//        case OTAI_ATTR_VALUE_TYPE_UINT16_LIST:
//            return otai_deserialize_number_list(s, attr.value.u16list, countOnly);
//
//        case OTAI_ATTR_VALUE_TYPE_INT16_LIST:
//            return otai_deserialize_number_list(s, attr.value.s16list, countOnly);

        case OTAI_ATTR_VALUE_TYPE_UINT32_LIST:
            return otai_deserialize_number_list(s, attr.value.u32list, countOnly);

        case OTAI_ATTR_VALUE_TYPE_INT32_LIST:
            return otai_deserialize_enum_list(s, meta.enummetadata, attr.value.s32list, countOnly);

        case OTAI_ATTR_VALUE_TYPE_UINT32_RANGE:
            return otai_deserialize_range(s, attr.value.u32range);

//        case OTAI_ATTR_VALUE_TYPE_INT32_RANGE:
//            return otai_deserialize_range(s, attr.value.s32range);

        //case OTAI_ATTR_VALUE_TYPE_UINT16_RANGE_LIST:
        //    return otai_deserialize_u16_range_list(s, attr.value.u16rangelist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_VLAN_LIST:
        //    return otai_deserialize_number_list(s, attr.value.vlanlist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
        //    return otai_deserialize_qos_map_list(s, attr.value.qosmap, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_MAP_LIST:
        //    return otai_deserialize_map_list(s, attr.value.maplist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_ACL_RESOURCE_LIST:
        //    return otai_deserialize_acl_resource_list(s, attr.value.aclresource, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
        //    return otai_deserialize_ip_address_list(s, attr.value.ipaddrlist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_SEGMENT_LIST:
        //    return otai_deserialize_segment_list(s, attr.value.segmentlist, countOnly);

        //    // ACL FIELD DATA

        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT64:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
        //case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
        //    return otai_deserialize_acl_field(s, meta, attr.value.aclfield, countOnly);

        //    // ACL ACTION DATA

        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
        //case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IP_ADDRESS:
        //    return otai_deserialize_acl_action(s, meta, attr.value.aclaction, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_ACL_CAPABILITY:
        //    return otai_deserialize_acl_capability(s, attr.value.aclcapability);

        //case OTAI_ATTR_VALUE_TYPE_AUTH_KEY:
        //    return otai_deserialize_hex_binary(s, attr.value.authkey);

        //case OTAI_ATTR_VALUE_TYPE_ENCRYPT_KEY:
        //    return otai_deserialize_hex_binary(s, attr.value.encrypt_key);

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SAK:
        //    return otai_deserialize_hex_binary(s, attr.value.macsecsak);

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_AUTH_KEY:
        //    return otai_deserialize_hex_binary(s, attr.value.macsecauthkey);

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SALT:
        //    return otai_deserialize_hex_binary(s, attr.value.macsecsalt);

        //case OTAI_ATTR_VALUE_TYPE_SYSTEM_PORT_CONFIG:
        //    return otai_deserialize_system_port_config(s, attr.value.sysportconfig);

        //case OTAI_ATTR_VALUE_TYPE_SYSTEM_PORT_CONFIG_LIST:
        //    return otai_deserialize_system_port_config_list(s, attr.value.sysportconfiglist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_IP_PREFIX_LIST:
        //    return otai_deserialize_ip_prefix_list(s, attr.value.ipprefixlist, countOnly);

        //case OTAI_ATTR_VALUE_TYPE_POE_PORT_POWER_CONSUMPTION:
        //    return otai_deserialize_poe_port_power_consumption(s, attr.value.portpowerconsumption);

        default:
            SWSS_LOG_THROW("deserialize type %s is not supported yet FIXME",
                    otai_serialize_attr_value_type(meta.attrvaluetype).c_str());
    }
}

void otai_deserialize_status(
        _In_ const std::string& s,
        _Out_ otai_status_t& status)
{
    SWSS_LOG_ENTER();

    otai_deserialize_enum(s, &otai_metadata_enum_otai_status_t, status);
}

//void otai_deserialize_port_oper_status(
//        _In_ const std::string& s,
//        _Out_ otai_port_oper_status_t& status)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_port_oper_status_t, (int32_t&)status);
//}

//void otai_deserialize_port_error_status(
//        _In_ const std::string& s,
//        _Out_ otai_port_error_status_t& status)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_port_error_status_t, (int32_t&)status);
//}

//void otai_deserialize_port_host_tx_ready_status(
//        _In_ const std::string& s,
//        _Out_ otai_port_host_tx_ready_status_t& status)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_port_host_tx_ready_status_t, (int32_t&)status);
//}
//
//void otai_deserialize_queue_deadlock(
//        _In_ const std::string& s,
//        _Out_ otai_queue_pfc_deadlock_event_type_t& event)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_queue_pfc_deadlock_event_type_t, (int32_t&)event);
//}

//void otai_deserialize_ipmc_entry_type(
//        _In_ const std::string& s,
//        _Out_ otai_ipmc_entry_type_t& type)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_deserialize_enum(s, &otai_metadata_enum_otai_ipmc_entry_type_t, (int32_t&)type);
//}
//
//void otai_deserialize_l2mc_entry_type(
//        _In_ const std::string& s,
//        _Out_ otai_l2mc_entry_type_t& type)
//{
//    SWSS_LOG_ENTER();
//
//    return otai_deserialize_enum(s, &otai_metadata_enum_otai_l2mc_entry_type_t, (int32_t&)type);
//}
//
//void otai_deserialize_fdb_event(
//        _In_ const std::string& s,
//        _Out_ otai_fdb_event_t& event)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_fdb_event_t, (int32_t&)event);
//}
//
//void otai_deserialize_nat_event(
//        _In_ const std::string& s,
//        _Out_ otai_nat_event_t& event)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_nat_event_t, (int32_t&)event);
//}
//
//void otai_deserialize_bfd_session_state(
//        _In_ const std::string& s,
//        _Out_ otai_bfd_session_state_t& state)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_bfd_session_state_t, (int32_t&)state);
//}
//
//void otai_deserialize_twamp_session_state(
//        _In_ const std::string& s,
//        _Out_ otai_twamp_session_state_t& state)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_twamp_session_state_t, (int32_t&)state);
//}
//
//void otai_deserialize_twamp_session_stat(
//        _In_ const std::string& s,
//        _Out_ otai_twamp_session_stat_t& stat)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_twamp_session_stat_t, (int32_t&)stat);
//}
//
//void otai_deserialize_switch_oper_status(
//        _In_ const std::string& s,
//        _Out_ otai_object_id_t &switch_id,
//        _Out_ otai_switch_oper_status_t& status)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], switch_id);
//    otai_deserialize_enum(j["status"], &otai_metadata_enum_otai_switch_oper_status_t, (int32_t&)status);
//}

//void otai_deserialize_timespec(
//        _In_ const std::string& s,
//        _Out_ otai_timespec_t &timestamp)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//    try
//    {
//        j = json::parse(s);
//    }
//    catch (const std::exception&)
//    {
//        SWSS_LOG_THROW("Received an exception after trying to parse timespec_t from %s", s.c_str());
//    }
//
//    otai_deserialize_number<uint64_t>(j["tv_sec"], timestamp.tv_sec);
//    otai_deserialize_number<uint32_t>(j["tv_nsec"], timestamp.tv_nsec);
//}

//void otai_deserialize_switch_asic_sdk_health_event(
//        _In_ const std::string& s,
//        _Out_ otai_object_id_t &switch_id,
//        _Out_ otai_switch_asic_sdk_health_severity_t &severity,
//        _Out_ otai_timespec_t &timestamp,
//        _Out_ otai_switch_asic_sdk_health_category_t &category,
//        _Out_ otai_switch_health_data_t &data,
//        _Out_ otai_u8_list_t &description)
//{
//    SWSS_LOG_ENTER();
//
//    json j;
//    try
//    {
//        j = json::parse(s);
//    }
//    catch (const std::exception&)
//    {
//        SWSS_LOG_THROW("Received an exception after trying to parse switch_asic_sdk_health_event from %s", s.c_str());
//    }
//
//    otai_deserialize_object_id(j["switch_id"], switch_id);
//    otai_deserialize_enum(j["severity"], &otai_metadata_enum_otai_switch_asic_sdk_health_severity_t, (int32_t&)severity);
//    otai_deserialize_timespec(j["timestamp"], timestamp);
//    otai_deserialize_enum(j["category"], &otai_metadata_enum_otai_switch_asic_sdk_health_category_t, (int32_t&)category);
//    int32_t data_type;
//    otai_deserialize_enum(j["data.data_type"], &otai_metadata_enum_otai_health_data_type_t, data_type);
//    data.data_type = (otai_health_data_type_t)data_type;
//    data.data_type = OTAI_HEALTH_DATA_TYPE_GENERAL;
//    otai_deserialize_number_list(j["description"], description, false, false);
//}
//
//void otai_deserialize_free_switch_asic_sdk_health_event(
//        _In_ otai_u8_list_t &description)
//{
//    SWSS_LOG_ENTER();
//
//    otai_free_list(description);
//}
//
//void otai_deserialize_switch_shutdown_request(
//        _In_ const std::string& s,
//        _Out_ otai_object_id_t &switch_id)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], switch_id);
//}

void otai_deserialize_object_type(
        _In_ const std::string& s,
        _Out_ otai_object_type_t& object_type)
{
    SWSS_LOG_ENTER();

    otai_deserialize_enum(s, &otai_metadata_enum_otai_object_type_t, (int32_t&)object_type);
}

void otai_deserialize_log_level(
        _In_ const std::string& s,
        _Out_ otai_log_level_t& log_level)
{
    SWSS_LOG_ENTER();

    otai_deserialize_enum(s, &otai_metadata_enum_otai_log_level_t, (int32_t&)log_level);
}

void otai_deserialize_api(
        _In_ const std::string& s,
        _Out_ otai_api_t& api)
{
    SWSS_LOG_ENTER();

    otai_deserialize_enum(s, &otai_metadata_enum_otai_api_t, (int32_t&)api);
}

//void otai_deserialize_vlan_id(
//        _In_ const std::string& s,
//        _In_ otai_vlan_id_t& vlan_id)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_number(s, vlan_id);
//}

//void otai_deserialize_fdb_entry(
//        _In_ const std::string &s,
//        _Out_ otai_fdb_entry_t &fdb_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], fdb_entry.switch_id);
//    otai_deserialize_mac(j["mac"], fdb_entry.mac_address);
//    otai_deserialize_object_id(j["bvid"], fdb_entry.bv_id);
//}
//
//void otai_deserialize_neighbor_entry(
//        _In_ const std::string& s,
//        _In_ otai_neighbor_entry_t &ne)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], ne.switch_id);
//    otai_deserialize_object_id(j["rif"], ne.rif_id);
//    otai_deserialize_ip_address(j["ip"], ne.ip_address);
//}
//
//void otai_deserialize_meter_bucket_entry(
//        _In_ const std::string& s,
//        _Out_ otai_meter_bucket_entry_t& meter_bucket_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], meter_bucket_entry.switch_id);
//    otai_deserialize_object_id(j["eni_id"], meter_bucket_entry.eni_id);
//    otai_deserialize_number(j["meter_class"], meter_bucket_entry.meter_class);
//}
//
//void otai_deserialize_flow_entry(
//        _In_ const std::string& s,
//        _Out_ otai_flow_entry_t &flow_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], flow_entry.switch_id);
//    otai_deserialize_mac(j["eni_mac"], flow_entry.eni_mac);
//    otai_deserialize_number(j["vnet_id"], flow_entry.vnet_id);
//    otai_deserialize_number(j["ip_proto"], flow_entry.ip_proto);
//    otai_deserialize_ip_address(j["src_ip"], flow_entry.src_ip);
//    otai_deserialize_ip_address(j["dst_ip"], flow_entry.dst_ip);
//    otai_deserialize_number(j["src_port"], flow_entry.src_port);
//    otai_deserialize_number(j["dst_port"], flow_entry.dst_port);
//}
//
//void otai_deserialize_twamp_session_stats_data(
//        _In_ const std::string& s,
//        _Out_ otai_twamp_session_stats_data_t &twamp_session_stats_data)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_number(j["index"], twamp_session_stats_data.index);
//
//    json arr = j["list"];
//
//    twamp_session_stats_data.number_of_counters = (uint32_t)arr.size();
//    twamp_session_stats_data.counters_ids = new otai_twamp_session_stat_t[twamp_session_stats_data.number_of_counters];
//    twamp_session_stats_data.counters = new uint64_t[twamp_session_stats_data.number_of_counters];
//
//    for (uint32_t i = 0; i < twamp_session_stats_data.number_of_counters; ++i)
//    {
//        const json &item = arr[i];
//
//        otai_deserialize_twamp_session_stat(item["counters_ids"], twamp_session_stats_data.counters_ids[i]);
//
//        otai_deserialize_number(item["counters"], twamp_session_stats_data.counters[i]);
//    }
//}
//
//void otai_deserialize_json_twamp_session_event_notification_data(
//        _In_ const json& j,
//        _Out_ otai_twamp_session_event_notification_data_t& twamp_session_data)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_object_id(j["twamp_session_id"], twamp_session_data.twamp_session_id);
//    otai_deserialize_twamp_session_state(j["session_state"], twamp_session_data.session_state);
//
//    otai_deserialize_number(j["index"], twamp_session_data.session_stats.index);
//
//    json arr = j["list"];
//
//    twamp_session_data.session_stats.number_of_counters = (uint32_t)arr.size();
//    twamp_session_data.session_stats.counters_ids = new otai_twamp_session_stat_t[twamp_session_data.session_stats.number_of_counters];
//    twamp_session_data.session_stats.counters = new uint64_t[twamp_session_data.session_stats.number_of_counters];
//
//    for (uint32_t i = 0; i < twamp_session_data.session_stats.number_of_counters; ++i)
//    {
//        const json &item = arr[i];
//
//        otai_deserialize_twamp_session_stat(item["counters_ids"], twamp_session_data.session_stats.counters_ids[i]);
//
//        otai_deserialize_number(item["counters"], twamp_session_data.session_stats.counters[i]);
//    }
//
//}

#define EXPECT(x) { \
    if (strncmp(buf, x, sizeof(x) - 1) == 0) { buf += sizeof(x) - 1; }  \
    else { \
        SWSS_LOG_THROW("failed to expect %s on %s", x, buf); } }
#define EXPECT_QUOTE     EXPECT("\"")
#define EXPECT_KEY(k)    EXPECT("\"" k "\":")
#define EXPECT_NEXT_KEY(k) { EXPECT(","); EXPECT_KEY(k); }
#define EXPECT_CHECK(expr, suffix) {                                    \
    ret = (expr);                                                       \
    if (ret < 0) {                                                      \
        SWSS_LOG_THROW("failed to deserialize " #suffix ""); }          \
    buf += ret; }
#define EXPECT_QUOTE_CHECK(expr, suffix) {\
    EXPECT_QUOTE; EXPECT_CHECK(expr, suffix); EXPECT_QUOTE; }

//static int otai_deserialize_object_id_buf(
//        _In_ const char* buf,
//        _Out_ otai_object_id_t* oid)
//{
//    SWSS_LOG_ENTER();
//
//    if (strncmp(buf, "oid:0x", 6) != 0)
//    {
//        SWSS_LOG_THROW("invalid oid %s", buf);
//    }
//
//    errno = 0;
//
//    char *endptr = NULL;
//
//    *oid = (otai_object_id_t)strtoull(buf+4, &endptr, 16);
//
//    if (errno != 0 || !otai_serialize_is_char_allowed(*endptr))
//    {
//        SWSS_LOG_THROW("invalid oid %s", buf);
//    }
//
//    return (int)(endptr - buf);
//}

//void otai_deserialize_route_entry(
//        _In_ const std::string &s,
//        _Out_ otai_route_entry_t& route_entry)
//{
//    SWSS_LOG_ENTER();
//
//    // NOTE: this serialize is copy from OTAI/meta auto generated serialization
//    // but since previously we used json.hpp, then order of serialized item is
//    // different, so we copy actual serialize method and reorder names
//
//    // {"dest":"0.0.0.0/0","switch_id":"oid:0x21000000000000","vr":"oid:0x3000000000022"}
//
//    const char* buf = s.c_str();
//
//    int ret;
//
//    EXPECT("{");
//
//    EXPECT_KEY("dest");
//
//    EXPECT_QUOTE_CHECK(otai_deserialize_ip_prefix(buf, &route_entry.destination), ip_prefix);
//
//    EXPECT_NEXT_KEY("switch_id");
//
//    EXPECT_QUOTE_CHECK(otai_deserialize_object_id_buf(buf, &route_entry.switch_id), object_id);
//
//    EXPECT_NEXT_KEY("vr");
//
//    EXPECT_QUOTE_CHECK(otai_deserialize_object_id_buf(buf, &route_entry.vr_id), object_id);
//
//    EXPECT("}");
//}

//void otai_deserialize_inseg_entry(
//        _In_ const std::string &s,
//        _Out_ otai_inseg_entry_t& inseg_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], inseg_entry.switch_id);
//    otai_deserialize_number(j["label"], inseg_entry.label);
//}
//
//void otai_deserialize_my_sid_entry(
//        _In_ const std::string& s,
//        _Out_ otai_my_sid_entry_t &ne)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], ne.switch_id);
//    otai_deserialize_object_id(j["vr_id"], ne.vr_id);
//    otai_deserialize_number(j["locator_block_len"], ne.locator_block_len);
//    otai_deserialize_number(j["locator_node_len"], ne.locator_node_len);
//    otai_deserialize_number(j["function_len"], ne.function_len);
//    otai_deserialize_number(j["args_len"], ne.args_len);
//    otai_deserialize_ipv6(j["sid"], ne.sid);
//}
//
//static void otai_deserialize_nat_entry_key(
//        _In_ const json& j,
//        _Out_ otai_nat_entry_key_t& nat_entry_key)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_ipv4(j["src_ip"], nat_entry_key.src_ip);
//    otai_deserialize_ipv4(j["dst_ip"], nat_entry_key.dst_ip);
//    otai_deserialize_number(j["proto"], nat_entry_key.proto);
//    otai_deserialize_number(j["l4_src_port"], nat_entry_key.l4_src_port);
//    otai_deserialize_number(j["l4_dst_port"], nat_entry_key.l4_dst_port);
//}
//
//static void otai_deserialize_nat_entry_mask(
//        _In_ const json& j,
//        _Out_ otai_nat_entry_mask_t& nat_entry_mask)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_ipv4(j["src_ip"], nat_entry_mask.src_ip);
//    otai_deserialize_ipv4(j["dst_ip"], nat_entry_mask.dst_ip);
//    otai_deserialize_number(j["proto"], nat_entry_mask.proto);
//    otai_deserialize_number(j["l4_src_port"], nat_entry_mask.l4_src_port);
//    otai_deserialize_number(j["l4_dst_port"], nat_entry_mask.l4_dst_port);
//}
//
//static void otai_deserialize_nat_entry_data(
//        _In_ const json& j,
//        _Out_ otai_nat_entry_data_t& nat_entry_data)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_nat_entry_key(j["key"], nat_entry_data.key);
//    otai_deserialize_nat_entry_mask(j["mask"], nat_entry_data.mask);
//}
//
//void otai_deserialize_nat_entry_type(
//        _In_ const std::string& s,
//        _Out_ otai_nat_type_t& type)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_nat_type_t, (int32_t&)type);
//}
//
//void otai_deserialize_nat_entry(
//        _In_ const std::string &s,
//        _Out_ otai_nat_entry_t& nat_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], nat_entry.switch_id);
//    otai_deserialize_object_id(j["vr"], nat_entry.vr_id);
//    otai_deserialize_nat_entry_type(j["nat_type"], nat_entry.nat_type);
//    otai_deserialize_nat_entry_data(j["nat_data"], nat_entry.data);
//}
//
//void otai_deserialize_ipmc_entry(
//        _In_ const std::string &s,
//        _Out_ otai_ipmc_entry_t& ipmc_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], ipmc_entry.switch_id);
//    otai_deserialize_object_id(j["vr_id"], ipmc_entry.vr_id);
//    otai_deserialize_ipmc_entry_type(j["type"], ipmc_entry.type);
//    otai_deserialize_ip_address(j["destination"], ipmc_entry.destination);
//    otai_deserialize_ip_address(j["source"], ipmc_entry.source);
//}
//
//void otai_deserialize_l2mc_entry(
//        _In_ const std::string &s,
//        _Out_ otai_l2mc_entry_t& l2mc_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], l2mc_entry.switch_id);
//    otai_deserialize_object_id(j["bv_id"], l2mc_entry.bv_id);
//    otai_deserialize_l2mc_entry_type(j["type"], l2mc_entry.type);
//    otai_deserialize_ip_address(j["destination"], l2mc_entry.destination);
//    otai_deserialize_ip_address(j["source"], l2mc_entry.source);
//}
//
//void otai_deserialize_mcast_fdb_entry(
//        _In_ const std::string &s,
//        _Out_ otai_mcast_fdb_entry_t& mcast_fdb_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], mcast_fdb_entry.switch_id);
//    otai_deserialize_object_id(j["bv_id"], mcast_fdb_entry.bv_id);
//    otai_deserialize_mac(j["mac_address"], mcast_fdb_entry.mac_address);
//}
//
//void otai_deserialize_direction_lookup_entry(
//        _In_ const std::string &s,
//        _Out_ otai_direction_lookup_entry_t& direction_lookup_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], direction_lookup_entry.switch_id);
//    otai_deserialize_number(j["vni"], direction_lookup_entry.vni);
//}
//
//void otai_deserialize_eni_ether_address_map_entry(
//        _In_ const std::string &s,
//        _Out_ otai_eni_ether_address_map_entry_t& eni_ether_address_map_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], eni_ether_address_map_entry.switch_id);
//    otai_deserialize_mac(j["address"], eni_ether_address_map_entry.address);
//}
//
//void otai_deserialize_vip_entry(
//        _In_ const std::string &s,
//        _Out_ otai_vip_entry_t& vip_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], vip_entry.switch_id);
//    otai_deserialize_ip_address(j["vip"], vip_entry.vip);
//}
//
//void otai_deserialize_inbound_routing_entry(
//        _In_ const std::string &s,
//        _Out_ otai_inbound_routing_entry_t& inbound_routing_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], inbound_routing_entry.switch_id);
//    otai_deserialize_object_id(j["eni_id"], inbound_routing_entry.eni_id);
//    otai_deserialize_number(j["vni"], inbound_routing_entry.vni);
//    otai_deserialize_ip_address(j["sip"], inbound_routing_entry.sip);
//    otai_deserialize_ip_address(j["sip_mask"], inbound_routing_entry.sip_mask);
//    otai_deserialize_number(j["priority"], inbound_routing_entry.priority);
//}
//
//void otai_deserialize_pa_validation_entry(
//        _In_ const std::string &s,
//        _Out_ otai_pa_validation_entry_t& pa_validation_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], pa_validation_entry.switch_id);
//    otai_deserialize_object_id(j["vnet_id"], pa_validation_entry.vnet_id);
//    otai_deserialize_ip_address(j["sip"], pa_validation_entry.sip);
//}
//
//void otai_deserialize_outbound_routing_entry(
//        _In_ const std::string &s,
//        _Out_ otai_outbound_routing_entry_t& outbound_routing_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], outbound_routing_entry.switch_id);
//    otai_deserialize_ip_prefix(j["destination"], outbound_routing_entry.destination);
//    otai_deserialize_object_id(j["outbound_routing_group_id"], outbound_routing_entry.outbound_routing_group_id);
//}
//
//void otai_deserialize_outbound_ca_to_pa_entry(
//        _In_ const std::string &s,
//        _Out_ otai_outbound_ca_to_pa_entry_t& outbound_ca_to_pa_entry)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j["switch_id"], outbound_ca_to_pa_entry.switch_id);
//    otai_deserialize_object_id(j["dst_vnet_id"], outbound_ca_to_pa_entry.dst_vnet_id);
//    otai_deserialize_ip_address(j["dip"], outbound_ca_to_pa_entry.dip);
//}

void otai_deserialize_attr_id(
        _In_ const std::string& s,
        _Out_ const otai_attr_metadata_t** meta)
{
    SWSS_LOG_ENTER();

    if (meta == NULL)
    {
        SWSS_LOG_THROW("meta pointer is null");
    }

    auto m = otai_metadata_get_attr_metadata_by_attr_id_name(s.c_str());

    if (m == NULL)
    {
        // check ignored attributes names for backward compatibility
        // m = otai_metadata_get_ignored_attr_metadata_by_attr_id_name(s.c_str());
    }

    if (m == NULL)
    {
        SWSS_LOG_THROW("invalid attr id: %s", s.c_str());
    }

    *meta = m;
}

void otai_deserialize_attr_id(
        _In_ const std::string& s,
        _Out_ otai_attr_id_t& attrid)
{
    SWSS_LOG_ENTER();

    const otai_attr_metadata_t *meta = NULL;

    otai_deserialize_attr_id(s, &meta);

    attrid = meta->attrid;
}

bool otai_deserialize_object_entry(
    _In_ const std::string object_id,
    _Inout_ otai_object_meta_key_t& meta_key)
{
    SWSS_LOG_ENTER();

    switch (meta_key.objecttype)
    {
        //case OTAI_OBJECT_TYPE_FDB_ENTRY:
        //    otai_deserialize_fdb_entry(object_id, meta_key.objectkey.key.fdb_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_ROUTE_ENTRY:
        //    otai_deserialize_route_entry(object_id, meta_key.objectkey.key.route_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
        //    otai_deserialize_neighbor_entry(object_id, meta_key.objectkey.key.neighbor_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_NAT_ENTRY:
        //    otai_deserialize_nat_entry(object_id, meta_key.objectkey.key.nat_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_INSEG_ENTRY:
        //    otai_deserialize_inseg_entry(object_id, meta_key.objectkey.key.inseg_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_MY_SID_ENTRY:
        //    otai_deserialize_my_sid_entry(object_id, meta_key.objectkey.key.my_sid_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_L2MC_ENTRY:
        //    otai_deserialize_l2mc_entry(object_id, meta_key.objectkey.key.l2mc_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_IPMC_ENTRY:
        //    otai_deserialize_ipmc_entry(object_id, meta_key.objectkey.key.ipmc_entry);
        //    return true;

        //case OTAI_OBJECT_TYPE_MCAST_FDB_ENTRY:
        //    otai_deserialize_mcast_fdb_entry(object_id, meta_key.objectkey.key.mcast_fdb_entry);
        //    return true;

        default:
            return false;
    }
}

//bool otai_deserialize_object_extension_entry(
//    _In_ const std::string object_id,
//    _Inout_ otai_object_meta_key_t& meta_key)
//{
//    SWSS_LOG_ENTER();
//
//    switch ((otai_object_type_extensions_t)meta_key.objecttype)
//    {
//        case OTAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
//            otai_deserialize_direction_lookup_entry(object_id, meta_key.objectkey.key.direction_lookup_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
//            otai_deserialize_eni_ether_address_map_entry(object_id, meta_key.objectkey.key.eni_ether_address_map_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_VIP_ENTRY:
//            otai_deserialize_vip_entry(object_id, meta_key.objectkey.key.vip_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
//            otai_deserialize_inbound_routing_entry(object_id, meta_key.objectkey.key.inbound_routing_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
//            otai_deserialize_pa_validation_entry(object_id, meta_key.objectkey.key.pa_validation_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
//            otai_deserialize_outbound_routing_entry(object_id, meta_key.objectkey.key.outbound_routing_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
//            otai_deserialize_outbound_ca_to_pa_entry(object_id, meta_key.objectkey.key.outbound_ca_to_pa_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_FLOW_ENTRY:
//            otai_deserialize_flow_entry(object_id, meta_key.objectkey.key.flow_entry);
//            return true;
//
//        case OTAI_OBJECT_TYPE_METER_BUCKET_ENTRY:
//            otai_deserialize_meter_bucket_entry(object_id, meta_key.objectkey.key.meter_bucket_entry);
//            return true;
//
//        default:
//            return false;
//    }
//}

void otai_deserialize_object_meta_key(
        _In_ const std::string &s,
        _Out_ otai_object_meta_key_t& meta_key)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("%s", s.c_str());

    const std::string &str_object_type = s.substr(0, s.find(":"));
    const std::string &str_object_id = s.substr(s.find(":") + 1);

    otai_deserialize_object_type(str_object_type, meta_key.objecttype);

    if (!otai_metadata_is_object_type_valid(meta_key.objecttype))
    {
        SWSS_LOG_THROW("invalid object type value %s", otai_serialize_object_type(meta_key.objecttype).c_str());
    }

    if (!otai_deserialize_object_entry(str_object_id, meta_key))// &&
        //!otai_deserialize_object_extension_entry(str_object_id, meta_key))
    {
        const auto& meta = otai_metadata_get_object_type_info(meta_key.objecttype);

        if (meta->isnonobjectid)
        {
            SWSS_LOG_THROW("object %s is non object id, not supported yet, FIXME",
                    otai_serialize_object_type(meta->objecttype).c_str());
        }

        otai_deserialize_object_id(str_object_id, meta_key.objectkey.key.object_id);
    }
}

// deserialize notifications

//static void otai_deserialize_json_fdb_event_notification_data(
//        _In_ const json& j,
//        _Out_ otai_fdb_event_notification_data_t& fdb)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_fdb_event(j["fdb_event"], fdb.event_type);
//    otai_deserialize_fdb_entry(j["fdb_entry"], fdb.fdb_entry);
//
//    json arr = j["list"];
//
//    fdb.attr_count = (uint32_t)arr.size();
//    fdb.attr = new otai_attribute_t[fdb.attr_count];
//
//    for (uint32_t i = 0; i < fdb.attr_count; ++i)
//    {
//        const json &item = arr[i];
//
//        const otai_attr_metadata_t *meta = NULL;
//
//        otai_deserialize_attr_id(item["id"], &meta);
//
//        fdb.attr[i].id = meta->attrid;
//
//        otai_deserialize_attr_value(item["value"], *meta, fdb.attr[i]);
//    }
//}
//
//void otai_deserialize_fdb_event_ntf(
//        _In_ const std::string& s,
//        _Out_ uint32_t &count,
//        _Out_ otai_fdb_event_notification_data_t** fdb_event)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    count = (uint32_t)j.size();
//
//    auto data = new otai_fdb_event_notification_data_t[count];
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        otai_deserialize_json_fdb_event_notification_data(j[i], data[i]);
//    }
//
//    *fdb_event = data;
//}

//static void otai_deserialize_json_nat_event_notification_data(
//        _In_ const json& j,
//        _Out_ otai_nat_event_notification_data_t& nat)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_nat_event(j["nat_event"], nat.event_type);
//    otai_deserialize_nat_entry(j["nat_entry"], nat.nat_entry);
//}

//void otai_deserialize_nat_event_ntf(
//        _In_ const std::string& s,
//        _Out_ uint32_t &count,
//        _Out_ otai_nat_event_notification_data_t** nat_event)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    count = (uint32_t)j.size();
//
//    auto data = new otai_nat_event_notification_data_t[count];
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        otai_deserialize_json_nat_event_notification_data(j[i], data[i]);
//    }
//
//    *nat_event = data;
//}
//
//void otai_deserialize_port_oper_status_ntf(
//        _In_ const std::string& s,
//        _Out_ uint32_t &count,
//        _Out_ otai_port_oper_status_notification_t** port_oper_status)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    count = (uint32_t)j.size();
//
//    auto data = new otai_port_oper_status_notification_t[count];
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        otai_deserialize_object_id(j[i]["port_id"], data[i].port_id);
//        otai_deserialize_port_oper_status(j[i]["port_state"], data[i].port_state);
//        otai_deserialize_port_error_status(j[i]["port_error_status"], data[i].port_error_status);
//    }
//
//    *port_oper_status = data;
//}
//
//void otai_deserialize_port_host_tx_ready_ntf(
//        _In_ const std::string& s,
//        _Out_ otai_object_id_t& switch_id,
//        _Out_ otai_object_id_t& port_id,
//        _Out_ otai_port_host_tx_ready_status_t& host_tx_ready_status)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_object_id(j[0]["port_id"], port_id);
//    otai_deserialize_object_id(j[0]["switch_id"], switch_id);
//    otai_deserialize_port_host_tx_ready_status(j[0]["host_tx_ready_status"], host_tx_ready_status);
//}
//
//void otai_deserialize_queue_deadlock_ntf(
//        _In_ const std::string& s,
//        _Out_ uint32_t &count,
//        _Out_ otai_queue_deadlock_notification_data_t** deadlock_data)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    count = (uint32_t)j.size();
//
//    auto data = new otai_queue_deadlock_notification_data_t[count];
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        otai_deserialize_object_id(j[i]["queue_id"], data[i].queue_id);
//        otai_deserialize_queue_deadlock(j[i]["event"], data[i].event);
//    }
//
//    *deadlock_data = data;
//}
//
//void otai_deserialize_bfd_session_state_ntf(
//        _In_ const std::string& s,
//        _Out_ uint32_t &count,
//        _Out_ otai_bfd_session_state_notification_t** bfd_session_state)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    count = (uint32_t)j.size();
//
//    auto data = new otai_bfd_session_state_notification_t[count];
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        otai_deserialize_object_id(j[i]["bfd_session_id"], data[i].bfd_session_id);
//        otai_deserialize_bfd_session_state(j[i]["session_state"], data[i].session_state);
//    }
//
//    *bfd_session_state = data;
//}
//
//void otai_deserialize_twamp_session_event_ntf(
//        _In_ const std::string& s,
//        _Out_ uint32_t &count,
//        _Out_ otai_twamp_session_event_notification_data_t** twamp_session_event)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    count = (uint32_t)j.size();
//
//    auto data = new otai_twamp_session_event_notification_data_t[count];
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        otai_deserialize_json_twamp_session_event_notification_data(j[i], data[i]);
//    }
//
//    *twamp_session_event = data;
//}

// deserialize free

void otai_deserialize_free_attribute_value(
        _In_ const otai_attr_value_type_t type,
        _In_ otai_attribute_t &attr)
{
    SWSS_LOG_ENTER();

    // if we allocated list, then we need to free it

    switch (type)
    {
        case OTAI_ATTR_VALUE_TYPE_BOOL:
        case OTAI_ATTR_VALUE_TYPE_CHARDATA:
        case OTAI_ATTR_VALUE_TYPE_UINT8:
        case OTAI_ATTR_VALUE_TYPE_INT8:
        case OTAI_ATTR_VALUE_TYPE_UINT16:
        case OTAI_ATTR_VALUE_TYPE_INT16:
        case OTAI_ATTR_VALUE_TYPE_UINT32:
        case OTAI_ATTR_VALUE_TYPE_INT32:
        case OTAI_ATTR_VALUE_TYPE_UINT64:
        case OTAI_ATTR_VALUE_TYPE_INT64:
        //case OTAI_ATTR_VALUE_TYPE_MAC:
        //case OTAI_ATTR_VALUE_TYPE_IPV4:
        //case OTAI_ATTR_VALUE_TYPE_IPV6:
        case OTAI_ATTR_VALUE_TYPE_POINTER:
        //case OTAI_ATTR_VALUE_TYPE_IP_ADDRESS:
        //case OTAI_ATTR_VALUE_TYPE_IP_PREFIX:
        case OTAI_ATTR_VALUE_TYPE_OBJECT_ID:
        //case OTAI_ATTR_VALUE_TYPE_LATCH_STATUS:
            break;

        //case OTAI_ATTR_VALUE_TYPE_JSON:
        //    otai_free_list(attr.value.json.json);
        //    break;

        case OTAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            otai_free_list(attr.value.objlist);
            break;

        case OTAI_ATTR_VALUE_TYPE_UINT8_LIST:
            otai_free_list(attr.value.u8list);
            break;

        case OTAI_ATTR_VALUE_TYPE_INT8_LIST:
            otai_free_list(attr.value.s8list);
            break;

//        case OTAI_ATTR_VALUE_TYPE_UINT16_LIST:
//            otai_free_list(attr.value.u16list);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_INT16_LIST:
//            otai_free_list(attr.value.s16list);
//            break;

        case OTAI_ATTR_VALUE_TYPE_UINT32_LIST:
            otai_free_list(attr.value.u32list);
            break;

        case OTAI_ATTR_VALUE_TYPE_INT32_LIST:
            otai_free_list(attr.value.s32list);
            break;

        case OTAI_ATTR_VALUE_TYPE_UINT32_RANGE:
        case OTAI_ATTR_VALUE_TYPE_INT32_RANGE:
            break;

        //case OTAI_ATTR_VALUE_TYPE_UINT16_RANGE_LIST:
        //    otai_free_list(attr.value.u16rangelist);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_VLAN_LIST:
        //    otai_free_list(attr.value.vlanlist);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_QOS_MAP_LIST:
        //    otai_free_list(attr.value.qosmap);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_MAP_LIST:
        //    otai_free_list(attr.value.maplist);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_ACL_RESOURCE_LIST:
        //    otai_free_list(attr.value.aclresource);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_IP_ADDRESS_LIST:
        //    otai_free_list(attr.value.ipaddrlist);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_SEGMENT_LIST:
        //    otai_free_list(attr.value.segmentlist);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_PORT_LANE_LATCH_STATUS_LIST:
        //    otai_free_list(attr.value.portlanelatchstatuslist);
        //    break;

            /* ACL FIELD DATA */

//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_BOOL:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT8:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT16:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT16:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT32:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_INT32:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT64:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_MAC:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV4:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_IPV6:
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
//            break;
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
//            otai_free_list(attr.value.aclfield.data.objlist);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_UINT8_LIST:
//            otai_free_list(attr.value.aclfield.mask.u8list);
//            otai_free_list(attr.value.aclfield.data.u8list);
//            break;
//
//            /* ACL ACTION DATA */
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_BOOL:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT8:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT8:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT16:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT16:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_UINT32:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_INT32:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_MAC:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV4:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IPV6:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_IP_ADDRESS:
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
//            otai_free_list(attr.value.aclaction.parameter.objlist);
//            break;
//
//        case OTAI_ATTR_VALUE_TYPE_ACL_CAPABILITY:
//            otai_free_list(attr.value.aclcapability.action_list);
//            break;

        //case OTAI_ATTR_VALUE_TYPE_AUTH_KEY:
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_ENCRYPT_KEY:
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SAK:
        //case OTAI_ATTR_VALUE_TYPE_MACSEC_AUTH_KEY:
        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SALT:
        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SCI:
        //case OTAI_ATTR_VALUE_TYPE_MACSEC_SSCI:
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_SYSTEM_PORT_CONFIG:
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_SYSTEM_PORT_CONFIG_LIST:
        //    otai_free_list(attr.value.sysportconfiglist);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_IP_PREFIX_LIST:
        //    otai_free_list(attr.value.ipprefixlist);
        //    break;

        //case OTAI_ATTR_VALUE_TYPE_POE_PORT_POWER_CONSUMPTION:
        //    break;

        default:
            SWSS_LOG_THROW("otai attr value %s is not implemented, FIXME", otai_serialize_attr_value_type(type).c_str());
    }
}

//void otai_deserialize_poe_port_active_channel_type(
//        _In_ const std::string& s,
//        _Out_ otai_poe_port_active_channel_type_t& value)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_poe_port_active_channel_type_t, (int32_t&)value);
//}

//void otai_deserialize_poe_port_class_method_type(
//        _In_ const std::string& s,
//        _Out_ otai_poe_port_class_method_type_t& value)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_poe_port_class_method_type_t, (int32_t&)value);
//}
//
//void otai_deserialzie_poe_port_signature_type(
//        _In_ const std::string& s,
//        _Out_ otai_poe_port_signature_type_t& value)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_poe_port_signature_type_t, (int32_t&)value);
//}
//
//void otai_deserialize_poe_port_power_consumption(
//        _In_ const std::string& s,
//        _Out_ otai_poe_port_power_consumption_t& value)
//{
//    SWSS_LOG_ENTER();
//
//    json j = json::parse(s);
//
//    otai_deserialize_poe_port_active_channel_type(j["active_channel"], value.active_channel);
//    otai_deserialize_number(j["voltage"], value.voltage);
//    otai_deserialize_number(j["current"], value.current);
//    otai_deserialize_number(j["consumption"], value.consumption);
//    otai_deserialize_poe_port_class_method_type(j["class_method"], value.class_method);
//    otai_deserialzie_poe_port_signature_type(j["signature_type"],value.signature_type);
//    otai_deserialize_number(j["measured_class_a"], value.measured_class_a);
//    otai_deserialize_number(j["assigned_class_a"], value.assigned_class_a);
//    otai_deserialize_number(j["measured_class_b"], value.measured_class_b);
//    otai_deserialize_number(j["assigned_class_b"], value.assigned_class_b);
//}

// deserialize free notifications

//void otai_deserialize_free_fdb_event(
//        _In_ otai_fdb_event_notification_data_t& fdb_event)
//{
//    SWSS_LOG_ENTER();
//
//    for (uint32_t i = 0; i < fdb_event.attr_count; ++i)
//    {
//        auto meta = otai_metadata_get_attr_metadata(OTAI_OBJECT_TYPE_FDB_ENTRY, fdb_event.attr[i].id);
//
//        if (meta == NULL)
//        {
//            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
//                    otai_serialize_object_type(OTAI_OBJECT_TYPE_FDB_ENTRY).c_str(),
//                    fdb_event.attr[i].id);
//        }
//
//        otai_deserialize_free_attribute_value(meta->attrvaluetype, fdb_event.attr[i]);
//    }
//
//    delete[] fdb_event.attr;
//}
//
//void otai_deserialize_free_fdb_event_ntf(
//        _In_ uint32_t count,
//        _In_ otai_fdb_event_notification_data_t* fdb_event)
//{
//    SWSS_LOG_ENTER();
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        otai_deserialize_free_fdb_event(fdb_event[i]);
//    }
//
//    delete[] fdb_event;
//}

//void otai_deserialize_free_nat_event(
//        _In_ otai_nat_event_notification_data_t& nat_event)
//{
//    SWSS_LOG_ENTER();
//}
//
//void otai_deserialize_free_nat_event_ntf(
//        _In_ uint32_t count,
//        _In_ otai_nat_event_notification_data_t* nat_event)
//{
//    SWSS_LOG_ENTER();
//
//    for (uint32_t i = 0; i < count; ++i)
//    {
//        otai_deserialize_free_nat_event(nat_event[i]);
//    }
//
//    delete[] nat_event;
//}

//void otai_deserialize_free_port_oper_status_ntf(
//        _In_ uint32_t count,
//        _In_ otai_port_oper_status_notification_t* port_oper_status)
//{
//    SWSS_LOG_ENTER();
//
//    delete[] port_oper_status;
//}

//void otai_deserialize_free_queue_deadlock_ntf(
//        _In_ uint32_t count,
//        _In_ otai_queue_deadlock_notification_data_t* queue_deadlock)
//{
//    SWSS_LOG_ENTER();
//
//    delete[] queue_deadlock;
//}
//
//void otai_deserialize_free_bfd_session_state_ntf(
//        _In_ uint32_t count,
//        _In_ otai_bfd_session_state_notification_t* bfd_session_state)
//{
//    SWSS_LOG_ENTER();
//
//    delete[] bfd_session_state;
//}
//
//void otai_deserialize_free_twamp_session_event_ntf(
//        _In_ uint32_t count,
//        _In_ otai_twamp_session_event_notification_data_t* twamp_session_event)
//{
//    SWSS_LOG_ENTER();
//
//    delete[] twamp_session_event;
//}
//
//void otai_deserialize_ingress_priority_group_attr(
//        _In_ const std::string& s,
//        _Out_ otai_ingress_priority_group_attr_t& attr)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_ingress_priority_group_attr_t, (int32_t&)attr);
//}

//void otai_deserialize_queue_attr(
//        _In_ const std::string& s,
//        _Out_ otai_queue_attr_t& attr)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_queue_attr_t, (int32_t&)attr);
//}

//void otai_deserialize_macsec_sa_attr(
//        _In_ const std::string& s,
//        _Out_ otai_macsec_sa_attr_t& attr)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_macsec_sa_attr_t, (int32_t&)attr);
//}
//
//void otai_deserialize_acl_counter_attr(
//        _In_ const std::string& s,
//        _Out_ otai_acl_counter_attr_t& attr)
//{
//    SWSS_LOG_ENTER();
//
//    otai_deserialize_enum(s, &otai_metadata_enum_otai_acl_counter_attr_t, (int32_t&)attr);
//}

// otairedis

void otai_deserialize(
        _In_ const std::string& s,
        _Out_ otai_redis_notify_syncd_t& value)
{
    SWSS_LOG_ENTER();

    if (s == SYNCD_INIT_VIEW)
    {
        value = OTAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;
    }
    else if (s == SYNCD_APPLY_VIEW)
    {
        value = OTAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;
    }
    else if (s == SYNCD_INSPECT_ASIC)
    {
        value = OTAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC;
    }
    else if (s == SYNCD_INVOKE_DUMP)
    {
        value = OTAI_REDIS_NOTIFY_SYNCD_INVOKE_DUMP;
    }
    else
    {
        SWSS_LOG_THROW("enum %s not found in otai_redis_notify_syncd_t", s.c_str());
    }
}

otai_redis_notify_syncd_t otai_deserialize_redis_notify_syncd(
        _In_ const std::string& s)
{
    SWSS_LOG_ENTER();

    otai_redis_notify_syncd_t value;

    otai_deserialize(s, value);

    return value;
}

void otai_deserialize_redis_communication_mode(
        _In_ const std::string& s,
        _Out_ otai_redis_communication_mode_t& value)
{
    SWSS_LOG_ENTER();

    if (s == REDIS_COMMUNICATION_MODE_REDIS_ASYNC_STRING)
    {
        value = OTAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC;
    }
    else if (s == REDIS_COMMUNICATION_MODE_REDIS_SYNC_STRING)
    {
        value = OTAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC;
    }
    else if (s == REDIS_COMMUNICATION_MODE_ZMQ_SYNC_STRING)
    {
        value = OTAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC;
    }
    else
    {
        SWSS_LOG_THROW("enum '%s' not found in otai_redis_communication_mode_t", s.c_str());
    }
}

void otai_deserialize_redis_port_attr_id(
        _In_ const std::string& s,
        _Out_ otai_redis_port_attr_t& value)
{
    SWSS_LOG_ENTER();

    for (const auto& entry : otai_redis_port_attr_to_name_map)
    {
        if (s == entry.second)
        {
            value = entry.first;
            return;
        }
    }

    SWSS_LOG_WARN("%s is not found in otai_redis_port_attr_to_name_map.", s.c_str());

    otai_deserialize_number(s, value, false);
}

// Link event damping.

void otai_deserialize_redis_link_event_damping_algorithm(
        _In_ const std::string& s,
        _Out_ otai_redis_link_event_damping_algorithm_t& value)
{
    SWSS_LOG_ENTER();

    for (const auto& entry : otai_redis_link_event_damping_algorithm_to_name_map)
    {
        if (s == entry.second)
        {
            value = entry.first;
            return;
        }
    }

    SWSS_LOG_WARN("%s is not found in otai_redis_link_event_damping_algorithm_to_name_map.", s.c_str());

    otai_deserialize_number(s, value, false);
}

void otai_deserialize_redis_link_event_damping_aied_config(
        _In_ const std::string& s,
        _Out_ otai_redis_link_event_damping_algo_aied_config_t& value)
{
    SWSS_LOG_ENTER();

    json j = json::parse(s);

    otai_deserialize_number(j["max_suppress_time"], value.max_suppress_time, false);
    otai_deserialize_number(j["suppress_threshold"], value.suppress_threshold, false);
    otai_deserialize_number(j["reuse_threshold"], value.reuse_threshold, false);
    otai_deserialize_number(j["decay_half_life"], value.decay_half_life, false);
    otai_deserialize_number(j["flap_penalty"], value.flap_penalty, false);
}
