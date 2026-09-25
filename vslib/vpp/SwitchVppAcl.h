#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cap on the attributes read back per ACL entry. The attribute store is keyed
 * by attribute name, so an entry above the cap loses attributes in name order,
 * PRIORITY and TABLE_ID first. Keep it above the widest entry orchagent builds.
 */
#define MAX_ACL_ATTRS 20

/*
 * SAI HLD restricts SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_{INGRESS,EGRESS} to a single
 * session OID per rule; size > 1 so a slightly larger source list does not trip
 * BUFFER_OVERFLOW and drop the attribute in get_max().
 */
#define MAX_ACL_MIRROR_OIDS 4

/*
 * One SAI entry expands into (ingress ports x IP families x L4 protocols) VPP
 * rules. Past this many we warn: the expansion is legal but grows the
 * acl_add_replace message.
 */
#define ACL_MAX_RULES_PER_ACE 64
typedef struct _acl_tbl_entries_ {
    uint32_t priority;

    sai_attribute_t  attr_range;
    sai_object_id_t  range_objid_list[2];

    sai_u32_range_t range_limit[2];
    sai_acl_range_type_t range_type[2];
    uint32_t range_count;
    sai_attribute_t attrs[MAX_ACL_ATTRS];
    uint32_t attrs_count;

    /*
     * Backing storage for the MIRROR_INGRESS / MIRROR_EGRESS object lists.
     * get_max() copies through transfer_list(), which leaves dst.list = NULL
     * when dst.count was 0 on entry, so these are re-fetched into sized buffers.
     */
    sai_object_id_t mirror_ingress_objid_list[MAX_ACL_MIRROR_OIDS];
    sai_object_id_t mirror_egress_objid_list[MAX_ACL_MIRROR_OIDS];
} acl_tbl_entries_t;

typedef struct ordered_ace_list_ {
    uint32_t index;
    uint32_t priority;
    sai_object_id_t ace_oid;
    bool is_tunterm;
    // each ACE in SONiC maps to one or more VPP consequential ACL rules
    // vpp_rule_base_index is the starting index of these rules in VPP ACL
    uint32_t vpp_rule_base_index;
    // number of VPP ACL rules created for this ACE
    uint32_t num_rules;
} ordered_ace_list_t;

#ifdef __cplusplus
}
#endif

