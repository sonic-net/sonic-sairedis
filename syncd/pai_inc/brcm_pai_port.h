/*
 *
 * $Id: brcm_pai_port.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$ 
 *
 *
 */

#if !defined (__BRCM_PAI_PORT_H_)
#define __BRCM_PAI_PORT_H_

#include "saitypes.h"
#include "saiport.h"
#include "saistatus.h"
#include "brcm_pai_extensions.h"

#define  BRCM_MAX_LANE_PER_PORT    16
#define  BRCM_MAX_CHAR_CHIP_NAME   20
#define  BRCM_MAX_LIST_MAX_ELEMENT 10

typedef struct brcm_pai_u32_list_s
{
    uint32_t count;
    uint32_t list[BRCM_MAX_LIST_MAX_ELEMENT];
} brcm_pai_u32_list_t;

typedef struct brcm_pai_s32_list_s
{
    uint32_t count;
    int32_t list[BRCM_MAX_LIST_MAX_ELEMENT];
} brcm_pai_s32_list_t;

typedef struct brcm_pai_prbs_s {
    uint32_t enable;
    uint32_t poly;
    uint32_t tx_rx;
} brcm_pai_prbs_t;

typedef struct brcm_pai_prbs_status_s {
    uint32_t lock_status;
    uint32_t lock_loss_status;
    uint32_t error_counter;
} brcm_pai_prbs_status_t;

typedef struct brcm_pai_lt_status_s {
    uint32_t enable;
    uint32_t fail_status;
    uint32_t rx_status;
} brcm_pai_lt_status_t;

typedef struct brcm_pai_synce_cfg_s {
    int32_t clk_gen_squelch_cfg;
    int32_t recovered_clk_lane;
    sai_u32_list_t squelch_monitor_lanes;
    int32_t synce_divider;
    int32_t rclk_out_pin_sel;
    int32_t rclk_if_side;
} brcm_pai_synce_cfg_t;

/* Copper PHY attributes */
typedef struct pai_port_cu_attributes_s {
    sai_port_mdix_mode_config_t mdix_mode;
    brcm_pai_port_auto_neg_mode_t auto_neg_mode;
    brcm_pai_port_power_mode_t power_mode;
    sai_uint32_t eee_latency_value;
    sai_uint32_t eee_idle_threshold;
    brcm_pai_u32_list_t advert_speed_hd;
    sai_port_flow_control_mode_t advert_flow_ctrl;
    bool autogrEEEn_enable;
    bool eee_enable;
    bool eee_latency_mode;
    bool super_isolate;
    bool auto_detect;
    bool auto_neg_serdes;
    bool auto_medium;
    bool media_preferred;
    bool speed_2pair_mode;
    bool full_duplex_mode;
    sai_port_module_type_t module_type;
    brcm_pai_port_cable_type_t cable_type;
} pai_port_cu_attributes_t;


typedef struct pai_port_attributes_s {
    uint32_t hw_lane_list[BRCM_MAX_LANE_PER_PORT];
    sai_uint32_t speed;
    bool admin_state;
    sai_port_media_type_t media_type;
    sai_port_interface_type_t interface_type;
    sai_port_loopback_mode_t loopback_mode;

    bool use_extended_fec;
    sai_port_fec_mode_t fec_mode;
    sai_port_fec_mode_extended_t fec_mode_extended;
    bool enablelinktraining;
    sai_port_flow_control_mode_t flow_ctrl;

    /* Autoneg attributes*/
    bool autoneg_mode;
    brcm_pai_u32_list_t advert_speed;
    brcm_pai_s32_list_t advert_fec_mode;
    brcm_pai_s32_list_t advert_fec_mode_extended;
    bool advert_pause_asymmetric;
    sai_port_media_type_t advert_media_type;
    brcm_pai_prbs_t prbs;

    /*Aux mode attribute */
    unsigned int low_latency_variation;
    sai_uint64_t ref_clk;
    /* synce attributes */
    brcm_pai_synce_cfg_t synce_cfg;
    /* Failover config lane_masp */
    unsigned int failover_lane_map;
    /* Copper PHY attributes */
    pai_port_cu_attributes_t cu_attr;
} pai_port_attributes_t;

typedef struct pai_port_info_s {
    sai_object_id_t port_id ;
    char chip_name[BRCM_MAX_CHAR_CHIP_NAME];
    int lane_map;
    uint32_t no_of_lane;
    int tx_polarity;
    int rx_polarity;
    sai_object_id_t switch_id;
    int is_active;
    pai_port_attributes_t port_attributes;
#ifdef PLP_MACSEC_SUPPORT
    sai_object_id_t ingress_macsec_acl;
    sai_object_id_t egress_macsec_acl;
#endif
} pai_port_info_t;

typedef struct pai_port_connector_s {
     sai_object_id_t port_connector_id;
     sai_object_id_t line_port_id;
     sai_object_id_t sys_port_id;
     sai_object_id_t sys_failover_port_id;
     sai_object_id_t line_failover_port_id;
     sai_object_id_t switch_id;
} pai_port_connector_t;

typedef struct pai_port_serdes_s {
     sai_object_id_t port_serdes_id;
    /* Represents the line/sys port ID in which serdes port is associated*/
    sai_object_id_t port_id;
} pai_serdes_port_t;

sai_status_t 
_brcm_process_hw_lane_list(sai_object_id_t switch_id, pai_port_info_t *port_info, int *interface_side);
int _brcm_pai_port_info_phy_info(const pai_port_info_t *port_info,
    bcm_plp_access_t *plp_info);
sai_status_t brcm_pai_get_port_attribute(_In_ sai_object_id_t port_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list);
sai_status_t brcm_pai_set_port_attribute(_In_ sai_object_id_t port_id,
    _In_ const sai_attribute_t *attr);

#define BRCM_PAI_PORT_FORM_OBJECT(C_ID,IF_SIDE,OBJ_NO)  (((sai_uint64_t)SAI_OBJECT_TYPE_PORT << 56)|((sai_uint64_t)C_ID << 36)|((sai_uint64_t)IF_SIDE << 32)|((sai_uint64_t)OBJ_NO))
#define BRCM_PAI_PORT_CON_FORM_OBJECT(C_ID,OBJ_NO)  (((sai_uint64_t)SAI_OBJECT_TYPE_PORT_CONNECTOR << 56)|((sai_uint64_t)C_ID << 36)|((sai_uint64_t)OBJ_NO))
#define BRCM_PAI_SRDS_PORT_FORM_OBJECT(C_ID,IF_SIDE,OBJ_NO)  (((sai_uint64_t)SAI_OBJECT_TYPE_PORT_SERDES << 56)|((sai_uint64_t)C_ID << 36)|((sai_uint64_t)IF_SIDE << 32)|((sai_uint64_t)OBJ_NO))

#endif /*__PAI_PORT_H_*/
