#include "MetaKeyHasher.h"
#include "sai_serialize.h"

#include "swss/logger.h"

#include <cstring>

using namespace saimeta;

static bool operator==(
        _In_ const sai_fdb_entry_t& a,
        _In_ const sai_fdb_entry_t& b)
{
    SWSS_LOG_ENTER();

    return a.switch_id == b.switch_id &&
        a.bv_id == b.bv_id &&
        memcmp(a.mac_address, b.mac_address, sizeof(a.mac_address)) == 0;
}

static bool operator==(
        _In_ const sai_mcast_fdb_entry_t& a,
        _In_ const sai_mcast_fdb_entry_t& b)
{
    SWSS_LOG_ENTER();

    return a.switch_id == b.switch_id &&
        a.bv_id == b.bv_id &&
        memcmp(a.mac_address, b.mac_address, sizeof(a.mac_address)) == 0;
}

static bool operator==(
        _In_ const sai_route_entry_t& a,
        _In_ const sai_route_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    bool part = a.switch_id == b.switch_id &&
        a.vr_id == b.vr_id &&
        a.destination.addr_family == b.destination.addr_family;

    if (a.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return part &&
            a.destination.addr.ip4 == b.destination.addr.ip4 &&
            a.destination.mask.ip4 == b.destination.mask.ip4;
    }

    if (a.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        return part &&
            memcmp(a.destination.addr.ip6, b.destination.addr.ip6, sizeof(b.destination.addr.ip6)) == 0 &&
            memcmp(a.destination.mask.ip6, b.destination.mask.ip6, sizeof(b.destination.mask.ip6)) == 0;
    }

    SWSS_LOG_THROW("unknown route entry IP addr family: %d", a.destination.addr_family);
}

static bool operator==(
        _In_ const sai_l2mc_entry_t& a,
        _In_ const sai_l2mc_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    bool part = a.switch_id == b.switch_id &&
        a.bv_id == b.bv_id &&
        a.type == b.type &&
        a.destination.addr_family == b.destination.addr_family &&
        a.source.addr_family == b.source.addr_family;

    if (a.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        part &= a.destination.addr.ip4 == b.destination.addr.ip4;
    }
    else if (a.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        part &= memcmp(a.destination.addr.ip6, b.destination.addr.ip6, sizeof(b.destination.addr.ip6)) == 0;
    }
    else
    {
        SWSS_LOG_THROW("unknown l2mc entry destination IP addr family: %d", a.destination.addr_family);
    }

    if (a.source.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        part &= a.source.addr.ip4 == b.source.addr.ip4;
    }
    else if (a.source.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        part &= memcmp(a.source.addr.ip6, b.source.addr.ip6, sizeof(b.source.addr.ip6)) == 0;
    }
    else
    {
        SWSS_LOG_THROW("unknown l2mc entry source IP addr family: %d", a.source.addr_family);
    }

    return part;
}

static bool operator==(
        _In_ const sai_ipmc_entry_t& a,
        _In_ const sai_ipmc_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    bool part = a.switch_id == b.switch_id &&
        a.vr_id == b.vr_id &&
        a.type == b.type &&
        a.destination.addr_family == b.destination.addr_family &&
        a.source.addr_family == b.source.addr_family;

    if (a.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        part &= a.destination.addr.ip4 == b.destination.addr.ip4;
    }
    else if (a.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        part &= memcmp(a.destination.addr.ip6, b.destination.addr.ip6, sizeof(b.destination.addr.ip6)) == 0;
    }
    else
    {
        SWSS_LOG_THROW("unknown l2mc entry destination IP addr family: %d", a.destination.addr_family);
    }

    if (a.source.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        part &= a.source.addr.ip4 == b.source.addr.ip4;
    }
    else if (a.source.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        part &= memcmp(a.source.addr.ip6, b.source.addr.ip6, sizeof(b.source.addr.ip6)) == 0;
    }
    else
    {
        SWSS_LOG_THROW("unknown l2mc entry source IP addr family: %d", a.source.addr_family);
    }

    return part;
}

