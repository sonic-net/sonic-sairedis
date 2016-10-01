#include "sai_meta.h"
#include <sstream>

sai_object_type_t sai_object_type_query(
        _In_ sai_object_id_t oid)
{
    return SAI_OBJECT_TYPE_NULL;
}

void metadata_generate_type(const sai_attr_metadata_t& md, std::stringstream& ss)
{
    ss << "@type ";

#define ST(x,y) case SAI_SERIALIZATION_TYPE_ ## x: ss << #y; break;
#define STF(x,y) case SAI_SERIALIZATION_TYPE_ ## x: ss << "sai_acl_field_data_t " << #y; break;
#define STA(x,y) case SAI_SERIALIZATION_TYPE_ ## x: ss << "sai_acl_action_data_t " << #y; break;

    if (md.isenum())
    {
        ss << md.enumtypestr;
    }
    else if (md.isenumlist())
    {
        ss << md.enumtypestr;
    }
    else
        switch (md.serializationtype)
        {
            ST(BOOL, bool);
            ST(CHARDATA, char);
            ST(UINT8, sai_uint8_t);
            ST(INT8, sai_int8_t);
            ST(UINT16, sai_uint16_t);
            ST(INT16, sai_int16_t);
            ST(UINT32, sai_uint32_t);
            ST(INT32, sai_int32_t);
            ST(UINT64, sai_uint64_t);
            ST(INT64, sai_int64_t);
            ST(MAC, sai_mac_t);
            ST(IP4, sai_ip4_t);
            ST(IP6, sai_ip6_t);
            ST(IP_ADDRESS, sai_ip_address_t);
            ST(OBJECT_ID, sai_object_id_t);
            ST(OBJECT_LIST, sai_object_list_t);
            ST(UINT8_LIST, sai_uint8_list_t);
            ST(INT8_LIST, sai_int8_list_t);
            ST(UINT16_LIST, sai_uint16_list_t);
            ST(INT16_LIST, sai_int16_list_t);
            ST(UINT32_LIST, sai_uint32_list_t);
            ST(INT32_LIST, sai_int32_list_t);
            ST(UINT32_RANGE, sai_uint32_range_t);
            ST(INT32_RANGE, sai_int32_range_t);
            ST(VLAN_LIST, sai_vlan_list_t);

            STF(ACL_FIELD_DATA_BOOL, bool);
            STF(ACL_FIELD_DATA_UINT8, sai_uint8_t);
            STF(ACL_FIELD_DATA_INT8, sai_int8_t);
            STF(ACL_FIELD_DATA_UINT16, sai_uint16_t);
            STF(ACL_FIELD_DATA_INT16, sai_int16_t);
            STF(ACL_FIELD_DATA_INT32, sai_int32_t);
            STF(ACL_FIELD_DATA_UINT32, sai_uint32_t);
            STF(ACL_FIELD_DATA_MAC, sai_mac_t);
            STF(ACL_FIELD_DATA_IP4, sai_ip4_t);
            STF(ACL_FIELD_DATA_IP6, sai_ip6_t);
            STF(ACL_FIELD_DATA_OBJECT_ID, sai_object_id_t);
            STF(ACL_FIELD_DATA_OBJECT_LIST, sai_object_list_t);
            STF(ACL_FIELD_DATA_UINT8_LIST, sai_uint8_list_t);

            STA(ACL_ACTION_DATA_UINT8, sai_uint8_t);
            STA(ACL_ACTION_DATA_INT8, sai_int8_t);
            STA(ACL_ACTION_DATA_UINT16, sai_uint16_t);
            STA(ACL_ACTION_DATA_INT16, sai_int16_t);
            STA(ACL_ACTION_DATA_UINT32, sai_uint32_t);
            STA(ACL_ACTION_DATA_INT32, sai_int32_t);
            STA(ACL_ACTION_DATA_MAC, sai_mac_t);
            STA(ACL_ACTION_DATA_IPV4, sai_ipv4_t);
            STA(ACL_ACTION_DATA_IPV6, sai_ipv6_t);
            STA(ACL_ACTION_DATA_OBJECT_ID, sai_object_id_t);
            STA(ACL_ACTION_DATA_OBJECT_LIST, sai_object_list_t);

            ST(QOS_MAP_LIST, sai_qos_map_list_t);

            default:

            std::cerr << "unknown serialization type: " << md.serializationtype << std::endl;
            throw;
        }

    ss << std::endl;
}

