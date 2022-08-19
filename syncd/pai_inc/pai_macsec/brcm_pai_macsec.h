/*
 *
 * $Id: brcm_pai_macsec.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 *
 */

#ifndef BRCM_PAI_MACSEC_H
#define BRCM_PAI_MACSEC_H
#include <epdm_sec.h>
#define BRCM_PAI_CREATE_MACSEC_ID(CHIP_ID,MACSEC_SIDE,PHY_ID)  BRCM_PAI_CREATE_OBJ_ID(SAI_OBJECT_TYPE_MACSEC,CHIP_ID,MACSEC_SIDE,PHY_ID)
#define BRCM_PAI_CREATE_MACSEC_PORT_ID(CHIP_ID,MACSEC_SIDE,IDX)  BRCM_PAI_CREATE_OBJ_ID(SAI_OBJECT_TYPE_MACSEC_PORT,CHIP_ID,MACSEC_SIDE,IDX)
#define BRCM_PAI_CREATE_MACSEC_FLOW_ID(CHIP_ID,MACSEC_SIDE,PHY_ADDR,IDX)  BRCM_PAI_CREATE_OBJ_ID(SAI_OBJECT_TYPE_MACSEC_FLOW,CHIP_ID,MACSEC_SIDE,(PHY_ADDR<<16 | IDX))
#define BRCM_PAI_CREATE_MACSEC_SC_ID(CHIP_ID,MACSEC_SIDE,SCID)  BRCM_PAI_CREATE_OBJ_ID(SAI_OBJECT_TYPE_MACSEC_SC,CHIP_ID,MACSEC_SIDE, SCID)
#define BRCM_PAI_CREATE_MACSEC_SA_ID(CHIP_ID,MACSEC_SIDE,V_IDX,SA_IDX)  BRCM_PAI_CREATE_OBJ_ID(SAI_OBJECT_TYPE_MACSEC_SA,CHIP_ID,MACSEC_SIDE,(V_IDX<<16 | SA_IDX))
#define BRCM_PAI_CREATE_ACL_TABLE_ID(CHIP_ID,MACSEC_SIDE,IDX)  BRCM_PAI_CREATE_OBJ_ID(SAI_OBJECT_TYPE_ACL_TABLE,CHIP_ID,MACSEC_SIDE,IDX)
#define BRCM_PAI_CREATE_ACL_ENTRY_ID(CHIP_ID,MACSEC_SIDE,IDX)  BRCM_PAI_CREATE_OBJ_ID(SAI_OBJECT_TYPE_ACL_ENTRY,CHIP_ID,MACSEC_SIDE,IDX)

#define TRANSREC_INGRESS_SIZE 20
#define TRANSREC_EGRESS_SIZE 24
#define MAX_RULE 1024
#define MAX_SC   512
#define MAX_SA_PER_SC 4

typedef struct pai_sec_info_s {
        char chip_name[BRCM_MAX_CHAR_CHIP_NAME];
        sai_object_id_t switch_id;
        bcm_plp_access_t phy_info;
        bcm_plp_sec_mutex_t mutex;  /**< mutex lock for CfyE and SecY APIs */
        unsigned int macsec_side;
} pai_sec_info_t;
typedef struct pai_object_list_id_s
{
    sai_object_id_t object_id;
    struct pai_object_list_id_s *next;
} pai_object_list_id_t;

typedef struct pai_object_list_s
{
    pai_object_list_id_t *object_list_head;
    uint32_t count;
} pai_object_list_t;

/* pai object list functions     */
int pai_add_object_to_list(pai_object_list_t *object_list, sai_object_id_t object_id);
int pai_remove_object_from_list(pai_object_list_t *object_list, sai_object_id_t object_id);
int pai_fill_sai_object_list(pai_object_list_t *pai_object_list, sai_object_list_t *object_list);
int pai_free_object_list(pai_object_list_t *object_list);


typedef struct pai_macsec_object_s {
        sai_object_id_t macsec_id;
        sai_object_id_t switch_id;
        sai_uint16_t   ctag_tpid;
        sai_uint16_t   stag_tpid;
        sai_uint8_t max_vlan_tag_parsed;
        sai_stats_mode_t stats_mode;
        unsigned char bypass_enable;
        unsigned char warmboot_enable;
 } pai_macsec_object_t;

typedef struct pai_macsec_port_s {
        sai_object_id_t macsec_port_id;
        sai_object_id_t  line_port_id;
        sai_object_id_t switch_id;
        sai_object_id_t macsec_id;
        bool ctag_enable;
        bool stag_enable;
} pai_macsec_port_t;

