/*
 * VPP MPLS backend for SAI INSEG (MPLS in-segment / local label) entries.
 * Issue: sonic-buildimage#25782 - enable MPLS data plane testing on VPP.
 *
 * Translates SAI_OBJECT_TYPE_INSEG_ENTRY into VPP MPLS FIB programming via
 * mpls_route_add_del(). Pop (IP next-hop, no out-labels) and swap/push
 * (SAI_NEXT_HOP_TYPE_MPLS next-hop carrying an out-label stack) are supported.
 */
#include "SwitchVpp.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiVppXlate.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>

using namespace saivs;

/* VPP MPLS FIB table used for all SONiC in-segment entries. */
#define MPLS_TABLE_ID 0
/* Reserved MPLS implicit-null label (RFC 3032): pop and forward on the payload. */
#define MPLS_IMPLICIT_NULL_LABEL 3

void SwitchVpp::getOutsegTtl(
        _In_ const SaiObject *nh_obj,
        _Out_ uint8_t *ttl,
        _Out_ uint8_t *exp,
        _Out_ uint8_t *is_uniform)
{
    SWSS_LOG_ENTER();

    /*
     * SAI models the imposed-label treatment per next hop. Both attributes
     * default to UNIFORM, and the explicit TTL/EXP values are only valid in
     * PIPE mode (@validonly in sainexthop.h), so read the mode first and only
     * take the value when it applies. Attributes that were never set come back
     * as ITEM_NOT_FOUND, which leaves the SAI defaults in place.
     */
    sai_attribute_t attr;

    *is_uniform = 1;
    *ttl = MPLS_DEFAULT_OUT_TTL;
    *exp = 0;

    attr.id = SAI_NEXT_HOP_ATTR_OUTSEG_TTL_MODE;
    if (nh_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
        attr.value.s32 == SAI_OUTSEG_TTL_MODE_PIPE) {
        *is_uniform = 0;

        attr.id = SAI_NEXT_HOP_ATTR_OUTSEG_TTL_VALUE;
        if (nh_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
            *ttl = attr.value.u8;
        }
    }

    attr.id = SAI_NEXT_HOP_ATTR_OUTSEG_EXP_MODE;
    if (nh_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
        attr.value.s32 == SAI_OUTSEG_EXP_MODE_PIPE) {
        attr.id = SAI_NEXT_HOP_ATTR_OUTSEG_EXP_VALUE;
        if (nh_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
            *exp = attr.value.u8;
        }
    }
}