void metadata_generate_objects(const sai_attr_metadata_t& md, std::stringstream& ss)
{
    if (md.allowedobjecttypes.size() == 0)
    {
        return;
    }

    ss << "@objects ";

    size_t count = md.allowedobjecttypes.size();

    uint32_t i = 0;

    for (auto it = md.allowedobjecttypes.begin(); it !=  md.allowedobjecttypes.end(); it++, i++)
    {
        ss <<  "SAI_OBJECT_TYPE_" << get_object_type_name(*it);

        if (i != count - 1)
            ss << ", ";
    }

    ss << std::endl;
}

void metadata_generate_flags(const sai_attr_metadata_t& md, std::stringstream& ss)
{
    ss << "@flags ";

    switch ((int)md.flags)
    {
        case SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY | SAI_ATTR_FLAGS_KEY:
            ss << "MANDATORY_ON_CREATE | CREATE_ONLY | KEY";
            break;
        case SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY:
            ss << "MANDATORY_ON_CREATE | CREATE_ONLY";
            break;
        case SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET:
            ss << "MANDATORY_ON_CREATE | CREATE_AND_SET";
            break;
        case SAI_ATTR_FLAGS_CREATE_ONLY:
            ss << "CREATE_ONLY";
            break;
        case SAI_ATTR_FLAGS_CREATE_AND_SET:
            ss << "CREATE_AND_SET";
            break;
        case SAI_ATTR_FLAGS_READ_ONLY:
            ss << "READ_ONLY";
            break;

        default:
            std::cerr << "unknown flags" << std::endl;
            throw;
    }

    ss << std::endl;
}

void metadata_generate_allownull(const sai_attr_metadata_t& md, std::stringstream& ss)
{
    if (md.allownullobjectid)
        ss << "@allownull true" << std::endl;
}

void metadata_generate_condition(const sai_attr_metadata_t& md, std::stringstream& ss)
{
    auto &hash = AttributesMetadata[md.objecttype];

    if (md.conditions.empty())
    {
        return;
    }

    size_t index = 0;

    for (auto itc = md.conditions.begin(); itc!= md.conditions.end(); ++itc)
    {
        if (index == 0)
        {
            ss << "@condition ";
        }

        auto& c = *itc;
        auto it = hash.find(c.attrid);

        if (it == hash.end())
        {
            std::cerr << "cond attr not found" << std::endl;
            throw;
        }

        const sai_attr_metadata_t &cmd = *it->second;

        switch (cmd.serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_BOOL:

                ss
                    << get_attr_name(cmd.objecttype,cmd.attrid)
                    << " == "
                    << (c.condition.booldata ? "true" : "false");

                break;

            case SAI_SERIALIZATION_TYPE_INT32:

                ss
                    << get_attr_name(cmd.objecttype,cmd.attrid)
                    << " == "
                    << c.enumstr;
                break;

            default:

                std::cerr << "wrong serialization type " << cmd.serializationtype;
                throw;

        }

        if (index != md.conditions.size()-1)
        {
            ss << " or ";
        }

        index++;
    }

    ss << std::endl;
}

std::string get_default_value_string(const sai_attr_metadata_t& md)
{
    std::stringstream ss;

    // TODO we could assume default to false and 0 if not defined
    auto &d = md.defaultvalue;

    switch (md.serializationtype)
    {
        case SAI_SERIALIZATION_TYPE_BOOL:
            return d.booldata? "true" : "false";
        case SAI_SERIALIZATION_TYPE_UINT8:
            return std::to_string(d.u8);
        case SAI_SERIALIZATION_TYPE_UINT8_LIST:
            return "complex";

        case SAI_SERIALIZATION_TYPE_INT32:
            return std::to_string(d.s32);
        case SAI_SERIALIZATION_TYPE_UINT16:
            return std::to_string(d.u16);

        case SAI_SERIALIZATION_TYPE_UINT32:
            return std::to_string(d.u32);
        case SAI_SERIALIZATION_TYPE_UINT64:
            return std::to_string(d.u64);
        case SAI_SERIALIZATION_TYPE_OBJECT_ID:
            return std::to_string(d.u32);

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            return d.aclfield.data.booldata? "true" : "false";

        default:

            std::cerr << "not supported ser type: " << md.serializationtype <<std::endl;
            throw;
    }
}