typedef struct pai_macsec_flow_s {
        sai_object_id_t macsec_flow_id;
        sai_object_id_t macsec_id;
        sai_object_id_t switch_id;
        bcm_plp_cfye_vport_handle_t vport_handle;
        int lane_map;
 } pai_macsec_flow_t;

typedef struct pai_acl_table_s {
    sai_object_id_t acl_table_id;
    sai_object_id_t line_port_id;
    sai_acl_stage_t  acl_stage;
    sai_acl_bind_point_type_t bind_point;
    sai_acl_action_type_t action_type;
    bool match_dst_mac;
    bool match_ether_type;
    bool has_vlan_tag;
    bool match_field_vlan_tag;
    bool match_macsec_sci;
    bool match_outer_vlan_id;
    bool match_outer_vlan_pri;
    bool match_inner_vlan_id;
    bool match_inner_vlan_pri;
    bool match_inner_vlan_cfi;
    bool match_outer_vlan_cfi;
} pai_acl_table_t;

typedef struct pai_acl_table_entry_s {
    sai_object_id_t acl_entry_id;
    sai_object_id_t acl_table_id;
    sai_object_id_t line_port_id;
    sai_acl_action_data_t macsec_flow_action;
    sai_object_id_t switch_id;
    bcm_plp_cfye_rule_handle_t rule_handle;
    uint32_t rule_index;
    bcm_plp_cfye_vport_handle_t ctrl_pkt_vport;
    bcm_plp_secy_sa_handle_t ctrl_pkt_sahandle;
    sai_uint32_t acl_entry_priority;
    bool  admin_state;
    sai_acl_field_data_t ether_type;
    uint16_t no_of_vlan_tags;
    bool has_vlan_tag;
    sai_acl_field_data_t outer_vlan_id;
    uint8_t outer_vlan_priority;
    sai_acl_field_data_t inner_vlan_id;
    uint8_t inner_vlan_priority;
    uint8_t inner_vlan_cfi;
    uint8_t outer_vlan_cfi;
    sai_acl_field_data_t sci;
    sai_acl_action_data_t pkt_action;
    sai_acl_field_data_t src_mac;
    sai_acl_field_data_t dst_mac;
} pai_acl_table_entry_t;

typedef struct pai_macsec_sc_s {
        sai_object_id_t  macsec_sc_id;
        sai_object_id_t  macsec_flow_id;
        sai_object_id_t  macsec_id;
        sai_object_id_t switch_id;
        uint64_t   sci;
        bool xpn64_enable;
        sai_macsec_cipher_suite_t cipher_suite_type;
        bool encryption_enable;
        bool sci_enable;
        sai_uint8_t  sectag_offset;
        sai_object_id_t active_sa_id;
        bool replay_protection_enable;
        sai_uint32_t replay_protection_window;
} pai_macsec_sc_t;

typedef struct pai_macsec_sa_s {
        sai_object_id_t   macsec_sa_id;
        sai_object_id_t   sc_id;
        sai_object_id_t   macsec_id;
        sai_object_id_t switch_id;
        bcm_plp_secy_sa_handle_t sa_handle;
        sai_uint8_t sa_an;
        bool sak_256_bits;    /* false means 128-bit key */
        uint32_t ssci;
        bool encryption_enable;
        sai_macsec_sak_t  sak_key;
        sai_macsec_salt_t   salt_key;
        sai_macsec_auth_key_t  auth_key;
        sai_uint64_t egress_xpn;
        sai_uint64_t ingress_xpn;
} pai_macsec_sa_t;

int _brcm_pai_macsec_info_to_sec_info(const pai_sec_info_t *pai_sec_info,
                                              bcm_plp_sec_phy_access_t *sec_info);
sai_status_t brcm_pai_bind_port_acl_table(sai_object_id_t port_id,
                                               sai_object_id_t acl_table_id);
sai_status_t brcm_pai_get_macsec_port_stats(
        _In_ sai_object_id_t macsec_port_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters);
sai_status_t brcm_pai_get_macsec_sa_stats(
        _In_ sai_object_id_t macsec_sa_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters);
sai_status_t brcm_pai_get_macsec_sc_stats(
        _In_ sai_object_id_t macsec_sc_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters);
sai_status_t brcm_pai_get_macsec_flow_stats(
        _In_ sai_object_id_t macsec_flow_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters);

int brcm_pai_warmboot_macsec_shutdown(_In_ sai_object_id_t switch_id);
#endif   /* End BRCM_PAI_MACSEC_H */
