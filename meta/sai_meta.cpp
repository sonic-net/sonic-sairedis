#include "sai_meta.h"
#include "sai_extra.h"
#include "saiserialize.h"

#include <string.h>
#include <arpa/inet.h>

#include <map>
#include <iterator>

bool is_ipv6_mask_valid(
        _In_ const uint8_t* mask)
{
    if (mask == NULL)
    {
        SWSS_LOG_ERROR("mask is null");

        return false;
    }

    int ones = 0;
    bool zeros = false;

    for (uint8_t i = 0; i < 128; i++)
    {
        bool bit = mask[i/8] & (1 << (7 - (i%8)));

        if (zeros && bit)
        {
            return false;
        }

        zeros |= !bit;

        if (bit)
        {
            ones++;
        }
    }

    return true;
}

int get_ipv6_mask(
        _In_ const uint8_t* mask)
{
    if (mask == NULL)
    {
        SWSS_LOG_ERROR("FATAL: mask is null");
        throw;
    }

    int ones = 0;
    bool zeros = false;

    for (uint8_t i = 0; i < 128; i++)
    {
        bool bit = mask[i/8] & (1 << (7 - (i%8)));

        if (zeros && bit)
        {
            SWSS_LOG_ERROR("FATAL: mask is not correct");
            throw;
        }

        zeros |= !bit;

        if (bit)
        {
            ones++;
        }
    }

    return ones;
}

std::string mac_to_string(
        _In_ const sai_mac_t& mac)
{
    char macstr[128];

    sprintf(macstr, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return macstr;
}

std::string fdb_entry_to_string(
        _In_ const sai_fdb_entry_t& fdb_entry)
{
    return "mac:" + mac_to_string(fdb_entry.mac_address) +";vlan:" + std::to_string(fdb_entry.vlan_id);
}

std::string oid_to_string(
        _In_ sai_object_id_t oid)
{
    char str[32];

    sprintf(str, "0x%lx", oid);

    return str;
}

std::string ip_addr_to_string(
        _In_ sai_ip_addr_family_t family, const void* mem)
{
    char str[INET6_ADDRSTRLEN];

    switch (family)
    {
        case SAI_IP_ADDR_FAMILY_IPV4:

            {
                struct sockaddr_in sa;

                memcpy(&sa.sin_addr, mem, 4);

                if (inet_ntop(AF_INET, &(sa.sin_addr), str, INET_ADDRSTRLEN) == NULL)
                {
                    SWSS_LOG_ERROR("FATAL: failed to convert IPv4 address, errno: %d", errno);
                    throw;
                }

                break;
            }

        case SAI_IP_ADDR_FAMILY_IPV6:

            {
                struct sockaddr_in6 sa6;

                memcpy(&sa6.sin6_addr, mem, 16);

                if (inet_ntop(AF_INET6, &(sa6.sin6_addr), str, INET6_ADDRSTRLEN) == NULL)
                {
                    SWSS_LOG_ERROR("FATAL: failed to convert IPv6 address, errno: %d", errno);
                    throw;
                }

                break;
            }

        default:

            SWSS_LOG_ERROR("FATAL: invalid ip address family: %d", family);
            throw;
    }

    return str;
}

std::string ip_address_to_string(
        _In_ const sai_ip_address_t& ip)
{
    return "[" + ip_addr_to_string(ip.addr_family, &ip.addr) + "]";
}

std::string neighbor_entry_to_string(
        _In_ const sai_neighbor_entry_t& neighbor_entry)
{
    return "rif:" + oid_to_string(neighbor_entry.rif_id) + ";ip:" + ip_address_to_string(neighbor_entry.ip_address);
}

std::string ip_prefix_to_string(
        _In_ const sai_ip_prefix_t& prefix)
{
    if (prefix.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return "[" + ip_addr_to_string(prefix.addr_family, &prefix.addr) + "/" +
            ip_addr_to_string(prefix.addr_family, &prefix.mask) + "]";
    }

    return "[" + ip_addr_to_string(prefix.addr_family, &prefix.addr) + "/" +
        std::to_string(get_ipv6_mask(prefix.mask.ip6)) + "]";
}

std::string route_entry_to_string(
        _In_ const sai_unicast_route_entry_t& route_entry)
{
    return "vr:" + oid_to_string(route_entry.vr_id) + ";dest:" + ip_prefix_to_string(route_entry.destination);
}

std::string get_object_meta_key_string(
        _In_ const sai_object_meta_key_t& meta_key)
{
    SWSS_LOG_ENTER();

    std::string key;

    switch (meta_key.object_type)
    {
        case SAI_OBJECT_TYPE_SWITCH:
            key = "switch:0";
            break;

        case SAI_OBJECT_TYPE_VLAN:
            key = "vlan:" + std::to_string(meta_key.key.vlan_id);
            break;

        case SAI_OBJECT_TYPE_TRAP:
            key = "trap:" + std::to_string(meta_key.key.trap_id);
            break;

        case SAI_OBJECT_TYPE_FDB:
            key = "fdb:" + fdb_entry_to_string(meta_key.key.fdb_entry);
            break;

        case SAI_OBJECT_TYPE_ROUTE:
            key = "route:" + route_entry_to_string(meta_key.key.route_entry);
            break;

        case SAI_OBJECT_TYPE_NEIGHBOR:
            key = "neighbor:" + neighbor_entry_to_string(meta_key.key.neighbor_entry);
            break;

        default:
            key = "oid:" + oid_to_string(meta_key.key.object_id);
            break;
    }

    SWSS_LOG_DEBUG("%s", key.c_str());

    return key;
}

const sai_attribute_t* get_attribute_by_id(
        _In_ sai_attr_id_t id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list is null");

        return NULL;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        if (attr_list[i].id == id)
        {
            return &attr_list[i];
        }
    }

    return NULL;
}

const sai_attr_metadata_t* get_attribute_metadata(
        _In_ sai_object_type_t objecttype,
        _In_ sai_attr_id_t attrid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("objecttype: %s, attrid: %d", get_object_type_name(objecttype), attrid);

    const auto &it = AttributesMetadata.find(objecttype);

    if (it == AttributesMetadata.end())
    {
        SWSS_LOG_ERROR("invalid object type value: %d", objecttype);

        return NULL;
    }

    const auto &attrset = it->second;

    const auto &ita = attrset.find(attrid);

    if (ita == attrset.end())
    {
        SWSS_LOG_ERROR("attribute %d not found in metadata", attrid);

        return NULL;
    }

    return ita->second;
}

std::vector<const sai_attr_metadata_t*> get_attributes_metadata(
        _In_ sai_object_type_t objecttype)
{
    SWSS_LOG_ENTER();

    // NOTE: this is not performance best, we can do better

    SWSS_LOG_DEBUG("objecttype: %d", objecttype);

    const auto &it = AttributesMetadata.find(objecttype);

    std::vector<const sai_attr_metadata_t*> attrs;

    if (it == AttributesMetadata.end())
    {
        SWSS_LOG_ERROR("invalid object type value: %d", objecttype);

        return attrs;
    }

    for (auto i: it->second)
    {
        attrs.push_back(i.second);
    }

    return attrs;
}

class SaiAttrWrapper
{
    public:

        SaiAttrWrapper(
                _In_ const sai_attr_metadata_t* meta,
                _In_ const sai_attribute_t& attr):
            m_meta(meta),
            m_attr(attr)
    {
        SWSS_LOG_ENTER();

        m_attr.id = attr.id;

        std::string s = sai_serialize_attr_value(*meta, attr, false);

        sai_deserialize_attr_value(s, *meta, m_attr, false);
    }

        ~SaiAttrWrapper()
        {
            sai_deserialize_free_attribute_value(m_meta->serializationtype, m_attr);
        }

        const sai_attribute_t* getattr() const
        {
            return &m_attr;
        }

    private:

        SaiAttrWrapper(const SaiAttrWrapper&);
        SaiAttrWrapper& operator=(const SaiAttrWrapper&);

        const sai_attr_metadata_t* m_meta;
        sai_attribute_t m_attr;
};

#define META_LOG_ERROR(md, format, ...) SWSS_LOG_ERROR("%s " format, get_attr_info(md).c_str(), ##__VA_ARGS__)
#define META_LOG_DEBUG(md, format, ...) SWSS_LOG_DEBUG("%s " format, get_attr_info(md).c_str(), ##__VA_ARGS__)
#define META_LOG_NOTICE(md, format, ...) SWSS_LOG_NOTICE("%s " format, get_attr_info(md).c_str(), ##__VA_ARGS__)
#define META_LOG_INFO(md, format, ...) SWSS_LOG_INFO("%s " format, get_attr_info(md).c_str(), ##__VA_ARGS__)

// traps and vlan will be converted to oid
// fdb, route, neighbor don't need reference count,
// they are leafs and can be removed at any time
std::unordered_map<sai_object_id_t,int32_t> ObjectReferences;
std::unordered_map<sai_vlan_id_t,int32_t> VlanReferences;
std::unordered_map<std::string,std::string> AttributeKeys;
std::unordered_map<std::string,std::unordered_map<sai_attr_id_t,std::shared_ptr<SaiAttrWrapper>>> ObjectAttrHash;

// GENERIC REFERENCE FUNCTIONS

bool object_reference_exists(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    bool exists = ObjectReferences.find(oid) != ObjectReferences.end();

    SWSS_LOG_DEBUG("object 0x%llx refrence: %s", oid, exists ? "exists" : "missing");

    return exists;
}

void object_reference_inc(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        return;
    }

    if (!object_reference_exists(oid))
    {
        SWSS_LOG_ERROR("FATAL: object oid 0x%llx not in reference map", oid);
        throw;
    }

    ObjectReferences[oid]++;

    SWSS_LOG_DEBUG("increased reference on oid 0x%llx to %d", oid, ObjectReferences[oid]);
}

void object_reference_dec(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        return;
    }

    if (!object_reference_exists(oid))
    {
        SWSS_LOG_ERROR("FATAL: object oid 0x%llx not in reference map", oid);
        throw;
    }

    ObjectReferences[oid]--;

    if (ObjectReferences[oid] < 0)
    {
        SWSS_LOG_ERROR("FATAL: object oid 0x%llx reference count is negative!", oid);
        throw;
    }

    SWSS_LOG_DEBUG("decreased reference on oid 0x%llx to %d", oid, ObjectReferences[oid]);
}

void object_reference_dec(
        _In_ const sai_object_list_t& list)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < list.count; ++i)
    {
        object_reference_dec(list.list[i]);
    }
}

void object_reference_inc(
        _In_ const sai_object_list_t& list)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < list.count; ++i)
    {
        object_reference_inc(list.list[i]);
    }
}

void object_reference_insert(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (object_reference_exists(oid))
    {
        SWSS_LOG_ERROR("FATAL: object oid 0x%llx already in reference map");
        throw;
    }

    ObjectReferences[oid] = 0;

    SWSS_LOG_DEBUG("inserted reference on 0x%llx", oid);
}

int32_t object_reference_count(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (object_reference_exists(oid))
    {
        int32_t count = ObjectReferences[oid];

        SWSS_LOG_DEBUG("reference count on oid 0x%llx is %d", oid, count);

        return count;
    }

    SWSS_LOG_ERROR("FATAL: object oid 0x%llx reference not in map", oid);
    throw;
}

void object_reference_remove(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (object_reference_exists(oid))
    {
        int32_t count = object_reference_count(oid);

        if (count > 0)
        {
            SWSS_LOG_ERROR("FATAL: removing object oid 0x%llx but reference count is: %d", oid, count);
            throw;
        }
    }

    SWSS_LOG_DEBUG("removing object oid 0x%llx reference", oid);

    ObjectReferences.erase(oid);
}

// VLAN REFERENCE FUNCTIONS