static bool operator==(
        _In_ const sai_neighbor_entry_t& a,
        _In_ const sai_neighbor_entry_t& b)
{
    SWSS_LOG_ENTER();

    bool part = a.switch_id == b.switch_id &&
        a.rif_id == b.rif_id &&
        a.ip_address.addr_family == b.ip_address.addr_family;

    if (a.ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
        return part && a.ip_address.addr.ip4 == b.ip_address.addr.ip4;

    if (a.ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
        return part && memcmp(a.ip_address.addr.ip6, b.ip_address.addr.ip6, sizeof(b.ip_address.addr.ip6)) == 0;

    SWSS_LOG_THROW("unknown neighbor entry IP addr family= %d", a.ip_address.addr_family);
}

static bool operator==(
        _In_ const sai_nat_entry_t& a,
        _In_ const sai_nat_entry_t& b)
{
    SWSS_LOG_ENTER();

    // we can't use memory compare, since some fields will be padded and they
    // could contain garbage

    return a.switch_id == b.switch_id &&
        a.vr_id == b.vr_id &&
        a.nat_type == b.nat_type &&
        a.data.key.src_ip == b.data.key.src_ip &&
        a.data.key.dst_ip == b.data.key.dst_ip &&
        a.data.key.proto == b.data.key.proto &&
        a.data.key.l4_src_port == b.data.key.l4_src_port &&
        a.data.key.l4_dst_port == b.data.key.l4_dst_port &&
        a.data.mask.src_ip == b.data.mask.src_ip &&
        a.data.mask.dst_ip == b.data.mask.dst_ip &&
        a.data.mask.proto == b.data.mask.proto &&
        a.data.mask.l4_src_port == b.data.mask.l4_src_port &&
        a.data.mask.l4_dst_port == b.data.mask.l4_dst_port;
}

static bool operator==(
        _In_ const sai_inseg_entry_t& a,
        _In_ const sai_inseg_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.label == b.label;
}

bool MetaKeyHasher::operator()(
        _In_ const sai_object_meta_key_t& a,
        _In_ const sai_object_meta_key_t& b) const
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    if (a.objecttype != b.objecttype)
        return false;

    auto meta = sai_metadata_get_object_type_info(a.objecttype);

    if (meta && meta->isobjectid)
        return a.objectkey.key.object_id == b.objectkey.key.object_id;

    if (a.objecttype == SAI_OBJECT_TYPE_ROUTE_ENTRY)
        return a.objectkey.key.route_entry == b.objectkey.key.route_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_NEIGHBOR_ENTRY)
        return a.objectkey.key.neighbor_entry == b.objectkey.key.neighbor_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_FDB_ENTRY)
        return a.objectkey.key.fdb_entry == b.objectkey.key.fdb_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_NAT_ENTRY)
        return a.objectkey.key.nat_entry == b.objectkey.key.nat_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_INSEG_ENTRY)
        return a.objectkey.key.inseg_entry == b.objectkey.key.inseg_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_MCAST_FDB_ENTRY)
        return a.objectkey.key.mcast_fdb_entry == b.objectkey.key.mcast_fdb_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_L2MC_ENTRY)
        return a.objectkey.key.l2mc_entry == b.objectkey.key.l2mc_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_IPMC_ENTRY)
        return a.objectkey.key.ipmc_entry == b.objectkey.key.ipmc_entry;

    SWSS_LOG_THROW("not implemented: %s",
            sai_serialize_object_meta_key(a).c_str());
}

static_assert(sizeof(std::size_t) >= sizeof(uint32_t), "size_t must be at least 32 bits");

static inline std::size_t sai_get_hash(
        _In_ const sai_route_entry_t& re)
{
    // SWSS_LOG_ENTER(); // disabled for performance reason

    if (re.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return re.destination.addr.ip4;
    }

    if (re.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        // cast is not good enough for arm (cast align)
        uint32_t ip6[4];
        memcpy(ip6, re.destination.addr.ip6, sizeof(ip6));

        return ip6[0] ^ ip6[1] ^ ip6[2] ^ ip6[3];
    }

    SWSS_LOG_THROW("unknown route entry IP addr family: %d", re.destination.addr_family);
}

