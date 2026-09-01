#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ACL_ATTRS 12

typedef struct _acl_tbl_entries_ {
    uint32_t priority;

    sai_attribute_t  attr_range;
    sai_object_id_t  range_objid_list[2];

    sai_u32_range_t range_limit[2];
    sai_acl_range_type_t range_type[2];
    uint32_t range_count;
    sai_attribute_t attrs[MAX_ACL_ATTRS];
    uint32_t attrs_count;
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

