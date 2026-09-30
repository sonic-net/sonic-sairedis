#pragma once

#include "vppxlate/SaiVppXlate.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nexthop_grp_member_ {
    sai_ip_address_t addr;
    sai_object_id_t rif_oid;
    uint32_t weight;
    uint32_t seq_id;
    uint32_t sw_if_index;
    uint8_t n_labels;
    uint32_t label_stack[VPP_MPLS_MAX_LABELS];
    /* Imposed-label treatment; SAI carries this per next hop, not per label. */
    uint8_t out_ttl;
    uint8_t out_exp;
    uint8_t out_is_uniform;
} nexthop_grp_member_t;

typedef struct nexthop_grp_config_ {
    int32_t grp_type;
    uint32_t nmembers;

    /* Must be the last variable */
    nexthop_grp_member_t grp_members[0];
} nexthop_grp_config_t;

#ifdef __cplusplus
}
#endif

