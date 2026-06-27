#include "SwitchVpp.h"

#include "swss/exec.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiVppXlate.h"

#include "SwitchVppUtils.h"

using namespace saivs;

/* ======================================================================
 * L2 classify-based punt infrastructure.
 *
 * Shared classify tables and resolved hit-next indices are initialised
 * lazily on the first BD member add and persist for the lifetime of
 * the process.
 *
 * Scope:
 *   - LLDP (0x88cc): redirect to linux-cp-punt at l2-input-classify
 *     so the frame ends up on the originating member's LCP host tap.
 *   - DHCPv4 client→server broadcast: redirect to
 *     sonic-ext-l2-trap-fixup, which sets VLIB_RX to the parent
 *     physical interface (parent of the bridged sub-if), and hands
 *     forwards to linux-cp-punt -> bvi-host-tap interface-output ->
 *     sonic-ext-aggr-tap-redirect -> member-host-tap.  This emulates
 *     SAI_PACKET_ACTION_TRAP for DHCP on VPP-VS: the L2-flood is
 *     skipped entirely, so no copies are flooded to other VLAN
 *     members (sonic-mgmt DHCPBroadcastNotFloodedTest depends on
 *     this).
 *
 * LLDP needs the classifier to work around a VPP behavior: link-local
 * multicast frames (DA in 01:80:c2:00:00:00..0F) are flooded by the BD
 * because l2-input strips L2INPUT_FEAT_FWD from the per-buffer feature
 * bitmap for any frame with the multicast bit set, so the FDB is never
 * consulted and a static l2fib entry cannot suppress the flood.  Without
 * the classifier the LLDP frame would be flooded to every BD member
 * (including the BVI flood-copy, which then punts via linux-cp-punt-xc
 * to the BVI host tap), causing lldpd to observe the same neighbour on
 * multiple netdevs and producing inconsistent neighbour discovery
 * results.  The l2-input-classify arc runs before the multicast
 * feat-mask strip, so a redirect-punt session here consumes the frame
 * and delivers it only to the originating member's LCP host tap.
 *
 * DHCP needs the classifier for the dual problem: a client broadcast
 * (dst=ff:ff:ff:ff:ff:ff) would otherwise hit the BD's l2-flood and
 * fan out to every member port.  Trapping at l2-input-classify
 * consumes the buffer before flood, then this node hand-off to
 * sonic-ext-l2-trap-fixup delivers exactly one copy via the parent
 * phys's host tap (where the kernel's 8021q layer demuxes it up to
 * the Vlan netdev where dhcrelay is listening).
 *
 * LACP (0x8809) is intentionally NOT in the classifier: linux-cp
 * registers linux-cp-punt-xc as the ethernet-input next for 0x8809
 * (and 0x88cc, 0x0806) via lcp_ethertype_enable() at startup.  For
 * LACP that dispatch happens in ethernet-input on the parent phy --
 * before l2-input runs and before any sub-interface dispatch -- so
 * LACP never reaches the BD and never needs to be classifier-punted.
 * The same is true for tagged LLDP: ethernet-input dispatches by
 * outer ethertype, and 0x88cc != 0x8100, so tagged LLDP also bypasses
 * the BD via the same linux-cp-punt-xc shortcut.  Only untagged LLDP
 * (which arrives on a parent that IS in the BD) needs the classifier.
 *
 * ARP is handled by the sonic_ext VPP plugin via the arp arc ->
 * sonic-ext-aggr-tap-redirect on the BVI tap.  See
 * platform/vpp/vppbld/plugins/sonic_ext/.
 *
 * Slot selection in l2-input-classify (vnet/l2/l2_input_classify.c):
 * VPP picks the per-interface table by *outer* ethertype at
 * current_data (h0->type):
 *   - 0x0800 (IPv4)   -> ip4_table_index
 *   - 0x86DD (IPv6)   -> ip6_table_index
 *   - everything else -> other_table_index   (incl. 0x8100 VLAN, 0x88CC LLDP)
 *
 * Consequently:
 *
 *   Untagged member  (wire ethertype is the inner protocol):
 *     IP4 slot   <- untag_ip4_table   (DHCPv4 broadcast match)
 *     OTHER slot <- untag_other_table (LLDP match)
 *     No chain between the two: DHCPv4 only ever reaches the IP4
 *     slot; LLDP only ever reaches OTHER.
 *
 *   Tagged member (wire ethertype is 0x8100 -> OTHER slot):
 *     IP4 slot   <- ~0
 *     OTHER slot <- tag_other_table  (defensive LLDP match)
 *                       |  on miss
 *                       v
 *                   tag_dhcp_table (DHCPv4 over .1Q match)
 *                       |  on miss
 *                       v
 *                   continue normal L2 path
 *
 * Hit-next graph slots out of l2-input-classify, resolved once via
 * vpp_add_node_next():
 *   - linux-cp-punt          : LLDP + untagged DHCPv4
 *   - sonic-ext-l2-trap-fixup: tagged DHCPv4 only (rewrites VLIB_RX
 *     from bridged sub-if to parent phys before handing to linux-
 *     cp-punt, because a bridged sub-if has no LCP pair).
 * Untagged DHCP does not need the fixup node: VLIB_RX on an untagged
 * bridge member is already the parent phys, so linux-cp-punt resolves
 * the right LCP pair directly.
 * ====================================================================== */

#define SAIVS_CLASSIFY_ACTION_NONE   0

static bool     s_l2_punt_classify_inited = false;
static uint32_t s_punt_next_index = ~0;       /* linux-cp-punt (untagged + LLDP) */
static uint32_t s_trap_fixup_next_index = ~0; /* sonic-ext-l2-trap-fixup (tagged DHCP only) */

/* Untagged member tables: ip4 slot holds DHCPv4 broadcast,
 * other slot holds LLDP.  No chain between them (different ethertype
 * families select different slots in l2-input-classify). */
static uint32_t s_untag_other_table = ~0;     /* LLDP by ethertype */
static uint32_t s_untag_ip4_table   = ~0;     /* DHCPv4 client broadcast */

/* Tagged member tables: outer ethertype is 0x8100, so EVERY tagged
 * frame (including IPv4-inside-VLAN) lands in the other slot.
 * Chain tag_other -> tag_dhcp -> continue on miss. */
static uint32_t s_tag_other_table = ~0;       /* inner ethertype (LLDP) */
static uint32_t s_tag_dhcp_table = ~0;        /* DHCPv4 broadcast over .1Q */

/*
 * DHCPv4 client→server broadcast match.  We match on:
 *   - dst MAC == ff:ff:ff:ff:ff:ff   (mandatory: client-side broadcast)
 *   - ethertype == 0x0800            (IPv4)
 *   - IP protocol == 17              (UDP)
 *   - UDP dport == 67                (BOOTPS; sport is NOT matched)
 *
 * IP header length is NOT matched: virtually all DHCP packets have
 * IHL=5 (no options) so the UDP header lies at the canonical offset,
 * but if a future client sends DHCP with IP options the classifier
 * mask would also need to chain a second table.  Today this is
 * a non-issue and ASIC TCAM rules used by SAI_PACKET_ACTION_TRAP
 * make the same assumption.
 *
 * Server→client broadcast (sport=67, dport=68) is NOT matched here.
 * That direction is generated by the local relay/server stack and
 * exits via the BVI tap; it does not arrive on a bridge member's
 * wire RX in this topology.
 */
#define SAIVS_DHCP_BOOTPC 68
#define SAIVS_DHCP_BOOTPS 67

