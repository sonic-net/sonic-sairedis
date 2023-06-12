#include "MetaKeyHasher.h"
#include "sai_serialize.h"

#include "swss/logger.h"

#include <cstring>
#include <boost/functional/hash.hpp>

using namespace saimeta;

static bool operator==(
        _In_ const sai_ip_address_t& a,
        _In_ const sai_ip_address_t& b)
{
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
        return memcmp(a.addr.ip6, b.addr.ip6, sizeof(a.addr.ip6)) == 0;
    }

    SWSS_LOG_THROW("unknown IP addr family= %d", a.addr_family);
}

static bool operator==(
        _In_ const sai_ip_prefix_t& a,
        _In_ const sai_ip_prefix_t& b)
{
    if (a.addr_family != b.addr_family)
    {
        return false;
    }

    if (a.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        return a.addr.ip4 == b.addr.ip4 && a.mask.ip4 == b.mask.ip4;
    }

    if (a.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        return memcmp(a.addr.ip6, b.addr.ip6, sizeof(a.addr.ip6)) == 0 &&
            memcmp(a.mask.ip6, b.mask.ip6, sizeof(a.mask.ip6)) == 0;
    }

    SWSS_LOG_THROW("unknown IP addr family= %d", a.addr_family);
}

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

static bool operator==(
        _In_ const sai_my_sid_entry_t& a,
        _In_ const sai_my_sid_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    bool part = a.switch_id == b.switch_id &&
        a.vr_id == b.vr_id &&
        a.locator_block_len == b.locator_block_len &&
        a.locator_node_len == b.locator_node_len &&
        a.function_len == b.function_len &&
        a.args_len == b.args_len;

    return part && memcmp(a.sid, b.sid, sizeof(a.sid)) == 0;
}

static bool operator==(
        _In_ const sai_direction_lookup_entry_t& a,
        _In_ const sai_direction_lookup_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id && a.vni == b.vni;
}

static bool operator==(
        _In_ const sai_eni_ether_address_map_entry_t& a,
        _In_ const sai_eni_ether_address_map_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id && memcmp(a.address, b.address, sizeof(a.address)) == 0;
}

static bool operator==(
        _In_ const sai_vip_entry_t& a,
        _In_ const sai_vip_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id && a.vip == b.vip;
}

static bool operator==(
        _In_ const sai_inbound_routing_entry_t& a,
        _In_ const sai_inbound_routing_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id &&
           a.eni_id == b.eni_id &&
           a.vni == b.vni &&
           a.sip == b.sip &&
           a.sip_mask == b.sip_mask &&
           a.priority == b.priority;
}

static bool operator==(
        _In_ const sai_pa_validation_entry_t& a,
        _In_ const sai_pa_validation_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id && a.vnet_id == b.vnet_id && a.sip == b.sip;
}

static bool operator==(
        _In_ const sai_outbound_routing_entry_t& a,
        _In_ const sai_outbound_routing_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id && a.eni_id == b.eni_id && a.destination == b.destination;
}

static bool operator==(
        _In_ const sai_outbound_ca_to_pa_entry_t& a,
        _In_ const sai_outbound_ca_to_pa_entry_t& b)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    return a.switch_id == b.switch_id && a.dst_vnet_id == b.dst_vnet_id && a.dip == b.dip;
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

    if ((sai_object_type_extensions_t)a.objecttype == SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY)
        return a.objectkey.key.direction_lookup_entry == b.objectkey.key.direction_lookup_entry;

    if ((sai_object_type_extensions_t)a.objecttype == SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY)
        return a.objectkey.key.eni_ether_address_map_entry == b.objectkey.key.eni_ether_address_map_entry;

    if ((sai_object_type_extensions_t)a.objecttype == SAI_OBJECT_TYPE_VIP_ENTRY)
        return a.objectkey.key.vip_entry == b.objectkey.key.vip_entry;

    if ((sai_object_type_extensions_t)a.objecttype == SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY)
        return a.objectkey.key.inbound_routing_entry == b.objectkey.key.inbound_routing_entry;

    if ((sai_object_type_extensions_t)a.objecttype == SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY)
        return a.objectkey.key.pa_validation_entry == b.objectkey.key.pa_validation_entry;

    if ((sai_object_type_extensions_t)a.objecttype == SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY)
        return a.objectkey.key.outbound_routing_entry == b.objectkey.key.outbound_routing_entry;

    if ((sai_object_type_extensions_t)a.objecttype == SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY)
        return a.objectkey.key.outbound_ca_to_pa_entry == b.objectkey.key.outbound_ca_to_pa_entry;

    SWSS_LOG_THROW("not implemented: %s",
            sai_serialize_object_meta_key(a).c_str());
}

static_assert(sizeof(std::size_t) >= sizeof(uint32_t), "size_t must be at least 32 bits");

static inline std::size_t sai_get_hash(
    _In_ const sai_ip_address_t& addr)
{
    // SWSS_LOG_ENTER(); // disabled for performance reason

    size_t hash = 0;

    if (addr.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        boost::hash_combine(hash, addr.addr.ip4);

        return hash;
    }
    else if (addr.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        uint32_t ip6[4];
        memcpy(ip6, addr.addr.ip6, sizeof(ip6));
        boost::hash_combine(hash, ip6[0]);
        boost::hash_combine(hash, ip6[1]);
        boost::hash_combine(hash, ip6[2]);
        boost::hash_combine(hash, ip6[3]);

        return hash;
    }

    SWSS_LOG_THROW("unknown IP addr family: %d", addr.addr_family);
}