bool vlan_reference_exists(
        _In_ sai_vlan_id_t vlanid)
{
    SWSS_LOG_ENTER();

    bool exists = VlanReferences.find(vlanid) != VlanReferences.end();

    SWSS_LOG_DEBUG("vlan %u refrence: %s", vlanid, exists ? "exists" : "missing");

    return exists;
}

void vlan_reference_inc(
        _In_ sai_vlan_id_t vlanid)
{
    SWSS_LOG_ENTER();

    if (!vlan_reference_exists(vlanid))
    {
        SWSS_LOG_ERROR("FATAL: vlan vlanid %u not in reference map", vlanid);
        throw;
    }

    VlanReferences[vlanid]++;

    SWSS_LOG_DEBUG("increased reference on vlan %u to %d", vlanid, VlanReferences[vlanid]);
}

void vlan_reference_dec(
        _In_ sai_vlan_id_t vlanid)
{
    SWSS_LOG_ENTER();

    if (!vlan_reference_exists(vlanid))
    {
        SWSS_LOG_ERROR("FATAL: vlan vlanid %u not in reference map", vlanid);
        throw;
    }

    VlanReferences[vlanid]--;

    if (VlanReferences[vlanid] < 0)
    {
        SWSS_LOG_ERROR("FATAL: vlan vlanid %u reference count is negative!", vlanid);
        throw;
    }

    SWSS_LOG_DEBUG("decreased reference on vlan %u to %d", vlanid, VlanReferences[vlanid]);
}

void vlan_reference_insert(
        _In_ sai_vlan_id_t vlanid)
{
    SWSS_LOG_ENTER();

    if (vlan_reference_exists(vlanid))
    {
        SWSS_LOG_ERROR("FATAL: vlan %u already in reference map", vlanid);
        throw;
    }

    VlanReferences[vlanid] = 0;

    SWSS_LOG_DEBUG("inserted reference on vlan %u", vlanid);
}

int32_t vlan_reference_count(
        _In_ sai_vlan_id_t vlanid)
{
    SWSS_LOG_ENTER();

    if (vlan_reference_exists(vlanid))
    {
        int32_t count = VlanReferences[vlanid];

        SWSS_LOG_DEBUG("reference count on vlan %u is %d", vlanid, count);

        return count;
    }

    SWSS_LOG_ERROR("FATAL: vlan vlanid 0x%llx reference not in map", vlanid);
    throw;
}

void vlan_reference_remove(
        _In_ sai_vlan_id_t vlanid)
{
    SWSS_LOG_ENTER();

    if (vlan_reference_exists(vlanid))
    {
        int32_t count = vlan_reference_count(vlanid);

        if (count > 0)
        {
            SWSS_LOG_ERROR("FATAL: removing vlan vlanid 0x%llx but reference count is: %d", vlanid, count);
            throw;
        }
    }

    VlanReferences.erase(vlanid);

    SWSS_LOG_DEBUG("removing vlan %u reference", vlanid);
}

bool object_exists(
        _In_ const std::string& key)
{
    SWSS_LOG_ENTER();

    bool exists = ObjectAttrHash.find(key) != ObjectAttrHash.end();

    SWSS_LOG_DEBUG("%s %s", key.c_str(), exists ? "exists" : "missing");

    return exists;
}

bool object_exists(
        _In_ const sai_object_meta_key_t meta_key)
{
    SWSS_LOG_ENTER();

    std::string key = get_object_meta_key_string(meta_key);

    return object_exists(key);
}

sai_status_t meta_init_db()
{
    SWSS_LOG_ENTER();

    meta_init();

    // we need local db to keep track if
    // we are performing "get" or set on
    // conditional attribute and whether
    // condition is in force

    ObjectReferences.clear();
    VlanReferences.clear();
    ObjectAttrHash.clear();
    AttributeKeys.clear();

    // init switch

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_SWITCH, .key = { } };

    std::string switch_key = get_object_meta_key_string(meta_key);

    ObjectAttrHash[switch_key] = { };

    // init default vlan

    sai_object_meta_key_t meta_key_vlan = { .object_type = SAI_OBJECT_TYPE_VLAN, .key = { .vlan_id = DEFAULT_VLAN_NUMBER } };

    std::string vlan_key = get_object_meta_key_string(meta_key_vlan);

    ObjectAttrHash[vlan_key] = { };

    vlan_reference_insert(DEFAULT_VLAN_NUMBER);
    vlan_reference_inc(DEFAULT_VLAN_NUMBER);

    // init traps

    for (auto& trap: enum_sai_hostif_trap_id_t_values)
    {
        sai_object_meta_key_t meta_key_trap = { .object_type = SAI_OBJECT_TYPE_TRAP, .key = { .trap_id = (sai_hostif_trap_id_t)trap } };

        std::string trap_key = get_object_meta_key_string(meta_key_trap);

        ObjectAttrHash[trap_key] = { };

        // not need for now creating trap references since
        // in this SAI all traps are created by default
    }

    // TODO For object references like virtual router or vlanmembers they can
    // be using some other references (like vlan member can use port or vlan so
    // there may be existing dependency there and we dont know it so we should
    // do actual "get" here and populate them we could use object api to retrve
    // all objects and rebuild dependencies basing on metadata.

    // TODO Getting default object id's like TRAP which can contain other
    // object id as dependency (in this case queue/policer) we need to also GET
    // those attributes at first switch init to have full asic view of what's
    // going on and also we need extra logic on remove function (we can add 4
    // pointers in metadata c/r/s/g) because some default object's can't be
    // removed like cpu port or default virtual router and probably default
    // trap group.

    return SAI_STATUS_SUCCESS;
}

const sai_attribute_t* get_object_previous_attr(
        _In_ const sai_object_meta_key_t meta_key,
        _In_ const sai_attr_metadata_t& md)
{
    SWSS_LOG_ENTER();

    std::string key = get_object_meta_key_string(meta_key);

    auto it = ObjectAttrHash.find(key);

    if (it == ObjectAttrHash.end())
    {
        SWSS_LOG_ERROR("object key %s not found", key.c_str());

        return NULL;
    }

    auto ita = it->second.find(md.attrid);

    if (ita == it->second.end())
    {
        // attribute id not found
        return NULL;
    }

    // NOTE: this is actually dangerous since
    // we possibly expose memory on attribute
    // list that could be already freed
    return ita->second->getattr();
}

void set_object(
        _In_ const sai_object_meta_key_t meta_key,
        _In_ const sai_attr_metadata_t& md,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    std::string key = get_object_meta_key_string(meta_key);

    if (!object_exists(key))
    {
        SWSS_LOG_ERROR("FATAL: object %s don't exists", key.c_str());
        throw;
    }

    META_LOG_DEBUG(md, "set attribute %d on %s", attr->id, key.c_str());

    auto p = new SaiAttrWrapper(&md,*attr);

    ObjectAttrHash[key][attr->id] = std::shared_ptr<SaiAttrWrapper>(p);
}

const std::vector<std::shared_ptr<SaiAttrWrapper>> get_object(
        _In_ const sai_object_meta_key_t meta_key)
{
    std::string key = get_object_meta_key_string(meta_key);

    if (!object_exists(key))
    {
        SWSS_LOG_ERROR("FATAL: object %s don't exists", key.c_str());
        throw;
    }

    std::vector<std::shared_ptr<SaiAttrWrapper>> attrs;

    const auto& hash = ObjectAttrHash[key];

    for (auto it = hash.begin(); it != hash.end(); ++it)
    {
        attrs.push_back(it->second);
    }

    return attrs;
}

void remove_object(
        _In_ const sai_object_meta_key_t meta_key)
{
    SWSS_LOG_ENTER();

    std::string key = get_object_meta_key_string(meta_key);

    if (!object_exists(key))
    {
        SWSS_LOG_ERROR("FATAL: object %s don't exists", key.c_str());
        throw;
    }

    SWSS_LOG_DEBUG("removing object %s", key.c_str());

    ObjectAttrHash.erase(key);
}

void create_object(
        _In_ const sai_object_meta_key_t& meta_key)
{
    SWSS_LOG_ENTER();

    std::string key = get_object_meta_key_string(meta_key);

    if (object_exists(key))
    {
        SWSS_LOG_ERROR("FATAL: object %s already exists", key.c_str());
        throw;
    }

    SWSS_LOG_DEBUG("creating object %s", key.c_str());

    ObjectAttrHash[key] = {};
}