static int l2_punt_classify_init()
{
    if (s_l2_punt_classify_inited)
        return 0;

    /* Resolve hit-next graph slots out of l2-input-classify.  These
     * register the target nodes as nexts of l2-input-classify so the
     * hit_next_index in each session is a valid graph edge.
     *
     * linux-cp-punt is required (LLDP + untagged DHCP).
     * sonic-ext-l2-trap-fixup is only required for tagged DHCP: it
     * rewrites VLIB_RX from the bridged sub-if to the parent phys so
     * linux-cp-punt picks the parent's LCP host tap (the sub-if has
     * no LCP pair when it is a pure bridge member). */
    if (vpp_add_node_next("l2-input-classify", "linux-cp-punt",
                                &s_punt_next_index) != 0) {
        SWSS_LOG_ERROR("l2_punt_classify_init: vpp_add_node_next(linux-cp-punt) failed");
        return -1;
    }
    if (vpp_add_node_next("l2-input-classify", "sonic-ext-l2-trap-fixup",
                                &s_trap_fixup_next_index) != 0) {
        /* Not fatal: without the fixup node, only tagged-DHCP punt
         * is broken.  Untagged DHCP and LLDP still work via the
         * direct linux-cp-punt next. */
        SWSS_LOG_WARN("l2_punt_classify_init: sonic-ext-l2-trap-fixup not registered; "
                      "tagged DHCP broadcast will not be punted (untagged DHCP unaffected)");
        s_trap_fixup_next_index = ~0;
    } else {
        SWSS_LOG_NOTICE("l2_punt_classify_init: trap_fixup_next_index=%u",
                        s_trap_fixup_next_index);
    }
    SWSS_LOG_NOTICE("l2_punt_classify_init: punt_next_index=%u", s_punt_next_index);

    /* --- Untagged IP4-slot table (DHCPv4 broadcast) ---
     *
     * l2-input-classify selects this slot when the outer ethertype is
     * 0x0800, which is the case for ALL IPv4 frames on an untagged
     * member (including DHCPv4 client broadcasts).
     *
     * Match span: 0..37  -> skip=0, match=3 (3 x 16 = 48-byte vector).
     *   bytes  0..5   -> dst MAC = ff:ff:ff:ff:ff:ff
     *   bytes 12..13  -> ethertype = 0x0800
     *   byte  23      -> IP protocol = UDP (17)
     *   bytes 36..37  -> UDP dport (67); sport (bytes 34..35) NOT matched
     *
     * Hit_next is linux-cp-punt directly: VLIB_RX is already the
     * parent phys (untagged BD member IS the parent phys; sub-if
     * dispatch never ran), so linux-cp-punt picks the correct LCP
     * pair straight away.  No fixup node needed.
     *
     * No chain on miss: non-DHCP IPv4 traffic should fall through to
     * the rest of the L2 feature arc unchanged.
     */
    {
        uint8_t mask[48] = {0};
        mask[0] = mask[1] = mask[2] = mask[3] = mask[4] = mask[5] = 0xFF;
        mask[12] = 0xFF; mask[13] = 0xFF;
        mask[23] = 0xFF;
        mask[36] = 0xFF; mask[37] = 0xFF;       /* UDP dport only (sport ignored) */

        if (vpp_classify_table_create(
                8 /*nbuckets*/, 64*1024 /*memory_size*/,
                0 /*skip*/, 3 /*match_n_vectors*/,
                ~0 /*next_table*/, ~0 /*miss_next=continue*/,
                mask, 48, &s_untag_ip4_table) != 0) {
            SWSS_LOG_ERROR("l2_punt_classify_init: untag_ip4 table create failed");
            s_untag_ip4_table = ~0;
        } else {
            uint8_t m[48] = {0};
            m[0] = m[1] = m[2] = m[3] = m[4] = m[5] = 0xFF;
            m[12] = 0x08; m[13] = 0x00;
            m[23] = 0x11;                                /* IPPROTO_UDP */
            m[36] = (SAIVS_DHCP_BOOTPS >> 8) & 0xFF;     /* UDP dport == 67 */
            m[37] =  SAIVS_DHCP_BOOTPS       & 0xFF;
            vpp_classify_session_add(s_untag_ip4_table, s_punt_next_index,
                                     m, 48, 0, 0, SAIVS_CLASSIFY_ACTION_NONE);
        }
    }

    /* --- Untagged OTHER-slot table: match ethertype at offset 12 ---
     * l2-input-classify selects this slot for non-IPv4/IPv6 ethertypes
     * (LLDP 0x88CC, ARP, etc.).  Single LLDP session, no chain. */
    {
        uint8_t mask[16] = {0};
        mask[12] = 0xFF; mask[13] = 0xFF;

        if (vpp_classify_table_create(
                8 /*nbuckets*/, 64*1024 /*memory_size*/,
                0 /*skip*/, 1 /*match_n_vectors*/,
                ~0 /*next_table=none*/,
                ~0 /*miss_next=continue*/,
                mask, 16, &s_untag_other_table) != 0) {
            SWSS_LOG_ERROR("l2_punt_classify_init: untag_other table create failed");
            return -1;
        }

        /* LLDP 0x88CC → redirect-punt (consume) */
        uint8_t m[16] = {0}; m[12] = 0x88; m[13] = 0xCC;
        vpp_classify_session_add(s_untag_other_table, s_punt_next_index,
                                 m, 16, 0, 0, SAIVS_CLASSIFY_ACTION_NONE);
    }

    /* --- Tagged DHCP table (created before tag_other for the same
     *     chain reason).  Frame at l2-input-classify on a tagged
     *     sub-if still carries the outer 802.1Q tag (VTR pop has not
     *     yet run), so all post-L2 offsets shift by +4.
     *
     * Match span: 0..41  -> skip=0, match=3 (48-byte vector).
     *   bytes  0..5   -> dst MAC = ff:ff:ff:ff:ff:ff
     *   bytes 16..17  -> inner ethertype = 0x0800
     *   byte  27      -> IP protocol = UDP (= 14 + 4 vlan + 9)
     *   bytes 40..41  -> UDP dport (67); sport (bytes 38..39) NOT matched
     *
     * Tagged hit_next is sonic-ext-l2-trap-fixup (NOT linux-cp-punt
     * directly): VLIB_RX is the sub-if (e.g. Ethernet0.10), which has
     * no LCP pair for a bridged sub-if, so linux-cp-punt would drop
     * the frame.  The fixup node rewrites VLIB_RX to the parent phys
     * (Ethernet0), which always has an LCP pair.  Skip table install
     * if the fixup node is not registered (untagged DHCP still works).
     */
    if (s_trap_fixup_next_index != ~0u) {
        uint8_t mask[48] = {0};
        mask[0] = mask[1] = mask[2] = mask[3] = mask[4] = mask[5] = 0xFF;
        mask[16] = 0xFF; mask[17] = 0xFF;
        mask[27] = 0xFF;
        mask[40] = 0xFF; mask[41] = 0xFF;        /* UDP dport only (sport ignored) */

        if (vpp_classify_table_create(
                8, 64*1024,
                0 /*skip*/, 3 /*match_n_vectors*/,
                ~0 /*next_table*/, ~0 /*miss_next*/,
                mask, 48, &s_tag_dhcp_table) != 0) {
            SWSS_LOG_ERROR("l2_punt_classify_init: tag_dhcp table create failed");
            s_tag_dhcp_table = ~0;
        } else {
            uint8_t m[48] = {0};
            m[0] = m[1] = m[2] = m[3] = m[4] = m[5] = 0xFF;
            m[16] = 0x08; m[17] = 0x00;
            m[27] = 0x11;
            m[40] = (SAIVS_DHCP_BOOTPS >> 8) & 0xFF;     /* UDP dport == 67 */
            m[41] =  SAIVS_DHCP_BOOTPS       & 0xFF;
            vpp_classify_session_add(s_tag_dhcp_table, s_trap_fixup_next_index,
                                     m, 48, 0, 0, SAIVS_CLASSIFY_ACTION_NONE);
        }
    }

    /* --- Tagged "other" table: match inner ethertype at offset 16 ---
     * skip=1, match=1 → covers bytes 16-31.  Chains to tag_dhcp on miss.
     * VPP session match data must be (skip+match)*16 = 32 bytes.
     *
     * In practice ethernet-input dispatches by outer ethertype, and
     * 0x88cc != 0x8100, so tagged LLDP never reaches the sub-if
     * classifier -- it is consumed by linux-cp-punt-xc directly off
     * the parent.  This entry is purely a defensive safety net. */
    {
        uint8_t mask[16] = {0};
        mask[0] = 0xFF; mask[1] = 0xFF;          /* inner ethertype (offset 16-17) */

        uint32_t chain_next = (s_tag_dhcp_table != ~0u)
                                ? s_tag_dhcp_table : (uint32_t)~0u;

        if (vpp_classify_table_create(
                8, 64*1024,
                1 /*skip*/, 1 /*match*/,
                chain_next /*next_table on miss*/,
                ~0 /*miss_next*/,
                mask, 16, &s_tag_other_table) != 0) {
            SWSS_LOG_ERROR("l2_punt_classify_init: tag_other table create failed");
            return -1;
        }

        /* LLDP inner 0x88CC → redirect-punt (defensive)
         * match[0..15] = skip padding, match[16..31] = match vector */
        uint8_t m[32] = {0}; m[16+0] = 0x88; m[16+1] = 0xCC;
        vpp_classify_session_add(s_tag_other_table, s_punt_next_index,
                                 m, 32, 0, 0, SAIVS_CLASSIFY_ACTION_NONE);
    }

    s_l2_punt_classify_inited = true;
    SWSS_LOG_NOTICE("L2 punt classify tables initialized: "
                    "untag_ip4=%u untag_other=%u tag_other=%u tag_dhcp=%u "
                    "punt_next=%u trap_fixup_next=%u",
                    s_untag_ip4_table, s_untag_other_table,
                    s_tag_other_table, s_tag_dhcp_table,
                    s_punt_next_index, s_trap_fixup_next_index);
    return 0;
}