static inline std::size_t sai_get_hash(
        _In_ const sai_neighbor_entry_t& ne)
{
    SWSS_LOG_ENTER();

    if (ne.ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return ne.ip_address.addr.ip4;
    }

    if (ne.ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        // cast is not good enough for arm (cast align)
        uint32_t ip6[4];
        memcpy(ip6, ne.ip_address.addr.ip6, sizeof(ip6));

        return ip6[0] ^ ip6[1] ^ ip6[2] ^ ip6[3];
    }

    SWSS_LOG_THROW("unknown neighbor entry IP addr family= %d", ne.ip_address.addr_family);
}

static_assert(sizeof(uint32_t) == 4, "uint32_t expected to be 4 bytes");

static inline std::size_t sai_get_hash(
        _In_ const sai_fdb_entry_t& fe)
{
    SWSS_LOG_ENTER();

    uint32_t data;

    // use low 4 bytes of mac address as hash value
    // use memcpy instead of cast because of strict-aliasing rules
    memcpy(&data, fe.mac_address + 2, sizeof(uint32_t));

    return data;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_nat_entry_t& ne)
{
    SWSS_LOG_ENTER();

    // TODO revisit - may depend on nat_type

    return ne.data.key.src_ip ^ ne.data.key.dst_ip;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_inseg_entry_t& ie)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return ie.label;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_mcast_fdb_entry_t& mfe)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    uint32_t data;

    // use low 4 bytes of mac address as hash value
    // use memcpy instead of cast because of strict-aliasing rules
    memcpy(&data, mfe.mac_address + 2, sizeof(uint32_t));

    return data;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_l2mc_entry_t& le)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    if (le.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return le.destination.addr.ip4;
    }

    if (le.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        // cast is not good enough for arm (cast align)
        uint32_t ip6[4];
        memcpy(ip6, le.destination.addr.ip6, sizeof(ip6));

        return ip6[0] ^ ip6[1] ^ ip6[2] ^ ip6[3];
    }

    SWSS_LOG_THROW("unknown l2mc entry IP addr family: %d", le.destination.addr_family);
}

static inline std::size_t sai_get_hash(
        _In_ const sai_ipmc_entry_t& ie)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    if (ie.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return ie.destination.addr.ip4;
    }

    if (ie.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        // cast is not good enough for arm (cast align)
        uint32_t ip6[4];
        memcpy(ip6, ie.destination.addr.ip6, sizeof(ip6));

        return ip6[0] ^ ip6[1] ^ ip6[2] ^ ip6[3];
    }

    SWSS_LOG_THROW("unknown ipmc entry IP addr family: %d", ie.destination.addr_family);
}

std::size_t MetaKeyHasher::operator()(
        _In_ const sai_object_meta_key_t& k) const
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    auto meta = sai_metadata_get_object_type_info(k.objecttype);

    if (meta && meta->isobjectid)
    {
        // cast is required in case size_t is 4 bytes (arm)
        return (std::size_t)k.objectkey.key.object_id;
    }

    switch (k.objecttype)
    {
        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return sai_get_hash(k.objectkey.key.route_entry);

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return sai_get_hash(k.objectkey.key.neighbor_entry);

        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return sai_get_hash(k.objectkey.key.fdb_entry);

        case SAI_OBJECT_TYPE_NAT_ENTRY:
            return sai_get_hash(k.objectkey.key.nat_entry);

        case SAI_OBJECT_TYPE_INSEG_ENTRY:
            return sai_get_hash(k.objectkey.key.inseg_entry);

        case SAI_OBJECT_TYPE_MCAST_FDB_ENTRY:
            return sai_get_hash(k.objectkey.key.mcast_fdb_entry);

        case SAI_OBJECT_TYPE_L2MC_ENTRY:
            return sai_get_hash(k.objectkey.key.l2mc_entry);

        case SAI_OBJECT_TYPE_IPMC_ENTRY:
            return sai_get_hash(k.objectkey.key.ipmc_entry);

        default:
            SWSS_LOG_THROW("not handled: %s", sai_serialize_object_type(k.objecttype).c_str());
    }
}
