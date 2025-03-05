#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nexthop_grp_member_ {
    sai_ip_address_t addr;
    sai_object_id_t rif_oid;
    uint32_t weight;
    uint32_t seq_id;
    uint32_t sw_if_index;
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