static int l2_punt_classify_apply(const char *hwif_name, bool is_tagged)
{
    if (l2_punt_classify_init() != 0) {
        SWSS_LOG_ERROR("l2_punt_classify_apply: init failed for %s", hwif_name);
        return -1;
    }

    /* Tagged frames carry outer ethertype 0x8100 -> OTHER slot only.
     * Untagged frames carry the inner ethertype on the wire, so DHCPv4
     * (0x0800) lands in the IP4 slot and LLDP (0x88CC) in the OTHER
     * slot; install both. */
    uint32_t ip4_tbl   = is_tagged ? (uint32_t)~0u : s_untag_ip4_table;
    uint32_t other_tbl = is_tagged ? s_tag_other_table : s_untag_other_table;

    int rc = vpp_classify_set_interface_l2_tables(
        hwif_name, ip4_tbl, ~0 /*ip6*/, other_tbl, true /*is_input*/);
    if (rc == 0) {
        SWSS_LOG_NOTICE("l2_punt_classify_apply: %s tagged=%d ip4=%u other=%u",
                        hwif_name, is_tagged, ip4_tbl, other_tbl);
    } else {
        SWSS_LOG_ERROR("l2_punt_classify_apply: set_interface_l2_tables failed(%d) for %s",
                       rc, hwif_name);
    }
    return rc;
}

static int l2_punt_classify_remove(const char *hwif_name)
{
    /* Detach all tables from the interface */
    return vpp_classify_set_interface_l2_tables(
        hwif_name, ~0, ~0, ~0, true /*is_input*/);
}

/**
 * @brief FDB_ENTRY FLUSH Modes.
 */
 typedef enum _fdb_flush_mode_t
 {
     FLUSH_BY_INTERFACE = 1, /* Flushing DYNAMIC FDB_ENTRY on Interface */
     FLUSH_BY_BD_ID = 2,     /* Flushing DYNAMIC FDB_ENTRY on Bridge */
     FLUSH_ALL = 4,          /* Flushing all DYNAMIC FDB_ENTRY on all */
 } fdb_flush_mode;

// utility function to check whether a bridge port is of type TUNNEL
bool SwitchVpp::is_tunnel_bridge_port(
        _In_ sai_object_id_t br_port_id)
{
    SWSS_LOG_ENTER();

    try
    {
        auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT)
                                         .at(sai_serialize_object_id(br_port_id));

        auto meta = sai_metadata_get_attr_metadata(
                        SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_TYPE);

        auto it = br_port_attrs.find(meta->attridname);

        if (it != br_port_attrs.end() &&
            it->second->getAttr()->value.s32 == SAI_BRIDGE_PORT_TYPE_TUNNEL)
        {
            return true;
        }
    }
    catch (const std::exception &e)
    {
        SWSS_LOG_WARN("is_tunnel_bridge_port: exception for %s: %s",
                sai_serialize_object_id(br_port_id).c_str(), e.what());
    }

    return false;
}

sai_status_t SwitchVpp::createVlanMember(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_VLAN_MEMBER, sid, switch_id, attr_count, attr_list));

    return vpp_create_vlan_member(attr_count, attr_list);

}

