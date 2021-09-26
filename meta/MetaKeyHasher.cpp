#include "MetaKeyHasher.h"
#include "sai_serialize.h"

#include "swss/logger.h"

#include <cstring>

using namespace saimeta;

#define EQ_MAC(a,b) (memcmp(a, b, sizeof(a)) == 0)
#define EQ_IP6(a,b) (memcmp(a, b, sizeof(a)) == 0)

static inline bool operator==(
        _In_ const sai_fdb_entry_t& a,
        _In_ const sai_fdb_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.bv_id == b.bv_id &&
        EQ_MAC(a.mac_address, b.mac_address);
}

static inline bool operator==(
        _In_ const sai_mcast_fdb_entry_t& a,
        _In_ const sai_mcast_fdb_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.bv_id == b.bv_id &&
        EQ_MAC(a.mac_address, b.mac_address);
}

static inline bool operator==(
        _In_ const sai_ip_prefix_t& a,
        _In_ const sai_ip_prefix_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    if (a.addr_family != b.addr_family)
    {
        return false;
    }

    if (a.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return a.addr.ip4 == b.addr.ip4 &&
            a.mask.ip4 == b.mask.ip4;
    }

    if (a.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        return EQ_IP6(a.addr.ip6, b.addr.ip6) &&
            EQ_IP6(a.mask.ip6, b.mask.ip6);
    }

    SWSS_LOG_THROW("unknown IP addr family: %d", a.addr_family);
}

static inline bool operator==(
        _In_ const sai_route_entry_t& a,
        _In_ const sai_route_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.vr_id == b.vr_id &&
        a.destination == b.destination;
}

static inline bool operator==(
        _In_ const sai_ip_address_t& a,
        _In_ const sai_ip_address_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    if (a.addr_family != b.addr_family)
    {
        return false;
    }

    if (a.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return a.addr.ip4 == b.addr.ip4;
    }

    if (a.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        return EQ_IP6(a.addr.ip6, b.addr.ip6);
    }

    SWSS_LOG_THROW("unknown IP addr family: %d", a.addr_family);
}

static inline bool operator==(
        _In_ const sai_l2mc_entry_t& a,
        _In_ const sai_l2mc_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.bv_id == b.bv_id &&
        a.type == b.type &&
        a.destination == b.destination &&
        a.source == b.source;
}

static inline bool operator==(
        _In_ const sai_ipmc_entry_t& a,
        _In_ const sai_ipmc_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.vr_id == b.vr_id &&
        a.type == b.type &&
        a.destination == b.destination &&
        a.source == b.source;
}

static inline bool operator==(
        _In_ const sai_neighbor_entry_t& a,
        _In_ const sai_neighbor_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.rif_id == b.rif_id &&
        a.ip_address == b.ip_address;
}

static inline bool operator==(
        _In_ const sai_nat_entry_t& a,
        _In_ const sai_nat_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

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

static inline bool operator==(
        _In_ const sai_inseg_entry_t& a,
        _In_ const sai_inseg_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.label == b.label;
}

static inline bool operator==(
        _In_ const sai_my_sid_entry_t& a,
        _In_ const sai_my_sid_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
        a.vr_id == b.vr_id &&
        a.locator_block_len == b.locator_block_len &&
        a.locator_node_len == b.locator_node_len &&
        a.function_len == b.function_len &&
        a.args_len == b.args_len &&
        EQ_IP6(a.sid, b.sid);
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

    if (a.objecttype == SAI_OBJECT_TYPE_MY_SID_ENTRY)
        return a.objectkey.key.my_sid_entry == b.objectkey.key.my_sid_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_MCAST_FDB_ENTRY)
        return a.objectkey.key.mcast_fdb_entry == b.objectkey.key.mcast_fdb_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_L2MC_ENTRY)
        return a.objectkey.key.l2mc_entry == b.objectkey.key.l2mc_entry;

    if (a.objecttype == SAI_OBJECT_TYPE_IPMC_ENTRY)
        return a.objectkey.key.ipmc_entry == b.objectkey.key.ipmc_entry;

    SWSS_LOG_THROW("not implemented: %s, FIXME",
            sai_serialize_object_meta_key(a).c_str());
}

static_assert(sizeof(std::size_t) >= sizeof(uint32_t), "size_t must be at least 32 bits");

static inline std::size_t sai_get_hash(
        _In_ const sai_ip6_t& input)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    // cast is not good enough for arm (cast align)
    uint32_t ip6[4];
    memcpy(ip6, input, sizeof(ip6));

    return ip6[0] ^ ip6[1] ^ ip6[2] ^ ip6[3];
}

static inline std::size_t sai_get_hash(
        _In_ const sai_route_entry_t& re)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    if (re.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return re.destination.addr.ip4;
    }

    if (re.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        return sai_get_hash(re.destination.addr.ip6);
    }

    SWSS_LOG_THROW("unknown route entry IP addr family: %d", re.destination.addr_family);
}

static inline std::size_t sai_get_hash(
        _In_ const sai_neighbor_entry_t& ne)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    if (ne.ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return ne.ip_address.addr.ip4;
    }

    if (ne.ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        return sai_get_hash(ne.ip_address.addr.ip6);
    }

    SWSS_LOG_THROW("unknown neighbor entry IP addr family= %d", ne.ip_address.addr_family);
}

static_assert(sizeof(uint32_t) == 4, "uint32_t expected to be 4 bytes");

static inline std::size_t sai_get_hash(
        _In_ const sai_fdb_entry_t& fe)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    uint32_t data;

    // use low 4 bytes of mac address as hash value
    // use memcpy instead of cast because of strict-aliasing rules
    memcpy(&data, fe.mac_address + 2, sizeof(uint32_t));

    return data;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_nat_entry_t& ne)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

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
        _In_ const sai_my_sid_entry_t& se)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return sai_get_hash(se.sid);
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
        return sai_get_hash(le.destination.addr.ip6);
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
        return sai_get_hash(ie.destination.addr.ip6);
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

        case SAI_OBJECT_TYPE_MY_SID_ENTRY:
            return sai_get_hash(k.objectkey.key.my_sid_entry);

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