sai_status_t SwitchVpp::ensureMplsTable()
{
    SWSS_LOG_ENTER();

    if (m_mpls_table_created) {
        return SAI_STATUS_SUCCESS;
    }

    int ret = mpls_table_add_del(MPLS_TABLE_ID, true);
    if (ret != 0) {
        SWSS_LOG_ERROR("Failed to create VPP MPLS table %u: %d", MPLS_TABLE_ID, ret);
        return SAI_STATUS_FAILURE;
    }

    m_mpls_table_created = true;
    SWSS_LOG_NOTICE("Created VPP MPLS table %u", MPLS_TABLE_ID);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::fillMplsNexthop(
        _In_ const SaiObject *nh_obj,
        _Out_ vpp_mpls_nexthop_t *vnh)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    memset(vnh, 0, sizeof(*vnh));
    vnh->sw_if_index = (uint32_t)~0;   /* let VPP resolve egress recursively by IP */
    vnh->hwif_name = NULL;
    vnh->weight = 1;
    vnh->preference = 0;
    vnh->type = VPP_NEXTHOP_NORMAL;
    vnh->n_labels = 0;

    attr.id = SAI_NEXT_HOP_ATTR_TYPE;
    CHECK_STATUS_W_MSG(nh_obj->get_attr(attr), "MPLS path nexthop missing TYPE");
    int32_t nh_type = attr.value.s32;

    if (nh_type != SAI_NEXT_HOP_TYPE_IP && nh_type != SAI_NEXT_HOP_TYPE_MPLS) {
        SWSS_LOG_ERROR("Unsupported MPLS path nexthop type %d", nh_type);
        return SAI_STATUS_NOT_IMPLEMENTED;
    }

    attr.id = SAI_NEXT_HOP_ATTR_IP;
    CHECK_STATUS_W_MSG(nh_obj->get_attr(attr), "MPLS path nexthop missing IP");
    sai_ip_address_t_to_vpp_ip_addr_t(attr.value.ipaddr, vnh->addr);

    if (nh_type == SAI_NEXT_HOP_TYPE_MPLS) {
        /*
         * LABELSTACK is a list attribute: the caller must supply the output
         * buffer (list pointer + count) before calling get_attr, otherwise the
         * read fails and the out labels are silently dropped (turning a
         * swap/push into a bare pop).
         */
        uint32_t label_buf[VPP_MPLS_MAX_LABELS];
        attr.id = SAI_NEXT_HOP_ATTR_LABELSTACK;
        attr.value.u32list.count = VPP_MPLS_MAX_LABELS;
        attr.value.u32list.list = label_buf;

        sai_status_t label_status = nh_obj->get_attr(attr);

        /*
         * A stack deeper than the buffer yields SAI_STATUS_BUFFER_OVERFLOW with
         * count set to the required size and nothing copied. Fail explicitly:
         * falling through with zero labels would turn a swap/push into a bare
         * pop and silently misforward traffic.
         */
        if (label_status == SAI_STATUS_BUFFER_OVERFLOW) {
            SWSS_LOG_ERROR("MPLS out-label stack of %u labels exceeds maximum %u",
                    attr.value.u32list.count, VPP_MPLS_MAX_LABELS);
            return SAI_STATUS_NOT_SUPPORTED;
        }

        /*
         * Any other failure means the nexthop carries no label stack, which is
         * a valid pop/disposition path.
         */
        if (label_status == SAI_STATUS_SUCCESS && attr.value.u32list.count > 0) {
            uint32_t cnt = attr.value.u32list.count;
            uint8_t out_ttl, out_exp, out_is_uniform;

            getOutsegTtl(nh_obj, &out_ttl, &out_exp, &out_is_uniform);

            vnh->n_labels = (uint8_t)cnt;
            for (uint32_t i = 0; i < cnt; i++) {
                vnh->label_stack[i].label = attr.value.u32list.list[i];
                vnh->label_stack[i].ttl = out_ttl;
                vnh->label_stack[i].exp = out_exp;
                vnh->label_stack[i].is_uniform = out_is_uniform;
            }
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::MplsRouteAddRemove(
        _In_ const SaiObject *inseg_obj,
        _In_ const std::string &serializedObjectId,
        _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    sai_inseg_entry_t inseg_entry;
    sai_deserialize_inseg_entry(serializedObjectId, inseg_entry);

    sai_attribute_t attr;
    int32_t action = SAI_PACKET_ACTION_FORWARD;
    attr.id = SAI_INSEG_ENTRY_ATTR_PACKET_ACTION;
    if (inseg_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        action = attr.value.s32;
    }
    if (action != SAI_PACKET_ACTION_FORWARD) {
        SWSS_LOG_NOTICE("Ignoring inseg label %u: packet action %d is not forward",
                        inseg_entry.label, action);
        return SAI_STATUS_SUCCESS;
    }

    auto nh_obj = inseg_obj->get_linked_object(SAI_OBJECT_TYPE_NEXT_HOP,
                                               SAI_INSEG_ENTRY_ATTR_NEXT_HOP_ID);
    if (!nh_obj) {
        SWSS_LOG_NOTICE("Ignoring inseg label %u: no resolvable nexthop", inseg_entry.label);
        return SAI_STATUS_SUCCESS;
    }

    CHECK_STATUS(ensureMplsTable());

    vpp_mpls_route_t *route = (vpp_mpls_route_t *)
        calloc(1, sizeof(vpp_mpls_route_t) + sizeof(vpp_mpls_nexthop_t));
    if (!route) {
        return SAI_STATUS_FAILURE;
    }
    route->table_id = MPLS_TABLE_ID;
    route->label = inseg_entry.label;
    route->is_multipath = false;
    route->nexthop_cnt = 1;

    sai_status_t status = fillMplsNexthop(nh_obj.get(), &route->nexthop[0]);
    if (status != SAI_STATUS_SUCCESS) {
        free(route);
        return status;
    }

    /*
     * Protocol of the payload exposed once the end-of-stack label is popped.
     * The SAI INSEG entry does not carry it, so derive it from the resolved
     * next hop's address family. Must run after fillMplsNexthop() has populated
     * the address.
     */
    route->eos_proto_af = route->nexthop[0].addr.sa_family;

    bool has_outlabels = (route->nexthop[0].n_labels > 0);

    /*
     * Resolve the next hop's router interface to its VPP egress hwif so the
     * path is programmed as *attached* rather than recursive. This is required
     * for both forwarding cases:
     *   - pop  (disposition): VPP only inserts the MPLS disposition (and thus
     *     honours the uniform LSP mode) for an attached next hop; a recursive
     *     next hop collapses to a plain IP forward and ignores the mode.
     *   - swap/push (imposition): a recursive labelled path fails to resolve
     *     and the packet is dropped at the MPLS DROP DPO.
     * If resolution fails we fall back to the recursive (~0) path.
     */
    std::string egress_hwif;
    {
        sai_attribute_t rif_attr;
        rif_attr.id = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID;
        if (nh_obj->get_attr(rif_attr) == SAI_STATUS_SUCCESS) {
            sai_object_id_t rif_id = rif_attr.value.oid;
            sai_attribute_t port_attr;
            port_attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
            if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &port_attr) == SAI_STATUS_SUCCESS &&
                vpp_get_hwif_name(port_attr.value.oid, 0, egress_hwif)) {
                route->nexthop[0].hwif_name = egress_hwif.c_str();
            }
        }
    }

    if (!has_outlabels) {
        /*
         * Pop/disposition case (no out labels). Inject an implicit-null out
         * label so VPP builds an MPLS disposition rather than leaving the
         * default. The LSP mode follows SAI_INSEG_ENTRY_ATTR_POP_TTL_MODE:
         *
         *   UNIFORM (SAI default) -> is_uniform=1: the popped inner IP TTL is
         *       derived from the MPLS TTL (mpls_ttl - 1), matching typical
         *       hardware.
         *   PIPE                  -> is_uniform=0: the inner TTL is left
         *       untouched and only decremented once by the IP stage.
         *
         * Without the injected label VPP defaults the disposition to PIPE.
         */
        int32_t pop_ttl_mode = SAI_INSEG_ENTRY_POP_TTL_MODE_UNIFORM;
        sai_attribute_t ttl_attr;
        ttl_attr.id = SAI_INSEG_ENTRY_ATTR_POP_TTL_MODE;
        if (inseg_obj->get_attr(ttl_attr) == SAI_STATUS_SUCCESS) {
            pop_ttl_mode = ttl_attr.value.s32;
        }

        route->nexthop[0].n_labels = 1;
        route->nexthop[0].label_stack[0].label = MPLS_IMPLICIT_NULL_LABEL;
        route->nexthop[0].label_stack[0].ttl = 0;
        route->nexthop[0].label_stack[0].exp = 0;
        route->nexthop[0].label_stack[0].is_uniform =
            (pop_ttl_mode == SAI_INSEG_ENTRY_POP_TTL_MODE_PIPE) ? 0 : 1;
    }

    /*
     * The SAI INSEG entry carries a single local label with no End-of-Stack
     * qualifier, but the VPP MPLS FIB is keyed by {label, eos}. Always program
     * the eos=1 entry (single-label / disposition case). For swap/push (out
     * labels present) also program eos=0 so a non-bottom label in a stack is
     * handled (e.g. test_swap_labelstack).
     */
    uint8_t eos_list[2];
    int n_eos = 0;
    if (has_outlabels) {
        eos_list[n_eos++] = 0;
    }
    eos_list[n_eos++] = 1;

    int ret = 0;
    int programmed = 0;
    for (int e = 0; e < n_eos; e++) {
        route->eos = eos_list[e];
        ret = mpls_route_add_del(route, is_add);
        if (ret != 0) {
            break;
        }
        programmed++;
    }

    /*
     * A multi-EOS add that fails part way through would leave an orphaned FIB
     * entry behind that no SAI object refers to, because the caller aborts
     * before recording the route. Undo the entries that did get programmed.
     */
    if (ret != 0 && is_add) {
        for (int e = 0; e < programmed; e++) {
            route->eos = eos_list[e];
            mpls_route_add_del(route, false);
        }
    }

    /*
     * Report the SAI-visible out-label count, not route->nexthop[0].n_labels:
     * the pop case injects an implicit-null label and sets n_labels to 1, which
     * would otherwise log a pop as having one out-label.
     */
    SWSS_LOG_NOTICE("%s inseg label %u out_labels %u status %d",
                    (is_add ? "Add" : "Remove"), inseg_entry.label,
                    (has_outlabels ? route->nexthop[0].n_labels : 0), ret);

    free(route);

    return (ret == 0) ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
}

sai_status_t SwitchVpp::addMplsRoute(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SaiCachedObject inseg_obj(this, SAI_OBJECT_TYPE_INSEG_ENTRY, serializedObjectId, attr_count, attr_list);

    bool route_programmed = false;

    if (is_ip_nbr_active() == true) {
        CHECK_STATUS(MplsRouteAddRemove(&inseg_obj, serializedObjectId, true));
        route_programmed = true;
    }

    sai_status_t status = create_internal(SAI_OBJECT_TYPE_INSEG_ENTRY, serializedObjectId, switch_id, attr_count, attr_list);
    if (status != SAI_STATUS_SUCCESS)
    {
        // The entry was not committed, so undo the VPP programming to keep the
        // MPLS FIB in sync with the SAI object database.
        if (route_programmed) {
            MplsRouteAddRemove(&inseg_obj, serializedObjectId, false);
        }
        return status;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeMplsRoute(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    auto inseg_obj = get_sai_object(SAI_OBJECT_TYPE_INSEG_ENTRY, serializedObjectId);
    if (inseg_obj && is_ip_nbr_active() == true) {
        CHECK_STATUS(MplsRouteAddRemove(inseg_obj.get(), serializedObjectId, false));
    }

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_INSEG_ENTRY, serializedObjectId));

    return SAI_STATUS_SUCCESS;
}