sai_status_t SwitchVpp::vpp_create_vlan_member(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t br_port_id;

    //find sw_if_index for given l2 interface
    auto attr_type = sai_metadata_get_attr_by_id(SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    br_port_id = attr_type->value.oid;
    sai_object_type_t obj_type = objectTypeQuery(br_port_id);

    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
    {
        SWSS_LOG_ERROR("SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID=%s expected to be BRIDGE PORT but is: %s",
                sai_serialize_object_id(br_port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Skip VPP operations for tunnel bridge ports -- they have no physical port
    if (is_tunnel_bridge_port(br_port_id))
    {
        SWSS_LOG_NOTICE("Skipping VLAN member VPP create for tunnel bridge port %s",
                sai_serialize_object_id(br_port_id).c_str());
        return SAI_STATUS_SUCCESS;
    }

    const char *hwifname = nullptr;
    uint32_t lag_swif_idx;

    auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_id));
    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto bp_attr = br_port_attrs[meta->attridname];
    auto port_id = bp_attr->getAttr()->value.oid;
    obj_type = objectTypeQuery(port_id);

    if (obj_type != SAI_OBJECT_TYPE_PORT && obj_type != SAI_OBJECT_TYPE_LAG )
    {
        SWSS_LOG_NOTICE("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT or LAG but is: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    if (obj_type == SAI_OBJECT_TYPE_PORT)
    {
        std::string if_name;
        bool found = getTapNameFromPortId(port_id, if_name);
        if (found == true)
        {
            hwifname = tap_to_hwif_name(if_name.c_str());
        }else {
            SWSS_LOG_NOTICE("No ports found for bridge port id :%s",sai_serialize_object_id(br_port_id).c_str());
            return SAI_STATUS_FAILURE;
        }
    } else if (obj_type == SAI_OBJECT_TYPE_LAG) {
        platform_bond_info_t bond_info;
        CHECK_STATUS(get_lag_bond_info(port_id, bond_info));
        lag_swif_idx = bond_info.sw_if_index;
        SWSS_LOG_NOTICE("lag swif idx :%d",lag_swif_idx);
	    hwifname =  vpp_get_swif_name(lag_swif_idx);
        SWSS_LOG_NOTICE("lag swif idx :%d swif_name:%s",lag_swif_idx, hwifname);
	    if (hwifname == NULL) {
            SWSS_LOG_NOTICE("LAG is not found for bridge port id :%s",sai_serialize_object_id(br_port_id).c_str());
            return SAI_STATUS_FAILURE;
	    }
    }

    auto attr_vlan_member = sai_metadata_get_attr_by_id(SAI_VLAN_MEMBER_ATTR_VLAN_ID, attr_count, attr_list);

    sai_object_id_t vlan_oid;

    if (attr_vlan_member == NULL)
    {
	    SWSS_LOG_NOTICE("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID was not passed");
	    return SAI_STATUS_FAILURE;
    } else {
	    vlan_oid = attr_vlan_member->value.oid;
    }
    auto attr_vlanid_map = m_objectHash.at(SAI_OBJECT_TYPE_VLAN).at(sai_serialize_object_id(vlan_oid));
    auto md_vlan_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN, SAI_VLAN_ATTR_VLAN_ID);
    auto vlan_id =  attr_vlanid_map.at(md_vlan_id->attridname)->getAttr()->value.u16;

    if (vlan_id == 0)
    {
        SWSS_LOG_NOTICE("attr VLAN object id  was not passed");
        return SAI_STATUS_FAILURE;
    }

    uint32_t bridge_id = (uint32_t)vlan_id;
    auto attr_tag_mode = sai_metadata_get_attr_by_id(SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE, attr_count, attr_list);
    uint32_t tagging_mode = 0;
    const char *hw_ifname;
    char host_subifname[32];

    if (attr_tag_mode == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID was not passed");
        return SAI_STATUS_FAILURE;
    }

    tagging_mode = attr_tag_mode->value.u32;

    if (tagging_mode == SAI_VLAN_TAGGING_MODE_TAGGED)
    {
        /*
         create vpp subinterface and set it as bridge port
        */
        snprintf(host_subifname, sizeof(host_subifname), "%s.%u", hwifname, vlan_id);

        /* lcp-auto-subint creates the host tap automatically */
        create_sub_interface(hwifname, vlan_id, vlan_id);

        hw_ifname = host_subifname;

        //Create bridge and set the l2 port
        set_sw_interface_l2_bridge(hw_ifname,bridge_id, true, VPP_API_PORT_TYPE_NORMAL);

        /* Strip the outer 802.1Q tag on ingress to the BD; VPP pushes
         * it back on egress symmetrically. Required so the BD/BVI
         * sees untagged frames and ip4-dvr-reinject does not deliver
         * tagged frames to the LCP host tap.
         */
        {
            vpp_l2_vtr_op_t vtr_op = L2_VTR_POP_1;
            vpp_vlan_type_t push_dot1q = VLAN_DOT1Q;
            uint32_t tag1 = (uint32_t)vlan_id;
            uint32_t tag2 = ~0;
            set_l2_interface_vlan_tag_rewrite(hw_ifname, tag1, tag2, push_dot1q, vtr_op);
        }

        //Set interface state up
        interface_set_state(hw_ifname, true);

        /* Enable L2 classify-based punt on the sub-interface so control
         * protocols (LLDP, LACP, ARP, DHCP) are punted/copied to the
         * LCP host tap.  Tagged frames still carry the 802.1Q header
         * when l2-input-classify runs (VTR has not yet stripped it).
         */
        l2_punt_classify_apply(hw_ifname, true /*tagged*/);
    }
    else if (tagging_mode == SAI_VLAN_TAGGING_MODE_UNTAGGED)
    {
        hw_ifname = hwifname;

        //Create bridge and set the l2 port
        set_sw_interface_l2_bridge(hw_ifname,bridge_id, true, VPP_API_PORT_TYPE_NORMAL);

        // Untagged BD member: do NOT install an input tag-rewrite on
        // the phy. The wire frame is untagged and must remain untagged
        // through l2-input/BD/BVI; otherwise ip4-dvr-reinject will
        // egress LCP traffic with a stale 802.1Q tag.

        /* Enable L2 classify-based punt on the parent so control
         * protocols (LLDP, LACP, ARP, DHCP) arriving on the parent
         * are punted/copied to the LCP host tap.
         */
        l2_punt_classify_apply(hw_ifname, false /*untagged*/);
    }
    else {
        SWSS_LOG_ERROR("Tagging Mode %d not implemented", tagging_mode);
        return SAI_STATUS_FAILURE;
    }


    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeVlanMember(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    vpp_remove_vlan_member(objectId);

    auto sid = sai_serialize_object_id(objectId);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_VLAN_MEMBER, sid));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_remove_vlan_member(
        _In_ sai_object_id_t vlan_member_oid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;

    sai_status_t status = get(SAI_OBJECT_TYPE_VLAN_MEMBER, vlan_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID is not present");

        return SAI_STATUS_FAILURE;
    }
    sai_object_id_t vlan_oid = attr.value.oid;

    sai_object_type_t obj_type = objectTypeQuery(vlan_oid);

    if (obj_type != SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID is not valid");
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_VLAN, vlan_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_ATTR_VLAN_ID is not present");

        return SAI_STATUS_FAILURE;
    }
    auto vlan_id = attr.value.u16;
    uint32_t bridge_id = (uint32_t)vlan_id;

    attr.id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
    status = get(SAI_OBJECT_TYPE_VLAN_MEMBER, vlan_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID is not present");
        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t br_port_oid = attr.value.oid;

    obj_type = objectTypeQuery(br_port_oid);
    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
    {
        SWSS_LOG_ERROR("SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID=%s expected to be BRIDGE PORT but is: %s",
                sai_serialize_object_id(br_port_oid).c_str(),
                sai_serialize_object_type(obj_type).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Skip VPP operations for tunnel bridge ports -- they have no physical port
    if (is_tunnel_bridge_port(br_port_oid))
    {
        SWSS_LOG_NOTICE("Skipping vlan member remove for TUNNEL bridge port %s",
                sai_serialize_object_id(br_port_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    const char *hw_ifname = nullptr;
    auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_oid));

    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto bp_attr = br_port_attrs[meta->attridname];
    auto port_id = bp_attr->getAttr()->value.oid;
    obj_type = objectTypeQuery(port_id);

    if (obj_type != SAI_OBJECT_TYPE_PORT && obj_type != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_NOTICE("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT or LAG but is: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    if (obj_type == SAI_OBJECT_TYPE_PORT)
    {
        std::string if_name;
        bool found = getTapNameFromPortId(port_id, if_name);
        if (found == true)
        {
            hw_ifname = tap_to_hwif_name(if_name.c_str());
        }else {
            SWSS_LOG_NOTICE("No ports found for bridge port id :%s",sai_serialize_object_id(br_port_oid).c_str());
            return SAI_STATUS_FAILURE;
        }
    } else if (obj_type == SAI_OBJECT_TYPE_LAG) {
        platform_bond_info_t bond_info;
        CHECK_STATUS(get_lag_bond_info(port_id, bond_info));
        uint32_t lag_swif_idx = bond_info.sw_if_index;
        SWSS_LOG_NOTICE("lag swif idx :%d",lag_swif_idx);
	    hw_ifname =  vpp_get_swif_name(lag_swif_idx);
        SWSS_LOG_NOTICE("lag swif idx :%d swif_name:%s",lag_swif_idx, hw_ifname);
	    if (hw_ifname == NULL) {
            SWSS_LOG_NOTICE("LAG port is not found for bridge port id :%s",sai_serialize_object_id(port_id).c_str());
            return SAI_STATUS_FAILURE;
	    }
    }

    attr.id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
    status = get(SAI_OBJECT_TYPE_VLAN_MEMBER, vlan_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE is not present");

        return SAI_STATUS_FAILURE;
    }

    uint32_t tagging_mode = attr.value.s32;
    char host_subifname[32];
    if (tagging_mode == SAI_VLAN_TAGGING_MODE_UNTAGGED)
    {
        /* Disable L2 classify punt before removing the parent
         * from the BD (mirror of the add path).
         */
        l2_punt_classify_remove(hw_ifname);

        /* Untagged member: parent itself is the BD member. No VTR was
         * installed on add, so just remove the parent from the BD.
         */
        set_sw_interface_l2_bridge(hw_ifname, bridge_id, false, VPP_API_PORT_TYPE_NORMAL);
    }
    else if (tagging_mode == SAI_VLAN_TAGGING_MODE_TAGGED)
    {
        // Tagged member: subif <parent>.<vid> is the BD member.
        const char *parent_hwif = hw_ifname;
        snprintf(host_subifname, sizeof(host_subifname), "%s.%u", hw_ifname, vlan_id);
        hw_ifname = host_subifname;

        /* Disable L2 classify punt on subif before teardown */
        l2_punt_classify_remove(hw_ifname);

        // Disable tag-rewrite before removing the subif from the bridge.
        {
            vpp_l2_vtr_op_t vtr_op = L2_VTR_DISABLED;
            vpp_vlan_type_t push_dot1q = VLAN_DOT1Q;
            uint32_t tag1 = (uint32_t)vlan_id;
            uint32_t tag2 = ~0;
            set_l2_interface_vlan_tag_rewrite(hw_ifname, tag1, tag2, push_dot1q, vtr_op);
        }

        // Remove the l2 port from bridge
        set_sw_interface_l2_bridge(hw_ifname, bridge_id, false, VPP_API_PORT_TYPE_NORMAL);

        // delete subinterface (lcp-auto-subint removes host tap automatically)
        delete_sub_interface(parent_hwif, vlan_id);
    }
    else {

        SWSS_LOG_ERROR("Tagging mode %d not implemented", tagging_mode);
        return SAI_STATUS_FAILURE;
    }

    //Check if the bridge has zero ports left, if so remove the bridge as well
    uint32_t member_count = 0;
    bridge_domain_get_member_count (bridge_id, &member_count);
    if (member_count == 0)
    {
        vpp_bridge_domain_add_del(bridge_id, false);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_create_bvi_interface(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto attr_vlan_oid = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_VLAN_ID, attr_count, attr_list);

    if (attr_vlan_oid == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_VLAN_ID was not passed");
        return SAI_STATUS_SUCCESS;
    }

    sai_object_id_t vlan_oid = attr_vlan_oid->value.oid;

    sai_object_type_t obj_type = objectTypeQuery(vlan_oid);

    if (obj_type != SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_ERROR(" VLAN object type was not passed");
        return SAI_STATUS_SUCCESS;
    }
    auto vlan_attrs = m_objectHash.at(SAI_OBJECT_TYPE_VLAN).at(sai_serialize_object_id(vlan_oid));
    auto md_vlan_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN, SAI_VLAN_ATTR_VLAN_ID);
    auto vlan_id = (uint32_t) vlan_attrs.at(md_vlan_id->attridname)->getAttr()->value.u16;

    if (vlan_id == 0)
    {
	    SWSS_LOG_NOTICE("attr VLAN object id  was not passed");
	    return SAI_STATUS_FAILURE;
    }

    auto attr_mac_addr = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS, attr_count, attr_list);
    if (attr_mac_addr == NULL)
    {
	    SWSS_LOG_NOTICE("attr ROUTER INTERFACE MAC Address is not found");
	    return SAI_STATUS_FAILURE;
    }

    sai_mac_t mac_addr;
    memcpy(mac_addr, attr_mac_addr->value.mac, sizeof(sai_mac_t));

    //Create BVI interface
    create_bvi_interface(mac_addr,vlan_id);

    // Get new list of physical interfaces from VS
    refresh_interfaces_list();

    char hw_bviifname[32];
    const char *hw_ifname;
    snprintf(hw_bviifname, sizeof(hw_bviifname), "bvi%u",vlan_id);
    hw_ifname = hw_bviifname;

    //Create bridge and set the l2 port as BVI
    set_sw_interface_l2_bridge(hw_ifname,vlan_id, true, VPP_API_PORT_TYPE_BVI);

    //Set interface state up
    interface_set_state(hw_ifname, true);

    // BVI is the L3 endpoint of the BD and exchanges *untagged* frames
    // with the BD, matching the Linux model where Vlan<id> is presented
    // untagged to the IP stack. No vlan tag-rewrite on the BVI itself.

    // Create LCP pair between bvi<id> and a Linux tap (tap_Vlan<id>).
    // The pair is required so that:
    //   - lcp_itf_pair_add fires the sonic_ext plugin's vft callback,
    //     which enables sonic-ext-aggr-tap-redirect on the BVI tap's
    //     interface-output arc (used by ARP / L3 punt paths).
    //   - linux-cp-punt[-xc] has a host tap to set VLIB_TX to before
    //     aggr-tap-redirect rewrites it to the originating member tap.
    // The kernel-visible Vlan<id> netdev is provisioned independently by
    // SONiC; data-plane punts land on the originating member tap via
    // aggr-tap-redirect, not on tap_Vlan<id>, so no tc mirror is required.
    {
        std::string vpp_ifname = std::string("bvi") + std::to_string(vlan_id);
        std::string tap_name    = std::string("tap_Vlan") + std::to_string(vlan_id);

        SWSS_LOG_NOTICE("configure_lcp_interface vpp_name:%s tap:%s",
                        vpp_ifname.c_str(), tap_name.c_str());
        configure_lcp_interface(vpp_ifname.c_str(), tap_name.c_str(), true);

        refresh_interfaces_list();
        interface_set_state(tap_name.c_str(), true);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_delete_bvi_interface(
        _In_ sai_object_id_t bvi_obj_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    sai_status_t status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, bvi_obj_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_TYPE is not present");
        return SAI_STATUS_FAILURE;
    }

    if (attr.value.s32 != SAI_ROUTER_INTERFACE_TYPE_VLAN)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_TYPE is not VLAN");
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, bvi_obj_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_VLAN_ID is not present");
        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t vlan_oid = attr.value.oid;
    sai_object_type_t obj_type = objectTypeQuery(vlan_oid);

    if (obj_type != SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_MEMBER_ATTR_VLAN_ID is not valid");
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_VLAN, vlan_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_VLAN_ATTR_VLAN_ID is not present");
        return SAI_STATUS_FAILURE;
    }
    auto vlan_id = attr.value.u16;
    uint32_t bd_id = (uint32_t)vlan_id;
    char hw_bviifname[32];
    const char *hw_ifname;
    snprintf(hw_bviifname, sizeof(hw_bviifname), "bvi%u",vlan_id);
    hw_ifname = hw_bviifname;

    //Remove interface from bridge, interface type should be changed to others types like l3.
    set_sw_interface_l2_bridge(hw_ifname, bd_id, false, VPP_API_PORT_TYPE_BVI);

    // Tear down LCP pair for the BVI tap.  Tap name is deterministic:
    // tap_Vlan<id>; no per-instance lookup table is needed.
    {
        std::string tap_name = std::string("tap_Vlan") + std::to_string(vlan_id);

        SWSS_LOG_NOTICE("configure_lcp_interface remove vpp_name:%s tap:%s",
                        hw_ifname, tap_name.c_str());
        configure_lcp_interface(hw_ifname, tap_name.c_str(), false);
    }

    //Remove the bvi interface
    delete_bvi_interface(hw_ifname);

    // refresh interfaces from VS
    refresh_interfaces_list();

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::get_lag_bond_info(const sai_object_id_t lag_id, platform_bond_info_t &bond_info)
{
    SWSS_LOG_ENTER();

    auto it = m_lag_bond_map.find(lag_id);
    if (it == m_lag_bond_map.end())
    {
        SWSS_LOG_ERROR("failed to find bond info for lag id: %s", sai_serialize_object_id(lag_id).c_str());
        return SAI_STATUS_ITEM_NOT_FOUND;
    }
    bond_info = it->second;
    return SAI_STATUS_SUCCESS;
}

int SwitchVpp::remove_lag_to_bond_entry(const sai_object_id_t lag_oid)
{
    SWSS_LOG_ENTER();

    auto it = m_lag_bond_map.find(lag_oid);

    if (it == m_lag_bond_map.end())
    {
        SWSS_LOG_ERROR("failed to find lag swif index for : %s", sai_serialize_object_id(lag_oid).c_str());
        return ~0;
    }

    SWSS_LOG_NOTICE("Removing lag object swif index: %s", sai_serialize_object_id(lag_oid).c_str());
    m_lag_bond_map.erase(it);
    return 0;
}

sai_status_t SwitchVpp::createLag(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);
    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_LAG, sid, switch_id, attr_count, attr_list));
    return vpp_create_lag(object_id, attr_count, attr_list);

}

/*
 * This function determines the ID of a newly created PortChannel.
 * It does this by querying the list of PortChannels using the ip command and
 * comparing it to the existing LAG interfaces in m_lag_bond_map.
 *
 * This function is necessary due to the way IP addresses are set on interfaces in Sonic-VPP,
 * which requires mapping between the PortChannel interface and the BondEthernet in VPP.
 * Although no issues have been observed during manual and sonic-mgmt testing,
 * it should be noted that this design may theoretically present a race condition,
 * where the wrong ID is returned for a given LAG interface if multiple PortChannels are created concurrently.
 */
uint32_t SwitchVpp::find_new_bond_id()
{
    SWSS_LOG_ENTER();

    std::stringstream cmd;
    std::string res;
    uint32_t bond_id = ~0;

    // Get list of PortChannels from ip command
    cmd << IP_CMD << " -o link show | awk -F': ' '{print $2}' | grep " << PORTCHANNEL_PREFIX;

    int ret = swss::exec(cmd.str(), res);
    if (ret) {
        SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        return ~0;
    }

    if (res.length() == 0) {
        SWSS_LOG_ERROR("No PortChannels found in output of command '%s': %s", cmd.str().c_str(), res.c_str());
        return ~0;
    }

    SWSS_LOG_DEBUG("Output of ip command: %s", res.c_str());

    std::unordered_set<uint32_t> existing_bond_ids;
    for (const auto& entry : m_lag_bond_map) {
        existing_bond_ids.insert(entry.second.id);
    }

    std::istringstream iss(res);
    std::string line;
    bool found_new_bond_id = false;
    while (std::getline(iss, line)) {
        std::string portchannel_name = line.substr(0, line.find('\n'));
        bond_id = std::stoi(portchannel_name.substr(strlen(PORTCHANNEL_PREFIX)));

        if (existing_bond_ids.find(bond_id) == existing_bond_ids.end()) {
            SWSS_LOG_NOTICE("Found new bond id from PortChannel name: %d", bond_id);
            found_new_bond_id = true;
            break;
        }
    }

    return found_new_bond_id ? bond_id : ~0;
}

sai_status_t SwitchVpp::vpp_create_lag(
        _In_ sai_object_id_t lag_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    uint32_t mode, lb;
    uint32_t bond_id = ~0;
    uint32_t swif_idx = ~0;
    const char *hw_ifname;

    // Extract bond_id from PortChannel name
    bond_id = find_new_bond_id();
    if (bond_id == static_cast<uint32_t>(~0))
    {
        SWSS_LOG_ERROR("Bond id could not be found");
        return SAI_STATUS_FAILURE;
    }

    // Set mode and lb. SONiC config does not have provision to pass mode and load balancing algorithm.
    // Select VPP's new opt-in inner-aware LAG hash algorithm (BOND_API_LB_ALGO_L34_INNER, value 6,
    // CLI keyword "l34-inner") so that LAG distribution stays balanced for IPinIP / 6in4 / 4in6 /
    // 6in6 / GRE / NVGRE transit tunnel traffic.  The existing BOND_API_LB_ALGO_L34 (= 1) and the
    // registered hash function "hash-eth-l34" are byte-for-byte unchanged on the VPP side, so a
    // libsaivs that selects value 1 keeps the legacy outer-only hashing behaviour; libsaivs that
    // selects value 6 (this code) gets the inner-aware hash function "hash-eth-l34-inner".
    // ABI compatibility with stock libvppinfra is preserved because the new enum value carries the
    // [backwards_compatible] annotation in src/vnet/bonding/bond.api -- vppapigen excludes it from
    // the CRC of every bond_create* / sw_interface_bond_details / sw_bond_interface_details message.
    mode = VPP_BOND_API_MODE_XOR;
    lb = VPP_BOND_API_LB_ALGO_L34_INNER;

    create_bond_interface(bond_id, mode, lb, &swif_idx);
    if (swif_idx == static_cast<uint32_t>(~0))
    {
        SWSS_LOG_ERROR("failed to create bond interface in VPP for %s", sai_serialize_object_id(lag_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    // Update the lag to bond map
    platform_bond_info_t bond_info = {swif_idx, bond_id, false};
    m_lag_bond_map[lag_id] = bond_info;
    SWSS_LOG_NOTICE("vpp bond interface created for lag_id:%s, swif index:%d, bond_id:%d\n", sai_serialize_object_id(lag_id).c_str(), swif_idx, bond_id);
    refresh_interfaces_list();

    // Set the bond interface state up
    hw_ifname = vpp_get_swif_name(swif_idx);
    SWSS_LOG_NOTICE("Setting lag hw interface state to up :%s",hw_ifname);
    interface_set_state(hw_ifname, true);
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeLag(
        _In_ sai_object_id_t lag_oid)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS_QUIET(vpp_remove_lag(lag_oid));
    auto sid = sai_serialize_object_id(lag_oid);
    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_LAG, sid));
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_remove_lag(
        _In_ sai_object_id_t lag_oid)
{
    SWSS_LOG_ENTER();

    int ret;
    platform_bond_info_t bond_info;

    CHECK_STATUS(get_lag_bond_info(lag_oid, bond_info));
    uint32_t lag_swif_idx = bond_info.sw_if_index;
    auto lag_ifname =  vpp_get_swif_name(lag_swif_idx);
    SWSS_LOG_NOTICE("lag swif idx :%d swif_name:%s",lag_swif_idx, lag_ifname);
    if (lag_ifname == NULL)
    {
        SWSS_LOG_NOTICE("LAG interface name is not found for LAG PORT :%s",sai_serialize_object_id(lag_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    //Delete the Bond interface (also deletes the lcp pair)
    ret = delete_bond_interface(lag_ifname);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("failed to delete bond interface in VPP for %s", sai_serialize_object_id(lag_oid).c_str());
        return SAI_STATUS_FAILURE;
    }
    remove_lag_to_bond_entry(lag_oid);
    refresh_interfaces_list();

    return SAI_STATUS_SUCCESS;
}


sai_status_t SwitchVpp::createLagMember(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_LAG_MEMBER, sid, switch_id, attr_count, attr_list));
    return vpp_create_lag_member(attr_count, attr_list);
}

sai_status_t SwitchVpp::vpp_create_lag_member(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    bool is_long_timeout = false;
    bool is_passive = false;
    int ret;
    uint32_t bond_if_idx;
    uint32_t bond_id;
    sai_object_id_t lag_oid, lag_port_oid;

    //Get the bond interface index from attr SAI_LAG_MEMBER_ATTR_LAG_ID
    auto attr_type = sai_metadata_get_attr_by_id(SAI_LAG_MEMBER_ATTR_LAG_ID, attr_count, attr_list);
    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_LAG_ID was not passed");
        return SAI_STATUS_FAILURE;
    }
    lag_oid = attr_type->value.oid;
    sai_object_type_t obj_type = objectTypeQuery(lag_oid);

    if (obj_type != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_ERROR(" SAI_LAG_MEMBER_ATTR_LAG_ID = %s expected to be LAG ID but is: %s",
                sai_serialize_object_id(lag_oid).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    platform_bond_info_t bond_info;
    CHECK_STATUS(get_lag_bond_info(lag_oid, bond_info));
    bond_if_idx = bond_info.sw_if_index;
    SWSS_LOG_NOTICE("bond if index is %d\n", bond_if_idx);

    attr_type = sai_metadata_get_attr_by_id(SAI_LAG_MEMBER_ATTR_PORT_ID, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_PORT_ID was not present\n");
        return SAI_STATUS_FAILURE;
    }

    lag_port_oid = attr_type->value.oid;
    SWSS_LOG_NOTICE("lag port id is %s",sai_serialize_object_id(lag_port_oid).c_str());
    obj_type = objectTypeQuery(lag_port_oid);
    if (obj_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_NOTICE("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(lag_port_oid).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    std::string if_name;
    bool found = getTapNameFromPortId(lag_port_oid, if_name);
    const char *hwifname;
    if (found == true)
    {
        hwifname = tap_to_hwif_name(if_name.c_str());
        SWSS_LOG_NOTICE("hwif name for port is %s",hwifname);
    }else {
        SWSS_LOG_NOTICE("No ports found for lag port id :%s",sai_serialize_object_id(lag_port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    ret = create_bond_member(bond_if_idx, hwifname, is_passive, is_long_timeout);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("failed to add bond member in VPP for %s", sai_serialize_object_id(lag_port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    if (!bond_info.lcp_created) {
        // create tap and lcp for the Bond intf after first member is added to ensure tap mac = member mac = bond mac
        std::ostringstream tap_stream;
        bond_id = bond_info.id;
        tap_stream << "be" << bond_id;
        std::string tap = tap_stream.str();

        const char *hw_ifname;
        hw_ifname = vpp_get_swif_name(bond_if_idx);
        configure_lcp_interface(hw_ifname, tap.c_str(), true);

        // add tc filter to redirect traffic from tap to PortChannel
        std::string portchannel = std::string("PortChannel") + std::to_string(bond_id);
        std::string be = std::string("be") + std::to_string(bond_id);
        CHECK_STATUS(add_tc_filter_redirect(be, portchannel));

        // update the lag to bond map
        bond_info.lcp_created = true;
        m_lag_bond_map[lag_oid] = bond_info;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeLagMember(
        _In_ sai_object_id_t lag_member_oid)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS_QUIET(vpp_remove_lag_member(lag_member_oid));

    auto sid = sai_serialize_object_id(lag_member_oid);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_LAG_MEMBER, sid));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_remove_lag_member(
        _In_ sai_object_id_t lag_member_oid)
{
    SWSS_LOG_ENTER();

    int ret;

    sai_attribute_t attr;

    attr.id = SAI_LAG_MEMBER_ATTR_LAG_ID;

    sai_status_t status = get(SAI_OBJECT_TYPE_LAG_MEMBER, lag_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_LAG_ID is not present");

        return SAI_STATUS_FAILURE;
    }
    sai_object_id_t lag_oid = attr.value.oid;

    sai_object_type_t obj_type = objectTypeQuery(lag_oid);

    if (obj_type != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_LAG_ID is not valid");
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_LAG_MEMBER_ATTR_PORT_ID;

    status = get(SAI_OBJECT_TYPE_LAG_MEMBER, lag_member_oid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_PORT_ID is not present");

        return SAI_STATUS_FAILURE;
    }
    sai_object_id_t port_oid = attr.value.oid;

    obj_type = objectTypeQuery(port_oid);

    if (obj_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("attr SAI_LAG_MEMBER_ATTR_PORT_ID is not valid");
        return SAI_STATUS_FAILURE;
    }

    std::string if_name;
    bool found = getTapNameFromPortId(port_oid, if_name);
    const char *lag_member_ifname;
    if (found == true)
    {
        lag_member_ifname = tap_to_hwif_name(if_name.c_str());
	SWSS_LOG_NOTICE("hwif name for port is %s",lag_member_ifname);
    } else {
        SWSS_LOG_NOTICE("No ports found for lag port id :%s",sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    ret = delete_bond_member(lag_member_ifname);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("failed to delete bond member in VPP for %s", sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::FdbEntryadd(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectId, switch_id, attr_count, attr_list));

    vpp_fdbentry_add(serializedObjectId, switch_id, attr_count, attr_list);

    return SAI_STATUS_SUCCESS;

}

sai_status_t SwitchVpp::FdbEntrydel(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    vpp_fdbentry_del(serializedObjectId);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectId));

    return SAI_STATUS_SUCCESS;

}

sai_status_t SwitchVpp::vpp_fdbentry_add(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{

    SWSS_LOG_ENTER();

    sai_fdb_entry_t fdb_entry;
    sai_deserialize_fdb_entry(serializedObjectId, fdb_entry);

    /* Attribute#1 */
    auto attr_type = sai_metadata_get_attr_by_id(SAI_FDB_ENTRY_ATTR_TYPE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_FDB_ENTRY_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }

    bool is_static = (attr_type->value.s32 == SAI_FDB_ENTRY_TYPE_STATIC ? true : false);
    bool is_add = true; /* Adding the entry in FDB*/

    /* Attribute#2 */
    sai_object_id_t br_port_id;
    sai_object_id_t port_id;

    attr_type = sai_metadata_get_attr_by_id(SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    br_port_id = attr_type->value.oid;
    sai_object_type_t obj_type = objectTypeQuery(br_port_id);

    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
    {
        SWSS_LOG_ERROR("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(br_port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Skip VPP FDB add for tunnel bridge ports -- L2 VXLAN FDB is handled
    // separately via the EVPN remote-MAC path, not the per-port FDB path
    if (is_tunnel_bridge_port(br_port_id))
    {
        SWSS_LOG_NOTICE("Skipping FDB add for tunnel bridge port %s",
                sai_serialize_object_id(br_port_id).c_str());
        return SAI_STATUS_SUCCESS;
    }

    auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_id));
    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto bp_attr = br_port_attrs[meta->attridname];
    port_id = bp_attr->getAttr()->value.oid;
    obj_type = objectTypeQuery(port_id);

    if (obj_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_NOTICE("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    /* Need to extract the VLAN ID attached based on the Port_ID */
    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;

    sai_status_t get_status = get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr);

    if (get_status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get port vlan id from port %s",
                sai_serialize_object_id(port_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    uint32_t bd_id = attr.value.u16; /* bd_id is same as VLAN ID for .1Q bridge */

    std::string ifname;
    if (vpp_get_hwif_name(port_id, 0, ifname) == true)
    {
        const char *hwif_name = ifname.c_str();
        auto ret = l2fib_add_del(hwif_name, fdb_entry.mac_address, bd_id, is_add, is_static);
        SWSS_LOG_NOTICE("FDB Entry Added on hwif_name %s Successful ret_val: %d", hwif_name, ret);

    }
    else
    {
        SWSS_LOG_ERROR("FDB_ENTRY failed because of INVALID PORT_ID");

        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_fdbentry_del(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_fdb_entry_t fdb_entry;
    sai_deserialize_fdb_entry(serializedObjectId, fdb_entry);

    sai_object_id_t br_port_id;
    sai_object_id_t port_id;
    bool is_static = false;

    sai_attribute_t attr_list[2];
    /* Attribute#1 */
    attr_list[0].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    /* Attribute#2 */
    attr_list[1].id = SAI_FDB_ENTRY_ATTR_TYPE;

    if (get(SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectId, 1, &attr_list[0]) == SAI_STATUS_SUCCESS)
    {
       if (SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID == attr_list[0].id)
        {
            br_port_id = attr_list[0].value.oid;
        }
        else
        {
            SWSS_LOG_ERROR("DELETE FDB_ENTRY failed because of INVALID ATTR BRIDGE_PORT_ID");
            return SAI_STATUS_FAILURE;
        }

        if (get(SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectId, 1, &attr_list[1]) == SAI_STATUS_SUCCESS)
        {
            if (SAI_FDB_ENTRY_ATTR_TYPE == attr_list[1].id )
            {
                is_static = (attr_list[1].value.s32 == SAI_FDB_ENTRY_TYPE_STATIC ? true : false);
            }
            else
            {
                SWSS_LOG_ERROR("DELETE FDB_ENTRY failed because of INVALID ATTR ENTRY TYPE");
                return SAI_STATUS_FAILURE;
            }
        }
    }
    else
    {
        SWSS_LOG_ERROR(" Invaid Attribute IDs passed for DELETE FDB_ENTRY");
        return SAI_STATUS_FAILURE;
    }
    bool is_add = false; /* Deleting the entry in FDB*/

    sai_object_type_t obj_type = objectTypeQuery(br_port_id);
    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
    {
        SWSS_LOG_ERROR("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(br_port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());

        return SAI_STATUS_FAILURE;
    }

    // Skip VPP FDB delete for tunnel bridge ports
    if (is_tunnel_bridge_port(br_port_id))
    {
        SWSS_LOG_NOTICE("Skipping FDB delete for tunnel bridge port %s",
                sai_serialize_object_id(br_port_id).c_str());
        return SAI_STATUS_SUCCESS;
    }

    auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_id));
    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto bp_attr = br_port_attrs[meta->attridname];
    port_id = bp_attr->getAttr()->value.oid;
    obj_type = objectTypeQuery(port_id);

    if (obj_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_object_type(obj_type).c_str());
        return SAI_STATUS_FAILURE;
    }

    /* Need the VLAN ID attached based on the Port_ID */
    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;

    sai_status_t get_status = get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr);

    if (get_status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get port vlan id from port %s",
                sai_serialize_object_id(port_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    uint32_t bd_id = attr.value.u16; /* bd_id is same as VLAN ID for .1Q bridge */

    std::string ifname;

    if (vpp_get_hwif_name(port_id, 0, ifname) == true)
    {
        const char *hwif_name = ifname.c_str();
        auto ret = l2fib_add_del(hwif_name, fdb_entry.mac_address, bd_id, is_add, is_static);
        SWSS_LOG_NOTICE(" Delete FDB_ENTRY on hwif_name %s Successful ret_val: %d", hwif_name, ret);

    }
    else
    {
        SWSS_LOG_ERROR("FDB entry Delete: Invalid ObjectID for the hwif on this bridge");

        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_fdbentry_flush(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attribute;
    sai_object_id_t br_port_id = 0;
    sai_object_id_t port_id;
    uint32_t bd_id = 0;
    uint8_t mode = 0;
    bool is_static_entry = false;

    for (uint32_t i = 0; i < attr_count; i++)
    {
        attribute = attr_list[i];
        switch (attribute.id)
        {
            case SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID:
                {
                    mode |= FLUSH_BY_INTERFACE;
                    br_port_id = attribute.value.oid;
                    sai_object_type_t obj_type = objectTypeQuery(br_port_id);

                    if (obj_type != SAI_OBJECT_TYPE_BRIDGE_PORT)
                    {
                        SWSS_LOG_ERROR("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID=%s expected to be PORT but is: %s",
                                sai_serialize_object_id(br_port_id).c_str(),
                                sai_serialize_object_type(obj_type).c_str());

                        return SAI_STATUS_FAILURE;
                    }
                }
                break;

            case SAI_FDB_FLUSH_ATTR_BV_ID:
                {
                    mode |= FLUSH_BY_BD_ID;
                    bd_id = attribute.value.u16;
                }
                break;

            case SAI_FDB_FLUSH_ATTR_ENTRY_TYPE:
                {
                    mode |= FLUSH_ALL;
                    is_static_entry = attribute.value.s32;
                    if ( is_static_entry == SAI_FDB_FLUSH_ENTRY_TYPE_STATIC)
                    {
                        SWSS_LOG_ERROR(" Cannot Flush STATIC FDB_ENTRY OBJECTS");
                        return SAI_STATUS_FAILURE;
                    }
                }
                break;

            default:
                SWSS_LOG_ERROR(" Invalid Attributes for fdb entry flush OBJECT");
                return SAI_STATUS_FAILURE;
                break;
        }
    }
    /*
       Here three cases are handled, the FDB_ENTRY's are flushed based on the Attributes set,
       1. If Interface and Type(DYNAMIC is expected here), FLUSH by Interface.
       2. If Bridge_ID(VLAN_ID for .1q) and Type(DYNAMIC is expected here), FLUSH by Bridge ID.
       3. If only Type (DYNAMIC) is set then SONiC FLUSH ALL the dynamic entries.
       */
    SWSS_LOG_NOTICE("VPP_FDB_FLUSH mode is : %d [1,5: Interface, 2,6: Bridge, 3,4,7: Flush ALL, 0: INVALID]", mode);
    switch (mode)
    {
        case FLUSH_BY_INTERFACE:
        case FLUSH_BY_INTERFACE | FLUSH_ALL:/*flush by interface*/
            {
                // Tunnel bridge ports have no physical port -- fall back to flush all
                if (is_tunnel_bridge_port(br_port_id))
                {
                    SWSS_LOG_NOTICE("Tunnel bridge port %s: falling back to flush all",
                            sai_serialize_object_id(br_port_id).c_str());
                    auto ret = l2fib_flush_all();
                    SWSS_LOG_NOTICE("Flush ALL (tunnel bridge port fallback) ret_val: %d", ret);
                    break;
                }

                auto br_port_attrs = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).at(sai_serialize_object_id(br_port_id));
                auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
                auto bp_attr = br_port_attrs[meta->attridname];
                port_id = bp_attr->getAttr()->value.oid;
                sai_object_type_t obj_type = objectTypeQuery(port_id);

                if (obj_type != SAI_OBJECT_TYPE_PORT)
                {
                    SWSS_LOG_ERROR("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                            sai_serialize_object_id(port_id).c_str(),
                            sai_serialize_object_type(obj_type).c_str());
                    return SAI_STATUS_FAILURE;
                }
                std::string ifname = "";
                if (vpp_get_hwif_name(port_id, 0, ifname) == true)
                {
                    const char *hwif_name = ifname.c_str();
                    auto ret = l2fib_flush_int(hwif_name);
                    SWSS_LOG_NOTICE(" Flush by interface on hwif_name %s  Successful ret_val: %d", hwif_name, ret);
                }
                else
                {
                    SWSS_LOG_ERROR("Flush Interface FDB: Invalid ObjectID for the hwif on this bridge");

                    return SAI_STATUS_FAILURE;
                }
            }
            break;

        case FLUSH_BY_BD_ID:
        case FLUSH_BY_BD_ID | FLUSH_ALL: /*flush by bd_id/vlan id*/
            {
                auto ret = l2fib_flush_bd(bd_id);
                SWSS_LOG_NOTICE(" Flush on bd_id %d Successfull ret_val: %d",bd_id, ret);
            }
            break;

        case FLUSH_BY_INTERFACE | FLUSH_BY_BD_ID:
        case FLUSH_ALL:
        case FLUSH_BY_INTERFACE| FLUSH_BY_BD_ID| FLUSH_ALL: /*flush all*/
            {
                auto ret = l2fib_flush_all();
                SWSS_LOG_NOTICE(" Flush ALL fdb entry ret_val: %d", ret);
            }
            break;

        default:
            SWSS_LOG_ERROR(" Unable to find attrs for FDB_FLUSH %d", mode);
            return SAI_STATUS_FAILURE;
            break;

    }

    return SAI_STATUS_SUCCESS;
}