void metadata_generate_default(const sai_attr_metadata_t& md, std::stringstream& ss)
{
#define STTA(x) case SAI_SERIALIZATION_TYPE_ ## x : return;

    // acl field and action don't have default val
    switch (md.serializationtype)
    {
        STTA(OBJECT_ID)

            STTA(ACL_FIELD_DATA_UINT8)
            STTA(ACL_FIELD_DATA_INT8)
            STTA(ACL_FIELD_DATA_UINT16)
            STTA(ACL_FIELD_DATA_INT16)
            STTA(ACL_FIELD_DATA_INT32)
            STTA(ACL_FIELD_DATA_UINT32)
            STTA(ACL_FIELD_DATA_MAC)
            STTA(ACL_FIELD_DATA_IP4)
            STTA(ACL_FIELD_DATA_IP6)
            STTA(ACL_FIELD_DATA_OBJECT_ID)
            STTA(ACL_FIELD_DATA_OBJECT_LIST)
            STTA(ACL_FIELD_DATA_UINT8_LIST)

            STTA(ACL_ACTION_DATA_UINT8)
            STTA(ACL_ACTION_DATA_INT8)
            STTA(ACL_ACTION_DATA_UINT16)
            STTA(ACL_ACTION_DATA_INT16)
            STTA(ACL_ACTION_DATA_UINT32)
            STTA(ACL_ACTION_DATA_INT32)
            STTA(ACL_ACTION_DATA_MAC)
            STTA(ACL_ACTION_DATA_IPV4)
            STTA(ACL_ACTION_DATA_IPV6)
            STTA(ACL_ACTION_DATA_OBJECT_ID)
            STTA(ACL_ACTION_DATA_OBJECT_LIST)

        default:
            break;
    }

    switch (md.defaultvaluetype)
    {
        case SAI_DEFAULT_VALUE_TYPE_NONE:
            return;
        case SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST:
            ss << "@default empty" << std::endl;
            return;

        case SAI_DEFAULT_VALUE_TYPE_CONST:
            ss << "@default " <<  get_default_value_string(md) << std::endl;
            break;

        case SAI_DEFAULT_VALUE_TYPE_ATTR_RANGE:
        case SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE:
        case SAI_DEFAULT_VALUE_TYPE_VENDOR_SPECIFIC:
        case SAI_DEFAULT_VALUE_TYPE_SWITCH_INTERNAL:
        case SAI_DEFAULT_VALUE_TYPE_INHERIT:

            ss << "@default complex" << std::endl;
            break;

        default:
            std::cerr << "unknown def type" <<std::endl;
            throw;
    }
}

void metadata_generate_isvlan(const sai_attr_metadata_t& md, std::stringstream& ss)
{
    if (md.isvlan())
    {
        ss << "@isvlan true" << std::endl;
    }
}

std::string metadata_generate(const sai_attr_metadata_t& md)
{
    std::stringstream ss;

    ss << "@objecttype " <<  "SAI_OBJECT_TYPE_" << get_object_type_name(md.objecttype) << std::endl;
    ss << "@attrid " <<  get_attr_name(md.objecttype,md.attrid) << std::endl;

    metadata_generate_type(md, ss);
    metadata_generate_objects(md, ss);
    metadata_generate_flags(md, ss);
    metadata_generate_allownull(md, ss);
    metadata_generate_default(md, ss);
    metadata_generate_condition(md, ss);
    metadata_generate_isvlan(md, ss);

    // TODO should those be bool's ? like @bools vlan allowrepetitiononlist allowmixedobjecttypes
    // bool allowrepetitiononlist; (false by default)
    // bool allowmixedobjecttypes; (false by default) (not respected currently)
    // bool allowemptylist; to set/create (false by default)

    return ss.str();
}

std::string generate_metadata()
{
    std::stringstream ss;

#define GENERATE(x) \
    for (size_t i = 0; i < sai_ ## x ## _attr_metadata_count; ++i) \
    ss << metadata_generate(sai_ ## x ## _attr_metadata[i]);

    GENERATE(acl_counter);
    GENERATE(acl_entry);
    GENERATE(acl_range);
    GENERATE(acl_table);
    GENERATE(buffer_pool);
    GENERATE(buffer_profile);
    GENERATE(hash);
    GENERATE(fdb);
    GENERATE(hostintf);
    GENERATE(hostintf_trap);
    GENERATE(hostintf_trap_group);
    GENERATE(lag);
    GENERATE(lag_member);
    GENERATE(mirror);
    GENERATE(neighbor);
    GENERATE(nexthop);
    GENERATE(nexthopgroup);
    GENERATE(policer);
    GENERATE(port);
    GENERATE(qos_maps);
    GENERATE(queue);
    GENERATE(route);
    GENERATE(router);
    GENERATE(routerintf);
    GENERATE(samplepacket);
    GENERATE(scheduler);
    GENERATE(scheduler_group);
    GENERATE(stp);
    GENERATE(switch);
    GENERATE(tunnel);
    GENERATE(tunnel_map);
    GENERATE(tunnel_table_entry);
    GENERATE(udf);
    GENERATE(udf_group);
    GENERATE(udf_match);
    GENERATE(vlan);
    GENERATE(vlan_member);
    GENERATE(wred);
    
    return ss.str();
}

int main()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    SWSS_LOG_ENTER();

    meta_init_db();

    std::cout << generate_metadata();

    return 0;
}