sai_status_t meta_generic_validation_objlist(
        _In_ const sai_attr_metadata_t& md,
        _In_ uint32_t count,
        _In_ const sai_object_id_t* list)
{
    SWSS_LOG_ENTER();

    if (count > MAX_LIST_COUNT)
    {
        META_LOG_ERROR(md, "object list count %u is > then max list count %u", count, MAX_LIST_COUNT);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (list == NULL)
    {
        if (count == 0)
        {
            return SAI_STATUS_SUCCESS;
        }

        META_LOG_ERROR(md, "object list is null, but count is %u", count);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::set<sai_object_id_t> oids;

    sai_object_type_t object_type = SAI_OBJECT_TYPE_NULL;

    for (uint32_t i = 0; i < count; ++i)
    {
        sai_object_id_t oid = list[i];

        if (oids.find(oid) != oids.end())
        {
            META_LOG_ERROR(md, "object on list [%u] oid 0x%llx is duplicated, but not allowed", i, oid);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        oids.insert(oid);

        if (oid == SAI_NULL_OBJECT_ID)
        {
            if (md.allownullobjectid)
            {
                // ok, null object is allowed
                continue;
            }

            META_LOG_ERROR(md, "object on list [%u] is NULL, but not allowed", i);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        sai_object_type_t ot = sai_object_type_query(oid);

        if (ot == SAI_NULL_OBJECT_ID)
        {
            META_LOG_ERROR(md, "object on list [%u] oid 0x%llx is not valid, returned null object id", i, oid);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (md.allowedobjecttypes.find(ot) == md.allowedobjecttypes.end())
        {
            META_LOG_ERROR(md, "object on list [%u] oid 0x%llx object type %d is not allowed on this attribute", i, oid, ot);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (!object_reference_exists(oid))
        {
            META_LOG_ERROR(md, "object on list [%u] oid 0x%llx object type %d does not exists in local DB", i, oid, ot);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (i > 1)
        {
            // currently all objects on list must be the same type
            if (object_type != ot)
            {
                META_LOG_ERROR(md, "object list contain's mixed object types: %d vs %d, not allowed", object_type, ot);

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }

        object_type = ot;
    }

    return SAI_STATUS_SUCCESS;
}

template<typename T> sai_status_t meta_genetic_validation_list(const sai_attr_metadata_t& md, const T&list)
{
    uint32_t count = list.count;

    if (count > MAX_LIST_COUNT)
    {
        META_LOG_ERROR(md, "list count %u is > then max list count %u", count, MAX_LIST_COUNT);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (count == 0 && list.list != NULL)
    {
        META_LOG_ERROR(md, "when count is zero, list must be NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (list.list == NULL)
    {
        if (count == 0)
        {
            return SAI_STATUS_SUCCESS;
        }

        META_LOG_ERROR(md, "list is null, but count is %u", count);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

#define VALIDATION_LIST(md,list) \
{\
    auto status = meta_genetic_validation_list(md,list);\
    if (status != SAI_STATUS_SUCCESS)\
    {\
        return status;\
    }\
}

std::string construct_key(
        _In_ const sai_object_meta_key_t& meta_key,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t* attr_list)
{
    SWSS_LOG_ENTER();

    // use map to make sure that kays will be always sorded by id

    std::map<int32_t, std::string> keys;

    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        const sai_attribute_t* attr = &attr_list[idx];

        const auto& md = *get_attribute_metadata(meta_key.object_type, attr->id);

        const sai_attribute_value_t& value = attr->value;

        if (!HAS_FLAG_KEY(md.flags))
        {
            continue;
        }

        std::string name = std::string(get_attr_name(md.objecttype, md.attrid)) + ":";

        switch (md.serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_UINT32_LIST: // only for port

                // TODO this list should be sorted
                for (uint32_t i = 0; i < value.u32list.count; ++i)
                {
                    name += std::to_string(value.u32list.list[i]);

                    if (i != value.u32list.count - 1)
                    {
                        name += ",";
                    }
                }

                break;

            case SAI_SERIALIZATION_TYPE_INT32:
                name += std::to_string(value.s32); // if enum then get enum name?
                break;

            case SAI_SERIALIZATION_TYPE_UINT32:
                name += std::to_string(value.u32);
                break;

            case SAI_SERIALIZATION_TYPE_UINT8:
                name += std::to_string(value.u8);
                break;

            default:
                META_LOG_ERROR(md, "FATAL: marked as key, but have invalid serialization type");
                throw;
        }

        keys[md.attrid] = name;
    }

    std::string key;

    for (auto& k: keys)
    {
        key += k.second + ";";
    }

    SWSS_LOG_DEBUG("constructed key: %s", key.c_str());

    return key;
}

sai_status_t meta_generic_validation_create(
        _In_ const sai_object_meta_key_t& meta_key,
        _In_ const uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (attr_count > MAX_LIST_COUNT)
    {
        SWSS_LOG_ERROR("create attribute count is too large %u > then max list count %u", attr_count, MAX_LIST_COUNT);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count > 0 && attr_list == NULL)
    {
        SWSS_LOG_ERROR("attr count is %u but attribute list pointer is NULL", attr_count);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::unordered_map<sai_attr_id_t, const sai_attribute_t*> attrs;

    SWSS_LOG_DEBUG("attr count = %u", attr_count);

    bool haskeys = false;

    // check each attribute separetly
    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        const sai_attribute_t* attr = &attr_list[idx];

        auto mdp = get_attribute_metadata(meta_key.object_type, attr->id);

        if (mdp == NULL)
        {
            SWSS_LOG_ERROR("unable to find attribute metadata %d:%d", meta_key.object_type, attr->id);

            return SAI_STATUS_FAILURE;
        }

        const sai_attribute_value_t& value = attr->value;

        const sai_attr_metadata_t& md = *mdp;

        META_LOG_DEBUG(md, "(create)");

        if (attrs.find(attr->id) != attrs.end())
        {
            META_LOG_ERROR(md, "attribute id (%u) is defined on attr list multiple times", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        attrs[attr->id] = attr;

        if (HAS_FLAG_READ_ONLY(md.flags))
        {
            META_LOG_ERROR(md, "attr is read only and cannot be created");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (HAS_FLAG_KEY(md.flags))
        {
            haskeys = true;
            META_LOG_DEBUG(md, "attr is key");
        }

        // if we set OID check if exists and if type is correct

        switch (md.serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_CHARDATA:

                {
                    const char* chardata = value.chardata;

                    size_t len = strnlen(chardata, HOSTIF_NAME_SIZE);

                    if (len == HOSTIF_NAME_SIZE)
                    {
                        META_LOG_ERROR(md, "host interface name is too long");

                        return SAI_STATUS_INVALID_PARAMETER;
                    }

                    if (len == 0)
                    {
                        META_LOG_ERROR(md, "host interface name is zero");

                        return SAI_STATUS_INVALID_PARAMETER;
                    }

                    for (size_t i = 0; i < len; ++i)
                    {
                        char c = chardata[i];

                        if (c < 0x20 || c > 0x7e)
                        {
                            META_LOG_ERROR(md, "interface name contains invalid character 0x%02x", c);

                            return SAI_STATUS_INVALID_PARAMETER;
                        }
                    }

                    // TODO check whether name is not used by other host interface
                    break;
                }

            case SAI_SERIALIZATION_TYPE_BOOL:
            case SAI_SERIALIZATION_TYPE_UINT8:
            case SAI_SERIALIZATION_TYPE_INT8:
            case SAI_SERIALIZATION_TYPE_UINT16:
            case SAI_SERIALIZATION_TYPE_INT16:
            case SAI_SERIALIZATION_TYPE_UINT32:
            case SAI_SERIALIZATION_TYPE_INT32:
            case SAI_SERIALIZATION_TYPE_UINT64:
            case SAI_SERIALIZATION_TYPE_INT64:
            case SAI_SERIALIZATION_TYPE_MAC:
            case SAI_SERIALIZATION_TYPE_IP4:
            case SAI_SERIALIZATION_TYPE_IP6:
                break;

            case SAI_SERIALIZATION_TYPE_IP_ADDRESS:

                {
                    switch (value.ipaddr.addr_family)
                    {
                        case SAI_IP_ADDR_FAMILY_IPV4:
                        case SAI_IP_ADDR_FAMILY_IPV6:
                            break;

                        default:

                            SWSS_LOG_ERROR("invalid address family: %d", value.ipaddr.addr_family);

                            return SAI_STATUS_INVALID_PARAMETER;
                    }

                    break;
                }

            case SAI_SERIALIZATION_TYPE_OBJECT_ID:

                {
                    sai_status_t status = meta_generic_validation_objlist(md, 1, &value.oid);

                    if (status != SAI_STATUS_SUCCESS)
                    {
                        return status;
                    }

                    break;
                }

            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:

                {
                    sai_status_t status = meta_generic_validation_objlist(md, value.objlist.count, value.objlist.list);

                    if (status != SAI_STATUS_SUCCESS)
                    {
                        return status;
                    }

                    break;
                }

                // case SAI_SERIALIZATION_TYPE_VLAN_LIST:
                // require test for vlan existence

                // ACL FIELD

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:

                {
                    sai_status_t status = meta_generic_validation_objlist(md, 1, &value.aclfield.data.oid);

                    if (status != SAI_STATUS_SUCCESS)
                    {
                        return status;
                    }

                    break;
                }

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

                {
                    sai_status_t status = meta_generic_validation_objlist(md, value.aclfield.data.objlist.count, value.aclfield.data.objlist.list);

                    if (status != SAI_STATUS_SUCCESS)
                    {
                        return status;
                    }

                    break;
                }

                // case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:

                // ACL ACTION

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:

                {
                    sai_status_t status = meta_generic_validation_objlist(md, 1, &value.aclaction.parameter.oid);

                    if (status != SAI_STATUS_SUCCESS)
                    {
                        return status;
                    }

                    break;
                }

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

                {
                    sai_status_t status = meta_generic_validation_objlist(md, value.aclaction.parameter.objlist.count, value.aclaction.parameter.objlist.list);

                    if (status != SAI_STATUS_SUCCESS)
                    {
                        return status;
                    }

                    break;
                }

                // ACL END

            case SAI_SERIALIZATION_TYPE_UINT8_LIST:
                VALIDATION_LIST(md, value.u8list);
                break;
            case SAI_SERIALIZATION_TYPE_INT8_LIST:
                VALIDATION_LIST(md, value.s8list);
                break;
            case SAI_SERIALIZATION_TYPE_UINT16_LIST:
                VALIDATION_LIST(md, value.u16list);
                break;
            case SAI_SERIALIZATION_TYPE_INT16_LIST:
                VALIDATION_LIST(md, value.s16list);
                break;
            case SAI_SERIALIZATION_TYPE_UINT32_LIST:
                VALIDATION_LIST(md, value.u32list);
                break;
            case SAI_SERIALIZATION_TYPE_INT32_LIST:
                VALIDATION_LIST(md, value.s32list);
                break;
            case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
                VALIDATION_LIST(md, value.qosmap);
                break;
            case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
                VALIDATION_LIST(md, value.tunnelmap);
                break;

            case SAI_SERIALIZATION_TYPE_UINT32_RANGE:

                if (value.u32range.min > value.u32range.max)
                {
                    META_LOG_ERROR(md, "invalid range %u .. %u", value.u32range.min > value.u32range.max);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                break;

            case SAI_SERIALIZATION_TYPE_INT32_RANGE:

                if (value.s32range.min > value.s32range.max)
                {
                    META_LOG_ERROR(md, "invalid range %u .. %u", value.s32range.min > value.s32range.max);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                break;

            default:

                META_LOG_ERROR(md, "serialization type is not supported yet FIXME");
                throw;
        }

        if (md.isenum())
        {
            int32_t val = value.s32;

            switch (md.serializationtype)
            {
                case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
                    val = value.aclfield.data.s32;
                    break;

                case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
                    val = value.aclaction.parameter.s32;
                    break;

                default:
                    val = value.s32;
                    break;
            }

            if (md.enumallowedvalues.find(val) == md.enumallowedvalues.end())
            {
                META_LOG_ERROR(md, "is enum, but value %d not found on allowed values list", val);

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }

        if (md.isenumlist())
        {
            // we allow repeats on enum list
            if (value.s32list.count != 0 && value.s32list.list == NULL)
            {
                META_LOG_ERROR(md, "enum list is NULL");

                return SAI_STATUS_INVALID_PARAMETER;
            }

            for (uint32_t i = value.s32list.count; i < value.s32list.count; ++i)
            {
                int32_t s32 = value.s32list.list[i];

                if (md.enumallowedvalues.find(s32) == md.enumallowedvalues.end())
                {
                    META_LOG_ERROR(md, "is enum list, but value %d not found on allowed values list", value.s32);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }
        }

        if (md.isvlan())
        {
            if (!vlan_reference_exists(value.u16))
            {
                SWSS_LOG_ERROR("vlan %d is missing", value.u16);

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }

        // conditions are checked later on
    }

    // we are creating object, no need for check if exists (only key values needs to be checked)

    switch (meta_key.object_type)
    {
        case SAI_OBJECT_TYPE_SWITCH:
        case SAI_OBJECT_TYPE_ROUTE:
        case SAI_OBJECT_TYPE_FDB:
        case SAI_OBJECT_TYPE_NEIGHBOR:
        case SAI_OBJECT_TYPE_VLAN:
        case SAI_OBJECT_TYPE_TRAP:

            {
                // just sanity check if object already exists
                std::string key = get_object_meta_key_string(meta_key);

                if (object_exists(key))
                {
                    SWSS_LOG_ERROR("object key %s already exists", key.c_str());

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                break;
            }

        default:
            // we are creating OID object, and we don't have it's value yet
            break;
    }

    const auto& metadata = get_attributes_metadata(meta_key.object_type);

    if (metadata.empty())
    {
        SWSS_LOG_ERROR("get attributes metadata returned empty list for object type: %d", meta_key.object_type);

        return SAI_STATUS_FAILURE;
    }

    // check if all mandatory attrributes were passed

    for (auto mdp: metadata)
    {
        const sai_attr_metadata_t& md = *mdp;

        if (!HAS_FLAG_MANDATORY_ON_CREATE(md.flags))
        {
            continue;
        }

        if (md.isconditional())
        {
            // skip conditional attributes for now
            continue;
        }

        const auto &it = attrs.find(md.attrid);

        if (it == attrs.end())
        {
            META_LOG_ERROR(md, "attribute is mandatory but not passed in attr list");

            return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
        }
    }

    // check if we need any conditional attributes
    for (auto mdp: metadata)
    {
        const sai_attr_metadata_t& md = *mdp;

        if (!md.isconditional())
        {
            continue;
        }

        // this is conditional attribute, check if it's required

        bool any = false;

        for (auto& c : md.conditions)
        {
            // condtions may only be on the same object type
            const auto& cmd = *get_attribute_metadata(meta_key.object_type, c.attrid);

            const sai_attribute_value_t* cvalue = &cmd.defaultvalue;

            const sai_attribute_t *cattr = get_attribute_by_id(c.attrid, attr_count, attr_list);

            if (cattr != NULL)
            {
                META_LOG_DEBUG(md, "condition attr %d was passed, using it's value", c.attrid);

                cvalue = &cattr->value;
            }

            if (cmd.serializationtype == SAI_SERIALIZATION_TYPE_BOOL)
            {
                if (c.condition.booldata == cvalue->booldata)
                {
                    META_LOG_DEBUG(md, "bool condition was met on attr %d = %d", cmd.attrid, c.condition.booldata);

                    any = true;
                    break;
                }
            }
            else // enum condition
            {
                int32_t val = cvalue->s32;

                switch (cmd.serializationtype)
                {
                    case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
                        val = cvalue->aclfield.data.s32;
                        break;

                    case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
                        val = cvalue->aclaction.parameter.s32;
                        break;

                    default:
                        val = cvalue->s32;
                        break;
                }

                if (c.condition.s32 == val)
                {
                    META_LOG_DEBUG(md, "enum condition was met on attr id %d, val = %d", cmd.attrid, val);

                    any = true;
                    break;
                }
            }
        }

        if (!any)
        {
            // maybe we can let it go here?
            if (attrs.find(md.attrid) != attrs.end())
            {
                META_LOG_DEBUG(md, "conditional, but condition was not met, this attribute is not required, but passed");

                return SAI_STATUS_INVALID_PARAMETER;
            }

            continue;
        }

        // is required, check if user passed it
        const auto &it = attrs.find(md.attrid);

        if (it == attrs.end())
        {
            META_LOG_ERROR(md, "attribute is conditional and is mandatory but not passed in attr list");

            return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
        }
    }

    if (haskeys)
    {
        std::string key = construct_key(meta_key, attr_count, attr_list);

        for (auto& it: AttributeKeys)
        {
            // since we didn't created oid yet, we don't know if attribute key exists, check all

            if (it.second == key)
            {
                SWSS_LOG_ERROR("attribute key %s already exists, can't create", key.c_str());

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_generic_validation_remove(
        _In_ const sai_object_meta_key_t& meta_key)
{
    SWSS_LOG_ENTER();

    std::string key = get_object_meta_key_string(meta_key);

    if (!object_exists(key))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (meta_key.object_type)
    {
        case SAI_OBJECT_TYPE_ROUTE:
        case SAI_OBJECT_TYPE_FDB:
        case SAI_OBJECT_TYPE_NEIGHBOR:
            // we don't keep reference of those since those are leafs
            break;

        case SAI_OBJECT_TYPE_SWITCH:
            SWSS_LOG_ERROR("remove switch not supported yet FIXME");
            throw;

        case SAI_OBJECT_TYPE_TRAP:
            SWSS_LOG_ERROR("remove trap not supported yet FIXME");
            throw;

        case SAI_OBJECT_TYPE_VLAN:

            {
                sai_vlan_id_t vlan_id = meta_key.key.vlan_id;

                if (!vlan_reference_exists(vlan_id))
                {
                    SWSS_LOG_ERROR("vlan %u reference don't exists", vlan_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                int count = vlan_reference_count(vlan_id);

                if (count != 0)
                {
                    SWSS_LOG_ERROR("vlan %u reference count is %d, can't remove", vlan_id, count);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                if (vlan_id == DEFAULT_VLAN_NUMBER)
                {
                    SWSS_LOG_ERROR("removing vlan number %u is not supported", DEFAULT_VLAN_NUMBER);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                // should be safe to remove

                break;
            }

        default:

            {
                sai_object_id_t oid = meta_key.key.object_id;

                if (oid == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_ERROR("can't remove null object id");

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                sai_object_type_t object_type = sai_object_type_query(oid);

                if (object_type == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_ERROR("oid 0x%llx is not valid, returned null object id", oid);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                if (object_type != meta_key.object_type)
                {
                    SWSS_LOG_ERROR("oid 0x%llx type %d is not accepted, expected object type %d", oid, object_type, meta_key.object_type);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                if (!object_reference_exists(oid))
                {
                    SWSS_LOG_ERROR("object 0x%llx reference don't exists", oid);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                int count = object_reference_count(oid);

                if (count != 0)
                {
                    SWSS_LOG_ERROR("object 0x%llx reference count is %d, can't remove", oid, count);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                // should be safe to remove

                break;
            }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_generic_validation_set(
        _In_ const sai_object_meta_key_t& meta_key,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute pointer is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto mdp = get_attribute_metadata(meta_key.object_type, attr->id);

    if (mdp == NULL)
    {
        SWSS_LOG_ERROR("unable to find attribute metadata %d:%d", meta_key.object_type, attr->id);

        return SAI_STATUS_FAILURE;
    }

    const sai_attribute_value_t& value = attr->value;

    const sai_attr_metadata_t& md = *mdp;

    META_LOG_DEBUG(md, "(set)");

    if (HAS_FLAG_READ_ONLY(md.flags))
    {
        META_LOG_ERROR(md, "attr is read only and cannot be modified");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (HAS_FLAG_CREATE_ONLY(md.flags))
    {
        META_LOG_ERROR(md, "attr is create only and cannot be modified");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (HAS_FLAG_KEY(md.flags))
    {
        META_LOG_ERROR(md, "attr is key and cannot be modified");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // if we set OID check if exists and if type is correct

    switch (md.serializationtype)
    {
        case SAI_SERIALIZATION_TYPE_BOOL:
            // case SAI_SERIALIZATION_TYPE_CHARDATA:
        case SAI_SERIALIZATION_TYPE_UINT8:
        case SAI_SERIALIZATION_TYPE_INT8:
        case SAI_SERIALIZATION_TYPE_UINT16:
        case SAI_SERIALIZATION_TYPE_INT16:
        case SAI_SERIALIZATION_TYPE_UINT32:
        case SAI_SERIALIZATION_TYPE_INT32:
        case SAI_SERIALIZATION_TYPE_UINT64:
        case SAI_SERIALIZATION_TYPE_INT64:
        case SAI_SERIALIZATION_TYPE_MAC:
        case SAI_SERIALIZATION_TYPE_IP4:
        case SAI_SERIALIZATION_TYPE_IP6:
            // primitives
            break;

        case SAI_SERIALIZATION_TYPE_IP_ADDRESS:

            {
                switch (value.ipaddr.addr_family)
                {
                    case SAI_IP_ADDR_FAMILY_IPV4:
                    case SAI_IP_ADDR_FAMILY_IPV6:
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid address family: %d", value.ipaddr.addr_family);

                        return SAI_STATUS_INVALID_PARAMETER;
                }

                break;
            }

        case SAI_SERIALIZATION_TYPE_OBJECT_ID:

            {
                sai_status_t status = meta_generic_validation_objlist(md, 1, &value.oid);

                if (status != SAI_STATUS_SUCCESS)
                {
                    return status;
                }

                break;
            }

        case SAI_SERIALIZATION_TYPE_OBJECT_LIST:

            {
                sai_status_t status = meta_generic_validation_objlist(md, value.objlist.count, value.objlist.list);

                if (status != SAI_STATUS_SUCCESS)
                {
                    return status;
                }

                break;
            }

            // case SAI_SERIALIZATION_TYPE_VLAN_LIST:
            // will require test vlan existence

            // ACL FIELD

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
            // primitives
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:

            {
                sai_status_t status = meta_generic_validation_objlist(md, 1, &value.aclfield.data.oid);

                if (status != SAI_STATUS_SUCCESS)
                {
                    return status;
                }

                break;
            }

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

            {
                sai_status_t status = meta_generic_validation_objlist(md, value.aclfield.data.objlist.count, value.aclfield.data.objlist.list);

                if (status != SAI_STATUS_SUCCESS)
                {
                    return status;
                }

                break;
            }

            // case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:

            // ACL ACTION
            //
            // NOTE: when we are going to disable action parameter is not
            // needed (we don't need to validate it) also same object can be
            // enabled/disabled without changing oid? this makes things
            // complicated

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:

            {
                sai_status_t status = meta_generic_validation_objlist(md, 1, &value.aclaction.parameter.oid);

                if (status != SAI_STATUS_SUCCESS)
                {
                    return status;
                }

                break;
            }

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

            {
                sai_status_t status = meta_generic_validation_objlist(md, value.aclaction.parameter.objlist.count, value.aclaction.parameter.objlist.list);

                if (status != SAI_STATUS_SUCCESS)
                {
                    return status;
                }

                break;
            }

            // ACL END

        case SAI_SERIALIZATION_TYPE_UINT8_LIST:
            VALIDATION_LIST(md, value.u8list);
            break;
        case SAI_SERIALIZATION_TYPE_INT8_LIST:
            VALIDATION_LIST(md, value.s8list);
            break;
        case SAI_SERIALIZATION_TYPE_UINT16_LIST:
            VALIDATION_LIST(md, value.u16list);
            break;
        case SAI_SERIALIZATION_TYPE_INT16_LIST:
            VALIDATION_LIST(md, value.s16list);
            break;
        case SAI_SERIALIZATION_TYPE_UINT32_LIST:
            VALIDATION_LIST(md, value.u32list);
            break;
        case SAI_SERIALIZATION_TYPE_INT32_LIST:
            VALIDATION_LIST(md, value.s32list);
            break;
        case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
            VALIDATION_LIST(md, value.qosmap);
            break;
        case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
            VALIDATION_LIST(md, value.tunnelmap);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32_RANGE:

            if (value.u32range.min > value.u32range.max)
            {
                META_LOG_ERROR(md, "invalid range %u .. %u", value.u32range.min > value.u32range.max);

                return SAI_STATUS_INVALID_PARAMETER;
            }

            break;

        case SAI_SERIALIZATION_TYPE_INT32_RANGE:

            if (value.s32range.min > value.s32range.max)
            {
                META_LOG_ERROR(md, "invalid range %u .. %u", value.s32range.min > value.s32range.max);

                return SAI_STATUS_INVALID_PARAMETER;
            }

            break;

        default:

            META_LOG_ERROR(md, "serialization type is not supported yet FIXME");
            throw;
    }

    if (md.isenum())
    {
        int32_t val = value.s32;

        switch (md.serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
                val = value.aclfield.data.s32;
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
                val = value.aclaction.parameter.s32;
                break;

            default:
                val = value.s32;
                break;
        }

        if (md.enumallowedvalues.find(val) == md.enumallowedvalues.end())
        {
            META_LOG_ERROR(md, "is enum, but value %d not found on allowed values list", val);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    if (md.isenumlist())
    {
        // we allow repeats on enum list
        if (value.s32list.count != 0 && value.s32list.list == NULL)
        {
            META_LOG_ERROR(md, "enum list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        for (uint32_t i = value.s32list.count; i < value.s32list.count; ++i)
        {
            int32_t s32 = value.s32list.list[i];

            if (md.enumallowedvalues.find(s32) == md.enumallowedvalues.end())
            {
                SWSS_LOG_ERROR("is enum list, but value %d not found on allowed values list", value.s32);

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }
    }

    if (md.isvlan())
    {
        if (!vlan_reference_exists(value.u16))
        {
            SWSS_LOG_ERROR("vlan %d is missing", value.u16);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    if (md.isconditional())
    {
        // check if it was set on local DB
        // (this will not respect create_only with default)
        if (get_object_previous_attr(meta_key, md) == NULL)
        {
            META_LOG_ERROR(md, "set for conditional, but not found in local db");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        META_LOG_DEBUG(md, "conditional attr found in local db");
    }

    // check if object on which we perform operation exists

    std::string key = get_object_meta_key_string(meta_key);

    if (!object_exists(key))
    {
        META_LOG_ERROR(md, "object key %s don't exists", key.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // object exists in DB so we can do "set" operation

    switch (meta_key.object_type)
    {
        case SAI_OBJECT_TYPE_SWITCH:
        case SAI_OBJECT_TYPE_ROUTE:
        case SAI_OBJECT_TYPE_FDB:
        case SAI_OBJECT_TYPE_NEIGHBOR:
        case SAI_OBJECT_TYPE_VLAN:
        case SAI_OBJECT_TYPE_TRAP:

            SWSS_LOG_DEBUG("object key exists: %s", key.c_str());
            break;

        default:

            {
                // check if object we are calling SET is the same object type
                // as the type of SET function

                sai_object_id_t oid = meta_key.key.object_id;

                sai_object_type_t object_type = sai_object_type_query(oid);

                if (object_type == SAI_NULL_OBJECT_ID)
                {
                    META_LOG_ERROR(md, "oid 0x%llx is not valid, returned null object id", oid);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                if (object_type != meta_key.object_type)
                {
                    META_LOG_ERROR(md, "oid 0x%llx type %d is not accepted, expected object type %d", oid, object_type, meta_key.object_type);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_generic_validation_get(
        _In_ const sai_object_meta_key_t& meta_key,
        _In_ const uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("expected at least 1 attribute when calling get, zero given");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count > MAX_LIST_COUNT)
    {
        SWSS_LOG_ERROR("get attribute count is too large %u > then max list count %u", attr_count, MAX_LIST_COUNT);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list pointer is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    SWSS_LOG_DEBUG("attr count = %u", attr_count);

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        const sai_attribute_t* attr = &attr_list[i];

        auto mdp = get_attribute_metadata(meta_key.object_type, attr->id);

        if (mdp == NULL)
        {
            SWSS_LOG_ERROR("unable to find attribute metadata %d:%d", meta_key.object_type, attr->id);

            return SAI_STATUS_FAILURE;
        }

        const sai_attribute_value_t& value = attr->value;

        const sai_attr_metadata_t& md = *mdp;

        META_LOG_DEBUG(md, "(get)");

        if (md.isconditional())
        {
            // check if it was set on local DB
            // (this will not respect create_only with default)
            if (get_object_previous_attr(meta_key, md) == NULL)
            {
                META_LOG_ERROR(md, "request for conditional, but not found in local db");

                return SAI_STATUS_INVALID_PARAMETER;
            }

            META_LOG_DEBUG(md, "conditional attr found in local db");
        }

        switch (md.serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_BOOL:
            case SAI_SERIALIZATION_TYPE_CHARDATA:
            case SAI_SERIALIZATION_TYPE_UINT8:
            case SAI_SERIALIZATION_TYPE_INT8:
            case SAI_SERIALIZATION_TYPE_UINT16:
            case SAI_SERIALIZATION_TYPE_INT16:
            case SAI_SERIALIZATION_TYPE_UINT32:
            case SAI_SERIALIZATION_TYPE_INT32:
            case SAI_SERIALIZATION_TYPE_UINT64:
            case SAI_SERIALIZATION_TYPE_INT64:
            case SAI_SERIALIZATION_TYPE_MAC:
            case SAI_SERIALIZATION_TYPE_IP4:
            case SAI_SERIALIZATION_TYPE_IP6:
            case SAI_SERIALIZATION_TYPE_IP_ADDRESS:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_ID:
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                VALIDATION_LIST(md, value.objlist);
                break;

            case SAI_SERIALIZATION_TYPE_VLAN_LIST:

                {
                    if (value.vlanlist.count == 0 && value.vlanlist.list != NULL)
                    {
                        META_LOG_ERROR(md, "vlan list count is zero, but list not NULL");

                        return SAI_STATUS_INVALID_PARAMETER;
                    }

                    if (value.vlanlist.count != 0 && value.vlanlist.list == NULL)
                    {
                        META_LOG_ERROR(md, "vlan list count is %u, but list is NULL", value.vlanlist.count);

                        return SAI_STATUS_INVALID_PARAMETER;
                    }

                    if (value.vlanlist.count > MAXIMUM_VLAN_NUMBER)
                    {
                        META_LOG_ERROR(md, "vlan count is too big %u > %u", value.vlanlist.count, MAXIMUM_VLAN_NUMBER);

                        return SAI_STATUS_INVALID_PARAMETER;
                    }

                    break;
                }

                // ACL FIELD

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                VALIDATION_LIST(md, value.aclfield.data.objlist);
                break;

                // case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:

                // ACL ACTION

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                VALIDATION_LIST(md, value.aclaction.parameter.objlist);
                break;

                // ACL END

            case SAI_SERIALIZATION_TYPE_UINT8_LIST:
                VALIDATION_LIST(md, value.u8list);
                break;
            case SAI_SERIALIZATION_TYPE_INT8_LIST:
                VALIDATION_LIST(md, value.s8list);
                break;
            case SAI_SERIALIZATION_TYPE_UINT16_LIST:
                VALIDATION_LIST(md, value.u16list);
                break;
            case SAI_SERIALIZATION_TYPE_INT16_LIST:
                VALIDATION_LIST(md, value.s16list);
                break;
            case SAI_SERIALIZATION_TYPE_UINT32_LIST:
                VALIDATION_LIST(md, value.u32list);
                break;
            case SAI_SERIALIZATION_TYPE_INT32_LIST:
                VALIDATION_LIST(md, value.s32list);
                break;
            case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
                VALIDATION_LIST(md, value.qosmap);
                break;
            case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
                VALIDATION_LIST(md, value.tunnelmap);
                break;

            case SAI_SERIALIZATION_TYPE_UINT32_RANGE:
            case SAI_SERIALIZATION_TYPE_INT32_RANGE:
                // primitives
                break;

            default:

                // acl capability will is more complex since is in/out we need to check stage

                META_LOG_ERROR(md, "serialization type is not supported yet FIXME");
                throw;
        }
    }

    std::string key = get_object_meta_key_string(meta_key);

    if (!object_exists(key))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (meta_key.object_type)
    {
        case SAI_OBJECT_TYPE_SWITCH:
        case SAI_OBJECT_TYPE_ROUTE:
        case SAI_OBJECT_TYPE_FDB:
        case SAI_OBJECT_TYPE_NEIGHBOR:
        case SAI_OBJECT_TYPE_VLAN:
        case SAI_OBJECT_TYPE_TRAP:

            SWSS_LOG_DEBUG("object key exists: %s", key.c_str());

            break;

        default:

            {
                // check if object we are calling GET is the same object type
                // as the type of GET function

                sai_object_id_t oid = meta_key.key.object_id;

                sai_object_type_t object_type = sai_object_type_query(oid);

                if (object_type == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_ERROR("oid 0x%llx is not valid, returned null object id", oid);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                if (object_type != meta_key.object_type)
                {
                    SWSS_LOG_ERROR("oid 0x%llx type %d is not accepted, expected object type %d", oid, object_type, meta_key.object_type);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;
    }

    // object exists in DB so we can do "get" operation

    return SAI_STATUS_SUCCESS;
}

void meta_generic_validation_post_create(
        _In_ const sai_object_meta_key_t& meta_key,
        _In_ const uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string key = get_object_meta_key_string(meta_key);

    if (object_exists(key))
    {
        SWSS_LOG_ERROR("object key %s already exists (vendor bug?)", key.c_str());

        // this may produce inconsistency
    }

    create_object(meta_key);

    switch (meta_key.object_type)
    {
        case SAI_OBJECT_TYPE_ROUTE:
            object_reference_inc(meta_key.key.route_entry.vr_id);
            break;

        case SAI_OBJECT_TYPE_NEIGHBOR:
            object_reference_inc(meta_key.key.neighbor_entry.rif_id);
            break;

        case SAI_OBJECT_TYPE_FDB:
            vlan_reference_inc(meta_key.key.fdb_entry.vlan_id);
            break;

        case SAI_OBJECT_TYPE_VLAN:
            vlan_reference_insert(meta_key.key.vlan_id);
            break;

        case SAI_OBJECT_TYPE_SWITCH:
        case SAI_OBJECT_TYPE_TRAP:
            SWSS_LOG_ERROR("object not supported FIXME");
            throw;

        default:

            {
                // check if object created was expected type
                // as the type of SET function

                sai_object_id_t oid = meta_key.key.object_id;

                if (oid == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_ERROR("created oid is null object id (vendor bug?)");
                    break;
                }

                sai_object_type_t object_type = sai_object_type_query(oid);

                if (object_type == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_ERROR("created oid 0x%llx is not valid object type after create, returned null object id (vendor bug?)", oid);
                    break;
                }

                if (object_type != meta_key.object_type)
                {
                    SWSS_LOG_ERROR("created oid 0x%llx type %d is wrond type, expected object type %d (vendor bug?)", oid, object_type, meta_key.object_type);
                    break;
                }

                object_reference_insert(oid);

                break;
            }
    }

    bool haskeys;

    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        const sai_attribute_t* attr = &attr_list[idx];

        auto mdp = get_attribute_metadata(meta_key.object_type, attr->id);

        const sai_attribute_value_t& value = attr->value;

        const sai_attr_metadata_t& md = *mdp;

        if (HAS_FLAG_KEY(md.flags))
        {
            haskeys = true;
            META_LOG_DEBUG(md, "attr is key");
        }

        // increase reference on object id types

        switch (md.serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_BOOL:
            case SAI_SERIALIZATION_TYPE_CHARDATA:
            case SAI_SERIALIZATION_TYPE_UINT8:
            case SAI_SERIALIZATION_TYPE_INT8:
            case SAI_SERIALIZATION_TYPE_UINT16:
            case SAI_SERIALIZATION_TYPE_INT16:
            case SAI_SERIALIZATION_TYPE_UINT32:
            case SAI_SERIALIZATION_TYPE_INT32:
            case SAI_SERIALIZATION_TYPE_UINT64:
            case SAI_SERIALIZATION_TYPE_INT64:
            case SAI_SERIALIZATION_TYPE_MAC:
            case SAI_SERIALIZATION_TYPE_IP4:
            case SAI_SERIALIZATION_TYPE_IP6:
            case SAI_SERIALIZATION_TYPE_IP_ADDRESS:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_ID:
                object_reference_inc(value.oid);
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                object_reference_inc(value.objlist);
                break;

            case SAI_SERIALIZATION_TYPE_VLAN_LIST:
                break;

                // ACL FIELD

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                object_reference_inc(value.aclfield.data.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                object_reference_inc(value.aclfield.data.objlist);
                break;

                // case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:

                // ACL ACTION

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                object_reference_inc(value.aclaction.parameter.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                object_reference_inc(value.aclaction.parameter.objlist);
                break;

                // ACL END

            case SAI_SERIALIZATION_TYPE_UINT8_LIST:
            case SAI_SERIALIZATION_TYPE_INT8_LIST:
            case SAI_SERIALIZATION_TYPE_UINT16_LIST:
            case SAI_SERIALIZATION_TYPE_INT16_LIST:
            case SAI_SERIALIZATION_TYPE_UINT32_LIST:
            case SAI_SERIALIZATION_TYPE_INT32_LIST:
            case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
            case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
            case SAI_SERIALIZATION_TYPE_UINT32_RANGE:
            case SAI_SERIALIZATION_TYPE_INT32_RANGE:
                // no special action required
                break;

            default:

                META_LOG_ERROR(md, "serialization type is not supported yet FIXME");
                throw;
        }

        if (md.isvlan())
        {
            vlan_reference_inc(value.u16);
        }

        set_object(meta_key, md, attr);
    }

    if (haskeys)
    {
        std::string ok = get_object_meta_key_string(meta_key);

        AttributeKeys[ok] = construct_key(meta_key, attr_count, attr_list);
    }
}

void meta_generic_validation_post_remove(
        _In_ const sai_object_meta_key_t& meta_key)
{
    SWSS_LOG_ENTER();

    // get all attributes that was set

    for (auto&it: get_object(meta_key))
    {
        const sai_attribute_t* attr = it->getattr();

        auto mdp = get_attribute_metadata(meta_key.object_type, attr->id);

        const sai_attribute_value_t& value = attr->value;

        const sai_attr_metadata_t& md = *mdp;

        // decrease reference on object id types

        switch (md.serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_BOOL:
            case SAI_SERIALIZATION_TYPE_CHARDATA:
            case SAI_SERIALIZATION_TYPE_UINT8:
            case SAI_SERIALIZATION_TYPE_INT8:
            case SAI_SERIALIZATION_TYPE_UINT16:
            case SAI_SERIALIZATION_TYPE_INT16:
            case SAI_SERIALIZATION_TYPE_UINT32:
            case SAI_SERIALIZATION_TYPE_INT32:
            case SAI_SERIALIZATION_TYPE_UINT64:
            case SAI_SERIALIZATION_TYPE_INT64:
            case SAI_SERIALIZATION_TYPE_MAC:
            case SAI_SERIALIZATION_TYPE_IP4:
            case SAI_SERIALIZATION_TYPE_IP6:
            case SAI_SERIALIZATION_TYPE_IP_ADDRESS:
                // primitives, ok
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_ID:
                object_reference_dec(value.oid);
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                object_reference_dec(value.objlist);
                break;

                //case SAI_SERIALIZATION_TYPE_VLAN_LIST:
                // will require dec vlan references

                // ACL FIELD

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                object_reference_dec(value.aclfield.data.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                object_reference_dec(value.aclfield.data.objlist);
                break;

                // case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:

                // ACL ACTION

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                object_reference_dec(value.aclaction.parameter.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                object_reference_dec(value.aclaction.parameter.objlist);
                break;

                // ACL END

            case SAI_SERIALIZATION_TYPE_UINT8_LIST:
            case SAI_SERIALIZATION_TYPE_INT8_LIST:
            case SAI_SERIALIZATION_TYPE_UINT16_LIST:
            case SAI_SERIALIZATION_TYPE_INT16_LIST:
            case SAI_SERIALIZATION_TYPE_UINT32_LIST:
            case SAI_SERIALIZATION_TYPE_INT32_LIST:
            case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
            case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
            case SAI_SERIALIZATION_TYPE_UINT32_RANGE:
            case SAI_SERIALIZATION_TYPE_INT32_RANGE:
                // no special action required
                break;

            default:
                META_LOG_ERROR(md, "serialization type is not supported yet FIXME");
                throw;
        }

        if (md.isvlan())
        {
            vlan_reference_dec(value.u16);
        }
    }

    // we don't keep track of fdb, neighbor, route since
    // those are safe to remove any time (leafs)

    switch (meta_key.object_type)
    {
        case SAI_OBJECT_TYPE_SWITCH:
            SWSS_LOG_ERROR("remove switch not supported yet FIXME");
            throw;

        case SAI_OBJECT_TYPE_ROUTE:
            object_reference_dec(meta_key.key.route_entry.vr_id);
            break;

        case SAI_OBJECT_TYPE_NEIGHBOR:
            object_reference_dec(meta_key.key.neighbor_entry.rif_id);
            break;

        case SAI_OBJECT_TYPE_FDB:
            vlan_reference_dec(meta_key.key.fdb_entry.vlan_id);
            break;

        case SAI_OBJECT_TYPE_TRAP:
            SWSS_LOG_ERROR("trap remove not supported yet FIXME");
            throw;

        case SAI_OBJECT_TYPE_VLAN:
            vlan_reference_remove(meta_key.key.vlan_id);
            break;

        default:
            object_reference_remove(meta_key.key.object_id);
            break;
    }

    remove_object(meta_key);

    std::string ok = get_object_meta_key_string(meta_key);

    if (AttributeKeys.find(ok) != AttributeKeys.end())
    {
        SWSS_LOG_DEBUG("erasing attributes key %s", AttributeKeys[ok].c_str());

        AttributeKeys.erase(ok);
    }
}

void meta_generic_validation_post_set(
        _In_ const sai_object_meta_key_t& meta_key,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto mdp = get_attribute_metadata(meta_key.object_type, attr->id);

    const sai_attribute_value_t& value = attr->value;

    const sai_attr_metadata_t& md = *mdp;

    switch (md.serializationtype)
    {
        case SAI_SERIALIZATION_TYPE_BOOL:
        case SAI_SERIALIZATION_TYPE_CHARDATA:
        case SAI_SERIALIZATION_TYPE_UINT8:
        case SAI_SERIALIZATION_TYPE_INT8:
        case SAI_SERIALIZATION_TYPE_UINT16:
        case SAI_SERIALIZATION_TYPE_INT16:
        case SAI_SERIALIZATION_TYPE_UINT32:
        case SAI_SERIALIZATION_TYPE_INT32:
        case SAI_SERIALIZATION_TYPE_UINT64:
        case SAI_SERIALIZATION_TYPE_INT64:
        case SAI_SERIALIZATION_TYPE_MAC:
        case SAI_SERIALIZATION_TYPE_IP4:
        case SAI_SERIALIZATION_TYPE_IP6:
        case SAI_SERIALIZATION_TYPE_IP_ADDRESS:
            // primitives, ok
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_ID:

            {
                const sai_attribute_t *previous_attr = get_object_previous_attr(meta_key, md);

                if (previous_attr != NULL)
                {
                    // decrease previous if it was set
                    object_reference_dec(previous_attr->value.oid);
                }

                object_reference_inc(value.oid);

                break;
            }

        case SAI_SERIALIZATION_TYPE_OBJECT_LIST:

            {
                const sai_attribute_t *previous_attr = get_object_previous_attr(meta_key, md);

                if (previous_attr != NULL)
                {
                    // decrease previous if it was set
                    object_reference_dec(previous_attr->value.objlist);
                }

                object_reference_inc(value.objlist);

                break;
            }

            // case SAI_SERIALIZATION_TYPE_VLAN_LIST:
            // will require increase vlan references

            // ACL FIELD

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:

            {
                const sai_attribute_t *previous_attr = get_object_previous_attr(meta_key, md);

                if (previous_attr != NULL)
                {
                    // decrease previous if it was set
                    object_reference_dec(previous_attr->value.aclfield.data.oid);
                }

                object_reference_inc(value.aclfield.data.oid);

                break;
            }

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

            {
                const sai_attribute_t *previous_attr = get_object_previous_attr(meta_key, md);

                if (previous_attr != NULL)
                {
                    // decrease previous if it was set
                    object_reference_dec(previous_attr->value.aclfield.data.objlist);
                }

                object_reference_inc(value.aclfield.data.objlist);

                break;
            }

            // case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:

            // ACL ACTION

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
            // primitives
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:

            {
                const sai_attribute_t *previous_attr = get_object_previous_attr(meta_key, md);

                if (previous_attr != NULL)
                {
                    // decrease previous if it was set
                    object_reference_dec(previous_attr->value.aclaction.parameter.oid);
                }

                object_reference_inc(value.aclaction.parameter.oid);
                break;
            }

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

            {
                const sai_attribute_t *previous_attr = get_object_previous_attr(meta_key, md);

                if (previous_attr != NULL)
                {
                    // decrease previous if it was set
                    object_reference_dec(previous_attr->value.aclaction.parameter.objlist);
                }

                object_reference_inc(value.aclaction.parameter.objlist);

                break;
            }

            // ACL END

        case SAI_SERIALIZATION_TYPE_UINT8_LIST:
        case SAI_SERIALIZATION_TYPE_INT8_LIST:
        case SAI_SERIALIZATION_TYPE_UINT16_LIST:
        case SAI_SERIALIZATION_TYPE_INT16_LIST:
        case SAI_SERIALIZATION_TYPE_UINT32_LIST:
        case SAI_SERIALIZATION_TYPE_INT32_LIST:
        case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
        case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
        case SAI_SERIALIZATION_TYPE_UINT32_RANGE:
        case SAI_SERIALIZATION_TYPE_INT32_RANGE:
            // no special action required
            break;

        default:
            META_LOG_ERROR(md, "serialization type is not supported yet FIXME");
            throw;
    }

    if (md.isvlan())
    {
        const sai_attribute_t *previous_attr = get_object_previous_attr(meta_key, md);

        if (previous_attr != NULL)
        {
            // decrease previous if it was set
            vlan_reference_dec(previous_attr->value.u16);
        }

        vlan_reference_inc(value.u16);
    }

    // only on create we need to increase entry object types members
    // save actual attributes and values to local db

    set_object(meta_key, md, attr);
}

void meta_generic_validation_post_get_objlist(
        _In_ const sai_object_meta_key_t& meta_key,
        _In_ const sai_attr_metadata_t& md,
        _In_ uint32_t count,
        _In_ const sai_object_id_t* list)
{
    SWSS_LOG_ENTER();

    if (count > MAX_LIST_COUNT)
    {
        META_LOG_ERROR(md, "returned get object list count %u is > then max list count %u", count, MAX_LIST_COUNT);
    }

    if (list == NULL)
    {
        // query was for length
        return;
    }

    std::set<sai_object_id_t> oids;

    for (uint32_t i = 0; i < count; ++i)
    {
        sai_object_id_t oid = list[i];

        if (oids.find(oid) != oids.end())
        {
            META_LOG_ERROR(md, "returned get object on list [%u] is duplicated, but not allowed", i);
            continue;
        }

        oids.insert(oid);

        if (oid == SAI_NULL_OBJECT_ID)
        {
            if (md.allownullobjectid)
            {
                // ok, null object is allowed
                continue;
            }

            META_LOG_ERROR(md, "returned get object on list [%u] is NULL, but not allowed", i);
            continue;
        }

        sai_object_type_t ot = sai_object_type_query(oid);

        if (ot == SAI_NULL_OBJECT_ID)
        {
            META_LOG_ERROR(md, "returned get object on list [%u] oid 0x%llx is not valid, returned null object id", i, oid);
            continue;
        }

        if (md.allowedobjecttypes.find(ot) == md.allowedobjecttypes.end())
        {
            META_LOG_ERROR(md, "returned get object on list [%u] oid 0x%llx object type %d is not allowed on this attribute", i, oid, ot);
        }

        if (!object_reference_exists(oid))
        {
            // NOTE: there may happen that user will request multiple object lists
            // and first list was retrived ok, but second failed with overflow
            // then we may forget to snoop

            META_LOG_INFO(md, "returned get object on list [%u] oid 0x%llx object type %d does not exists in local DB (snoop)", i, oid, ot);

            sai_object_meta_key_t key = { .object_type = ot, .key = { .object_id = oid } };

            object_reference_insert(oid);

            if (!object_exists(key))
            {
                create_object(key);
            }
        }
    }
}

#define VALIDATION_LIST_GET(md, list) \
{\
    if (list.count > MAX_LIST_COUNT)\
    {\
        META_LOG_ERROR(md, "list count %u is > then max list count %u", list.count, MAX_LIST_COUNT);\
    }\
}

void meta_generic_validation_post_get(
        _In_ const sai_object_meta_key_t& meta_key,
        _In_ const uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        const sai_attribute_t* attr = &attr_list[idx];

        auto mdp = get_attribute_metadata(meta_key.object_type, attr->id);

        const sai_attribute_value_t& value = attr->value;

        const sai_attr_metadata_t& md = *mdp;

        switch (md.serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_BOOL:
            case SAI_SERIALIZATION_TYPE_CHARDATA:
            case SAI_SERIALIZATION_TYPE_UINT8:
            case SAI_SERIALIZATION_TYPE_INT8:
            case SAI_SERIALIZATION_TYPE_UINT16:
            case SAI_SERIALIZATION_TYPE_INT16:
            case SAI_SERIALIZATION_TYPE_UINT32:
            case SAI_SERIALIZATION_TYPE_INT32:
            case SAI_SERIALIZATION_TYPE_UINT64:
            case SAI_SERIALIZATION_TYPE_INT64:
            case SAI_SERIALIZATION_TYPE_MAC:
            case SAI_SERIALIZATION_TYPE_IP4:
            case SAI_SERIALIZATION_TYPE_IP6:
            case SAI_SERIALIZATION_TYPE_IP_ADDRESS:
                // primitives, ok
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_ID:
                meta_generic_validation_post_get_objlist(meta_key, md, 1, &value.oid);
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                meta_generic_validation_post_get_objlist(meta_key, md, value.objlist.count, value.objlist.list);
                break;

            case SAI_SERIALIZATION_TYPE_VLAN_LIST:

                {
                    uint32_t count = value.vlanlist.count;

                    if (count > MAXIMUM_VLAN_NUMBER)
                    {
                        META_LOG_ERROR(md, "too many vlans returned on vlan list (vendor bug?)");
                    }

                    if (value.vlanlist.list == NULL)
                    {
                        break;
                    }

                    for (uint32_t i = 0; i < count; ++i)
                    {
                        sai_vlan_id_t vlan_id = value.vlanlist.list[i];

                        sai_object_meta_key_t meta_key_vlan = { .object_type = SAI_OBJECT_TYPE_VLAN, .key = { .vlan_id = vlan_id } };

                        std::string key_vlan = get_object_meta_key_string(meta_key_vlan);

                        if (object_exists(key_vlan))
                        {
                            continue;
                        }

                        META_LOG_ERROR(md, "vlan id %d not exists, but returned on list [%u] (snoop?)", vlan_id, i);
                    }

                    break;
                }

                // ACL FIELD

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                meta_generic_validation_post_get_objlist(meta_key, md, 1, &value.aclfield.data.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                meta_generic_validation_post_get_objlist(meta_key, md, value.aclfield.data.objlist.count, value.aclfield.data.objlist.list);
                break;

                // case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST: (2 lists)

                // ACL ACTION

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
                // primitives
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                meta_generic_validation_post_get_objlist(meta_key, md, 1, &value.aclaction.parameter.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                meta_generic_validation_post_get_objlist(meta_key, md, value.aclaction.parameter.objlist.count, value.aclaction.parameter.objlist.list);
                break;

                // ACL END

            case SAI_SERIALIZATION_TYPE_UINT8_LIST:
                VALIDATION_LIST_GET(md, value.u8list);
                break;
            case SAI_SERIALIZATION_TYPE_INT8_LIST:
                VALIDATION_LIST_GET(md, value.s8list);
                break;
            case SAI_SERIALIZATION_TYPE_UINT16_LIST:
                VALIDATION_LIST_GET(md, value.u16list);
                break;
            case SAI_SERIALIZATION_TYPE_INT16_LIST:
                VALIDATION_LIST_GET(md, value.s16list);
                break;
            case SAI_SERIALIZATION_TYPE_UINT32_LIST:
                VALIDATION_LIST_GET(md, value.u32list);
                break;
            case SAI_SERIALIZATION_TYPE_INT32_LIST:
                VALIDATION_LIST_GET(md, value.s32list);
                break;
            case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
                VALIDATION_LIST_GET(md, value.qosmap);
                break;
            case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
                VALIDATION_LIST_GET(md, value.tunnelmap);
                break;

            case SAI_SERIALIZATION_TYPE_UINT32_RANGE:

                if (value.u32range.min > value.u32range.max)
                {
                    META_LOG_ERROR(md, "invalid range %u .. %u", value.u32range.min > value.u32range.max);
                }

                break;

            case SAI_SERIALIZATION_TYPE_INT32_RANGE:

                if (value.s32range.min > value.s32range.max)
                {
                    META_LOG_ERROR(md, "invalid range %u .. %u", value.s32range.min > value.s32range.max);
                }

                break;

            default:

                META_LOG_ERROR(md, "serialization type is not supported yet FIXME");
                throw;
        }

        if (md.isenum())
        {
            int32_t val = value.s32;

            switch (md.serializationtype)
            {
                case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
                    val = value.aclfield.data.s32;
                    break;

                case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
                    val = value.aclaction.parameter.s32;
                    break;

                default:
                    val = value.s32;
                    break;
            }

            if (md.enumallowedvalues.find(val) == md.enumallowedvalues.end())
            {
                META_LOG_ERROR(md, "is enum, but value %d not found on allowed values list", val);
                continue;
            }
        }

        if (md.isenumlist())
        {
            if (value.s32list.list == NULL)
            {
                continue;
            }

            for (uint32_t i = value.s32list.count; i < value.s32list.count; ++i)
            {
                int32_t s32 = value.s32list.list[i];

                if (md.enumallowedvalues.find(s32) == md.enumallowedvalues.end())
                {
                    META_LOG_ERROR(md, "is enum list, but value %d not found on allowed values list", value.s32);
                }
            }
        }

        if (md.isvlan())
        {
            if (value.u16 < MINIMUM_VLAN_NUMBER || value.u16 > MAXIMUM_VLAN_NUMBER)
            {
                META_LOG_ERROR(md, "get returned invalid vlan %d", value.u16);
            }
        }
    }
}

// SWITCH

sai_status_t meta_sai_set_switch(
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_switch_attribute_fn set)
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_SWITCH, .key = { } };

    status = meta_generic_validation_set(meta_key, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    //status = meta_pre_set_switch(attr);

    //if (status != SAI_STATUS_SUCCESS)
    //{
    //    return status;
    //}

    if (set == NULL)
    {
        SWSS_LOG_ERROR("set function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = set(attr);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("set status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("set status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_set(meta_key, attr);
    }

    return status;
}

sai_status_t meta_sai_get_switch(
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_switch_attribute_fn get)
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_SWITCH, .key = { } };

    status = meta_generic_validation_get(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (get == NULL)
    {
        SWSS_LOG_ERROR("get function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = get(attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("get status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("get status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_get(meta_key, attr_count, attr_list);
    }

    return status;
}

// FDB ENTRY

sai_status_t meta_sai_validate_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ bool create)
{
    SWSS_LOG_ENTER();

    if (fdb_entry == NULL)
    {
        SWSS_LOG_ERROR("fdb_entry pointer is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_vlan_id_t vlan_id = fdb_entry->vlan_id;

    if (vlan_id < MINIMUM_VLAN_NUMBER || vlan_id > MAXIMUM_VLAN_NUMBER)
    {
        SWSS_LOG_ERROR("invalid vlan number %d expected <%d..%d>", vlan_id, MINIMUM_VLAN_NUMBER, MAXIMUM_VLAN_NUMBER);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // check if vlan exists

    sai_object_meta_key_t meta_key_vlan = { .object_type = SAI_OBJECT_TYPE_VLAN, .key = { .vlan_id = vlan_id } };

    std::string key_vlan = get_object_meta_key_string(meta_key_vlan);

    if (!object_exists(key_vlan))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key_vlan.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // check if fdb entry exists

    sai_object_meta_key_t meta_key_fdb = { .object_type = SAI_OBJECT_TYPE_FDB, .key = { .fdb_entry = *fdb_entry } };

    std::string key_fdb = get_object_meta_key_string(meta_key_fdb);

    if (create)
    {
        if (object_exists(key_fdb))
        {
            SWSS_LOG_ERROR("object key %s already exists", key_fdb.c_str());

            return SAI_STATUS_INVALID_PARAMETER;
        }

        return SAI_STATUS_SUCCESS;
    }

    // set, get, remove

    if (!object_exists(key_fdb))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key_fdb.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // fdb entry is valid

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_sai_create_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_fdb_entry_fn create)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_fdb_entry(fdb_entry, true);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_FDB, .key = { .fdb_entry = *fdb_entry  } };

    status = meta_generic_validation_create(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (create == NULL)
    {
        SWSS_LOG_ERROR("create function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = create(fdb_entry, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("create status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("create status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_create(meta_key, attr_count, attr_list);
    }

    return status;
}

sai_status_t meta_sai_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ sai_remove_fdb_entry_fn remove)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_fdb_entry(fdb_entry, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_FDB, .key = { .fdb_entry = *fdb_entry  } };

    status = meta_generic_validation_remove(meta_key);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (remove == NULL)
    {
        SWSS_LOG_ERROR("remove function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = remove(fdb_entry);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("remove status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("remove status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_remove(meta_key);
    }

    return status;
}

sai_status_t meta_sai_set_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_fdb_entry_attribute_fn set)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_fdb_entry(fdb_entry, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_FDB, .key = { .fdb_entry = *fdb_entry  } };

    status = meta_generic_validation_set(meta_key, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (set == NULL)
    {
        SWSS_LOG_ERROR("set function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = set(fdb_entry, attr);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("set status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("set status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_set(meta_key, attr);
    }

    return status;
}

sai_status_t meta_sai_get_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_fdb_entry_attribute_fn get)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_fdb_entry(fdb_entry, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_FDB, .key = { .fdb_entry = *fdb_entry } };

    status = meta_generic_validation_get(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (get == NULL)
    {
        SWSS_LOG_ERROR("get function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = get(fdb_entry, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("get status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("get status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_get(meta_key, attr_count, attr_list);
    }

    return status;
}

// NEIGHBOR ENTRY

sai_status_t meta_sai_validate_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ bool create)
{
    SWSS_LOG_ENTER();

    if (neighbor_entry == NULL)
    {
        SWSS_LOG_ERROR("neighbor_entry pointer is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (neighbor_entry->ip_address.addr_family)
    {
        case SAI_IP_ADDR_FAMILY_IPV4:
        case SAI_IP_ADDR_FAMILY_IPV6:
            break;

        default:

            SWSS_LOG_ERROR("invalid address family: %d", neighbor_entry->ip_address.addr_family);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_id_t rif = neighbor_entry->rif_id;

    if (rif == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("router interface is set to null object id");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t object_type = sai_object_type_query(rif);

    if (object_type == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("router interface oid 0x%llx is not valid object type, returned null object id", rif);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t expected = SAI_OBJECT_TYPE_ROUTER_INTERFACE;

    if (object_type != expected)
    {
        SWSS_LOG_ERROR("router interface oid 0x%llx type %d is wrond type, expected object type %d", rif, object_type, expected);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // check if router interface exists

    sai_object_meta_key_t meta_key_rif = { .object_type = expected, .key = { .object_id = rif } };

    std::string key_rif = get_object_meta_key_string(meta_key_rif);

    if (!object_exists(key_rif))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key_rif.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_meta_key_t meta_key_neighbor = { .object_type = SAI_OBJECT_TYPE_NEIGHBOR, .key = { .neighbor_entry = *neighbor_entry } };

    std::string key_neighbor = get_object_meta_key_string(meta_key_neighbor);

    if (create)
    {
        if (object_exists(key_neighbor))
        {
            SWSS_LOG_ERROR("object key %s already exists", key_neighbor.c_str());

            return SAI_STATUS_INVALID_PARAMETER;
        }

        return SAI_STATUS_SUCCESS;
    }

    // set, get, remove

    if (!object_exists(key_neighbor))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key_neighbor.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // neighbor entry is valid

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_sai_create_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_neighbor_entry_fn create)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_neighbor_entry(neighbor_entry, true);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_NEIGHBOR, .key = { .neighbor_entry = *neighbor_entry  } };

    status = meta_generic_validation_create(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (create == NULL)
    {
        SWSS_LOG_ERROR("create function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = create(neighbor_entry, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("create status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("create status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_create(meta_key, attr_count, attr_list);
    }

    return status;
}

sai_status_t meta_sai_remove_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ sai_remove_neighbor_entry_fn remove)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_neighbor_entry(neighbor_entry, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_NEIGHBOR, .key = { .neighbor_entry = *neighbor_entry  } };

    status = meta_generic_validation_remove(meta_key);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (remove == NULL)
    {
        SWSS_LOG_ERROR("remove function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = remove(neighbor_entry);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("remove status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("remove status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_remove(meta_key);
    }

    return status;
}

sai_status_t meta_sai_set_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_neighbor_attribute_fn set)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_neighbor_entry(neighbor_entry, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_NEIGHBOR, .key = { .neighbor_entry = *neighbor_entry  } };

    status = meta_generic_validation_set(meta_key, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (set == NULL)
    {
        SWSS_LOG_ERROR("set function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = set(neighbor_entry, attr);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("set status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("set status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_set(meta_key, attr);
    }

    return status;
}

sai_status_t meta_sai_get_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_neighbor_attribute_fn get)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_neighbor_entry(neighbor_entry, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_NEIGHBOR, .key = { .neighbor_entry = *neighbor_entry } };

    status = meta_generic_validation_get(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (get == NULL)
    {
        SWSS_LOG_ERROR("get function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = get(neighbor_entry, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("get status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("get status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_get(meta_key, attr_count, attr_list);
    }

    return status;
}

// VLAN

sai_status_t meta_sai_validate_vlan_id(
        _In_ sai_vlan_id_t vlan_id,
        _In_ bool create)
{
    SWSS_LOG_ENTER();

    if (vlan_id < MINIMUM_VLAN_NUMBER || vlan_id > MAXIMUM_VLAN_NUMBER)
    {
        SWSS_LOG_ERROR("invalid vlan number %d", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_meta_key_t meta_key_vlan = { .object_type = SAI_OBJECT_TYPE_VLAN, .key = { .vlan_id = vlan_id } };

    std::string key_vlan = get_object_meta_key_string(meta_key_vlan);

    if (create)
    {
        if (object_exists(key_vlan))
        {
            SWSS_LOG_ERROR("object key %s already exists", key_vlan.c_str());

            return SAI_STATUS_INVALID_PARAMETER;
        }

        return SAI_STATUS_SUCCESS;
    }

    if (!object_exists(key_vlan))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key_vlan.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_sai_create_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ sai_create_vlan_fn create)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_vlan_id(vlan_id, true);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_VLAN, .key = { .vlan_id = vlan_id } };

    status = meta_generic_validation_create(meta_key, 0, NULL);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (create == NULL)
    {
        SWSS_LOG_ERROR("create function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = create(vlan_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("create status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("create status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_create(meta_key, 0, NULL);
    }

    return status;
}

sai_status_t meta_sai_remove_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ sai_remove_vlan_fn remove)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_vlan_id(vlan_id, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (vlan_id == DEFAULT_VLAN_NUMBER)
    {
        SWSS_LOG_ERROR("default vlan %d can't be removed", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_VLAN, .key = { .vlan_id = vlan_id } };

    status = meta_generic_validation_remove(meta_key);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (remove == NULL)
    {
        SWSS_LOG_ERROR("remove function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = remove(vlan_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("remove status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("remove status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_remove(meta_key);
    }

    return status;
}

sai_status_t meta_sai_set_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_vlan_attribute_fn set)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_vlan_id(vlan_id, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_VLAN, .key = { .vlan_id = vlan_id  } };

    status = meta_generic_validation_set(meta_key, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (set == NULL)
    {
        SWSS_LOG_ERROR("set function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = set(vlan_id, attr);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("set status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("set status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_set(meta_key, attr);
    }

    return status;
}

sai_status_t meta_sai_get_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_vlan_attribute_fn get)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_vlan_id(vlan_id, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_VLAN, .key = { .vlan_id = vlan_id } };

    status = meta_generic_validation_get(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (get == NULL)
    {
        SWSS_LOG_ERROR("get function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = get(vlan_id, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("get status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("get status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_get(meta_key, attr_count, attr_list);
    }

    return status;
}

// ROUTE ENTRY

sai_status_t meta_sai_validate_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ bool create)
{
    SWSS_LOG_ENTER();

    if (unicast_route_entry == NULL)
    {
        SWSS_LOG_ERROR("unicast_route_entry pointer is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto family = unicast_route_entry->destination.addr_family;

    switch (family)
    {
        case SAI_IP_ADDR_FAMILY_IPV4:
            break;

        case SAI_IP_ADDR_FAMILY_IPV6:

            if (!is_ipv6_mask_valid(unicast_route_entry->destination.mask.ip6))
            {
                SWSS_LOG_ERROR("invalid ipv6 mask: %s", ip_addr_to_string(family, unicast_route_entry->destination.mask.ip6).c_str());

                return SAI_STATUS_INVALID_PARAMETER;
            }

            break;

        default:

            SWSS_LOG_ERROR("invalid prefix family: %d", family);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_id_t vr = unicast_route_entry->vr_id;

    if (vr == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("virtual router is set to null object id");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t object_type = sai_object_type_query(vr);

    if (object_type == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("virtual router oid 0x%llx is not valid object type, returned null object id", vr);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t expected = SAI_OBJECT_TYPE_VIRTUAL_ROUTER;

    if (object_type != expected)
    {
        SWSS_LOG_ERROR("virtual router oid 0x%llx type %d is wrond type, expected object type %d", vr, object_type, expected);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // check if virtual router exists

    sai_object_meta_key_t meta_key_vr = { .object_type = expected, .key = { .object_id = vr } };

    std::string key_vr = get_object_meta_key_string(meta_key_vr);

    if (!object_exists(key_vr))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key_vr.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // check if route entry exists

    sai_object_meta_key_t meta_key_route = { .object_type = SAI_OBJECT_TYPE_ROUTE, .key = { .route_entry = *unicast_route_entry } };

    std::string key_route = get_object_meta_key_string(meta_key_route);

    if (create)
    {
        if (object_exists(key_route))
        {
            SWSS_LOG_ERROR("object key %s already exists", key_route.c_str());

            return SAI_STATUS_INVALID_PARAMETER;
        }

        return SAI_STATUS_SUCCESS;
    }

    // set, get, remove

    if (!object_exists(key_route))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key_route.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_sai_create_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_route_fn create)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_route_entry(unicast_route_entry, true);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_ROUTE, .key = { .route_entry = *unicast_route_entry  } };

    status = meta_generic_validation_create(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (create == NULL)
    {
        SWSS_LOG_ERROR("create function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = create(unicast_route_entry, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("create status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("create status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_create(meta_key, attr_count, attr_list);
    }

    return status;
}

sai_status_t meta_sai_remove_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ sai_remove_route_fn remove)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_route_entry(unicast_route_entry, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_ROUTE, .key = { .route_entry = *unicast_route_entry  } };

    status = meta_generic_validation_remove(meta_key);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (remove == NULL)
    {
        SWSS_LOG_ERROR("remove function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = remove(unicast_route_entry);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("remove status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("remove status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_remove(meta_key);
    }

    return status;
}

sai_status_t meta_sai_set_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_route_attribute_fn set)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_route_entry(unicast_route_entry, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_ROUTE, .key = { .route_entry = *unicast_route_entry  } };

    status = meta_generic_validation_set(meta_key, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (set == NULL)
    {
        SWSS_LOG_ERROR("set function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = set(unicast_route_entry, attr);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("set status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("set status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_set(meta_key, attr);
    }

    return status;
}

sai_status_t meta_sai_get_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_route_attribute_fn get)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_route_entry(unicast_route_entry, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_ROUTE, .key = { .route_entry = *unicast_route_entry } };

    status = meta_generic_validation_get(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (get == NULL)
    {
        SWSS_LOG_ERROR("get function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = get(unicast_route_entry, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("get status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("get status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_get(meta_key, attr_count, attr_list);
    }

    return status;
}

// TRAP

sai_status_t meta_sai_validate_trap(
        _In_ sai_hostif_trap_id_t hostif_trapid)
{
    SWSS_LOG_ENTER();

    if (enum_sai_hostif_trap_id_t_values.find(hostif_trapid) == enum_sai_hostif_trap_id_t_values.end())
    {
        SWSS_LOG_ERROR("trap id %d not found on allowed trap id values list", hostif_trapid);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_sai_set_trap(
        _In_ sai_hostif_trap_id_t hostif_trapid,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_hostif_trap_attribute_fn set)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_trap(hostif_trapid);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_TRAP, .key = { .trap_id = hostif_trapid } };

    status = meta_generic_validation_set(meta_key, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (set == NULL)
    {
        SWSS_LOG_ERROR("set function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = set(hostif_trapid, attr);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("set status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("set status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_set(meta_key, attr);
    }

    return status;
}

sai_status_t meta_sai_get_trap(
        _In_ sai_hostif_trap_id_t hostif_trapid,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_hostif_trap_attribute_fn get)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_trap(hostif_trapid);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = SAI_OBJECT_TYPE_TRAP, .key = { .trap_id = hostif_trapid } };

    status = meta_generic_validation_get(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (get == NULL)
    {
        SWSS_LOG_ERROR("get function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = get(hostif_trapid, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("get status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("get status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_get(meta_key, attr_count, attr_list);
    }

    return status;
}

// GENERIC

sai_status_t meta_sai_validate_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t* object_id,
        _In_ bool create)
{
    SWSS_LOG_ENTER();

    if (object_type <= SAI_OBJECT_TYPE_NULL ||
            object_type >= SAI_OBJECT_TYPE_MAX)
    {
        SWSS_LOG_ERROR("invalid object type specified: %d, FIXME", object_type);
        throw;
    }

    const char* otname = get_object_type_name(object_type);

    switch (object_type)
    {
        case SAI_OBJECT_TYPE_SWITCH:
        case SAI_OBJECT_TYPE_FDB:
        case SAI_OBJECT_TYPE_ROUTE:
        case SAI_OBJECT_TYPE_NEIGHBOR:
        case SAI_OBJECT_TYPE_VLAN:
        case SAI_OBJECT_TYPE_TRAP:

            SWSS_LOG_ERROR("invalid object type (%s) specified as generic, FIXME", otname);
            throw;

        default:
            break;
    }

    SWSS_LOG_DEBUG("generic object type: %s", otname);

    if (object_id == NULL)
    {
        SWSS_LOG_ERROR("oid pointer is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (create)
    {
        return SAI_STATUS_SUCCESS;
    }

    sai_object_id_t oid = *object_id;

    if (oid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("oid is set to null object id");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t ot = sai_object_type_query(oid);

    if (ot == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("%s oid 0x%llx is not valid object type, returned null object id", otname, oid);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t expected = object_type;

    if (ot != expected)
    {
        SWSS_LOG_ERROR("%s oid 0x%llx type %d is wrond type, expected object type %d", otname, oid, ot, expected);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // check if object exists

    sai_object_meta_key_t meta_key_oid = { .object_type = expected, .key = { .object_id = oid } };

    std::string key_oid = get_object_meta_key_string(meta_key_oid);

    if (!object_exists(key_oid))
    {
        SWSS_LOG_ERROR("object key %s don't exists", key_oid.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_sai_create_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_generic_fn create)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_oid(object_type, object_id, true);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = object_type, .key = { .object_id  = SAI_NULL_OBJECT_ID } };

    status = meta_generic_validation_create(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (create == NULL)
    {
        SWSS_LOG_ERROR("create function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = create(object_type, object_id, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("create status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("create status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        // new oid was created, save id and validate it

        meta_key.key.object_id = *object_id;
        meta_generic_validation_post_create(meta_key, attr_count, attr_list);
    }

    return status;
}

sai_status_t meta_sai_remove_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ sai_remove_generic_fn remove)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_oid(object_type, &object_id, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = object_type, .key = { .object_id  = object_id } };

    status = meta_generic_validation_remove(meta_key);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (remove == NULL)
    {
        SWSS_LOG_ERROR("remove function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = remove(object_type, object_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("remove status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("remove status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_remove(meta_key);
    }

    return status;
}

sai_status_t meta_sai_set_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_generic_attribute_fn set)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_oid(object_type, &object_id, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = object_type, .key = { .object_id  = object_id } };

    status = meta_generic_validation_set(meta_key, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (set == NULL)
    {
        SWSS_LOG_ERROR("set function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = set(object_type, object_id, attr);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("set status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("set status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_set(meta_key, attr);
    }

    return status;
}

sai_status_t meta_sai_get_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_generic_attribute_fn get)
{
    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_validate_oid(object_type, &object_id, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    sai_object_meta_key_t meta_key = { .object_type = object_type, .key = { .object_id  = object_id } };

    status = meta_generic_validation_get(meta_key, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (get == NULL)
    {
        SWSS_LOG_ERROR("get function pointer is NULL");

        return SAI_STATUS_FAILURE;
    }

    status = get(object_type, object_id, attr_count, attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("get status: %d", status);
    }
    else
    {
        SWSS_LOG_ERROR("get status: %d", status);
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        meta_generic_validation_post_get(meta_key, attr_count, attr_list);
    }

    return status;
}