static inline std::size_t sai_get_hash(
    _In_ const sai_ip_prefix_t& prefix)
{
    // SWSS_LOG_ENTER(); // disabled for performance reason

    size_t hash = 0;

    if (prefix.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        boost::hash_combine(hash, prefix.addr.ip4);
        boost::hash_combine(hash, prefix.mask.ip4);

        return hash;
    }
    else if (prefix.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        uint32_t ip6[4];
        memcpy(ip6, prefix.addr.ip6, sizeof(ip6));
        boost::hash_combine(hash, ip6[0]);
        boost::hash_combine(hash, ip6[1]);
        boost::hash_combine(hash, ip6[2]);
        boost::hash_combine(hash, ip6[3]);

        memcpy(ip6, prefix.mask.ip6, sizeof(ip6));
        boost::hash_combine(hash, ip6[0]);
        boost::hash_combine(hash, ip6[1]);
        boost::hash_combine(hash, ip6[2]);
        boost::hash_combine(hash, ip6[3]);

        return hash;
    }

    SWSS_LOG_THROW("unknown IP addr family: %d", prefix.addr_family);
}

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

    return ne.data.key.src_ip ^ ne.data.key.dst_ip ^ ne.data.key.proto ^ ne.data.key.l4_src_port ^ ne.data.key.l4_dst_port;
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

    uint32_t ip6[4];
    memcpy(ip6, se.sid, sizeof(ip6));

    return ip6[0] ^ ip6[1] ^ ip6[2] ^ ip6[3];
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

static inline std::size_t sai_get_hash(
        _In_ const sai_direction_lookup_entry_t& de)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    size_t hash = 0;
    boost::hash_combine(hash, de.vni);

    return hash;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_eni_ether_address_map_entry_t & ee)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    std::size_t hash = 0;

    boost::hash_combine(hash, ee.address[0]);
    boost::hash_combine(hash, ee.address[1]);
    boost::hash_combine(hash, ee.address[2]);
    boost::hash_combine(hash, ee.address[3]);
    boost::hash_combine(hash, ee.address[4]);
    boost::hash_combine(hash, ee.address[5]);

    return hash;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_vip_entry_t & ve)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    std::size_t hash = 0;

    boost::hash_combine(hash, sai_get_hash(ve.vip));

    return hash;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_inbound_routing_entry_t & re)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    size_t hash = 0;
    boost::hash_combine(hash, sai_get_hash(re.sip));
    boost::hash_combine(hash, sai_get_hash(re.sip_mask));
    boost::hash_combine(hash, re.eni_id);
    boost::hash_combine(hash, re.vni);
    boost::hash_combine(hash, re.priority);

    return hash;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_pa_validation_entry_t & pe)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    std::size_t hash = 0;

    boost::hash_combine(hash, pe.vnet_id);
    boost::hash_combine(hash, sai_get_hash(pe.sip));

    return hash;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_outbound_routing_entry_t & oe)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    std::size_t hash = 0;
    boost::hash_combine(hash, oe.eni_id);
    boost::hash_combine(hash, sai_get_hash(oe.destination));

    return hash;
}

static inline std::size_t sai_get_hash(
        _In_ const sai_outbound_ca_to_pa_entry_t & oe)
{
    // SWSS_LOG_ENTER(); // disabled for performance reasons

    std::size_t hash = 0;
    boost::hash_combine(hash, oe.dst_vnet_id);
    boost::hash_combine(hash, sai_get_hash(oe.dip));

    return hash;
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
            // Do nothing. Go to extensions
            break;
    }

    switch ((sai_object_type_extensions_t)k.objecttype)
    {
        case SAI_OBJECT_TYPE_DIRECTION_LOOKUP_ENTRY:
            return sai_get_hash(k.objectkey.key.direction_lookup_entry);

        case SAI_OBJECT_TYPE_ENI_ETHER_ADDRESS_MAP_ENTRY:
            return sai_get_hash(k.objectkey.key.eni_ether_address_map_entry);

        case SAI_OBJECT_TYPE_VIP_ENTRY:
            return sai_get_hash(k.objectkey.key.vip_entry);

        case SAI_OBJECT_TYPE_INBOUND_ROUTING_ENTRY:
            return sai_get_hash(k.objectkey.key.inbound_routing_entry);

        case SAI_OBJECT_TYPE_PA_VALIDATION_ENTRY:
            return sai_get_hash(k.objectkey.key.pa_validation_entry);

        case SAI_OBJECT_TYPE_OUTBOUND_ROUTING_ENTRY:
            return sai_get_hash(k.objectkey.key.outbound_routing_entry);

        case SAI_OBJECT_TYPE_OUTBOUND_CA_TO_PA_ENTRY:
            return sai_get_hash(k.objectkey.key.outbound_ca_to_pa_entry);

        default:
            SWSS_LOG_THROW("not handled: %s", sai_serialize_object_type(k.objecttype).c_str());
    }
}
