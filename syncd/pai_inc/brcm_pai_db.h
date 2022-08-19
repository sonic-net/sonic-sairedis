/*
 *
 * $Id: brcm_pai_db.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 *
 */

#ifndef __PAI_DB_H_
#define __PAI_DB_H_

#include "saitypes.h"
#include "brcm_pai_common.h"

#define MODULE_WB_FILE_PATH "/var/tmp/wb"
#define MODULE_FILE_NAME_MAX 32

#define ALLOC_FLAG_WB_DISABLE  0
#define ALLOC_FLAG_WB_ENABLE   1
#define ALLOC_FLAG_WB_RESTORE  2
#define ALLOC_FLAG_WB_RESTORED 3

extern int alloc_flag;

typedef enum pai_module_type_e {
    PAI_MODULE_PHY = 0,
    PAI_MODULE_PORT_LINE,
    PAI_MODULE_PORT_SYS,
    PAI_MODULE_PORTC,
    PAI_MODULE_SERDES,
    PAI_MODULE_MACSEC,
    PAI_MODULE_MACSEC_PORT,
    PAI_MODULE_MACSEC_FLOW,
    PAI_MODULE_MACSEC_SC,
    PAI_MODULE_MACSEC_SA,
    PAI_MODULE_MACSEC_ACL_TABLE,
    PAI_MODULE_MACSEC_ACL_ENTRY,
    PAI_MODULE_CFG_PHY,
    PAI_MODULE_CFG_LANE,
    PAI_MODULE_CFG_PORT_LINE,
    PAI_MODULE_CFG_PORT_SYS,
    PAI_MODULE_MAX
} pai_module_type_t;

typedef struct pai_phy_s {
    sai_object_id_t phy_obj_id;
    /* Static configuration read from the Config File */
    int phy_id;
    int phy_reverse_mode;
    int macsec_mode;
    int pll1_vco;
    int tx_drv_supply;
    char part_number[BRCM_MAX_CHAR_CHIP_NAME];
    /* Dynamic configuration options with end with _current*/
} pai_phy_t;

typedef struct pai_phy_init_s {
    sai_object_id_t phy_obj_id;
    void *platform_ctxt;
    int phy_id;
    sai_status_t (*pai_switch_register_read)(
            uint64_t platform_context,
            uint32_t device_addr,
            uint32_t start_reg_addr,
            uint32_t number_of_registers,
            uint32_t *reg_val);
    sai_status_t (*pai_switch_register_write)(uint64_t platform_context,
            uint32_t device_addr,
            uint32_t start_reg_addr,
            uint32_t number_of_registers,
            const uint32_t *reg_val);
    bool switch_inited;
    char chip_name[BRCM_MAX_CHAR_CHIP_NAME];
    bcm_plp_avs_config_t avs_config;
    sai_switch_switching_mode_t switching_mode;
    sai_status_t (*pai_sync_lock)(uint64_t platform_ctxt);
    sai_status_t (*pai_sync_unlock)(uint64_t platform_ctxt);
    bool wb_recover;
    boot_flags_t boot_flags;  /* SAI boot up flags */
    int port_idx;
    int serdes_port_idx;
    int port_connector_idx;
    int acl_table_idx;
    int acl_entry_idx;
    int sc_idx;
    int master_port_id;
} pai_phy_init_t;

typedef struct pai_lane_s {
    /* This lane_obj_id will not be associated with any lane object
     * but it will be created by the database for retrieval purposes*/
    sai_object_id_t lane_obj_id;
    int phy_id;
    int interface_side;
    int local_lane_number;
    int tx_lanemap;
    int rx_lanemap;
    int tx_polarity;
    int rx_polarity;
    int global_lane_number;
} pai_lane_t;

typedef struct pai_cfg_port_s {
    sai_object_id_t port_id;
    int unique_port_id;
    int speed;
    int interface_side;
    int no_of_lanes;
    int8_t lane_list[BRCM_MAX_LANE_PER_PORT];
} pai_cfg_port_t;

typedef struct pai_macsec_s {
    sai_object_id_t macsec_obj_id;
} pai_macsec_t;

typedef struct _pai_obj_size_s {
    pai_module_type_t pai_module_id;
    uint16_t pai_obj_size;
} pai_obj_size_t;

typedef struct _brcm_pai_wb_s {
    FILE *module_fd[PAI_MODULE_MAX];
    char module_file_name[PAI_MODULE_MAX][MODULE_FILE_NAME_MAX];
} brcm_pai_wb_t;

/* Functions */
int pai_db_init(pai_init_t *init);
int pai_db_alloc_module(pai_module_type_t module_id, sai_object_id_t object_id,
        void *data_p);
int pai_db_read_module(pai_module_type_t module_id, sai_object_id_t object_id,
        void *data_p);
int pai_db_write_module(pai_module_type_t module_id, sai_object_id_t object_id,
        void *data_p);
int pai_db_free_module(pai_module_type_t module_id, sai_object_id_t object_id);
int pai_db_free(void);
int pai_db_get_next_item(pai_module_type_t module_id, void *start, void **data_p);
int pai_db_build_module_list(pai_module_type_t module_id, sai_object_id_t object_id,
    uint32_t attr_count, sai_attribute_t *attr_list);
int pai_db_print_list(pai_module_type_t module_id);
int pai_db_get_obj_size(pai_module_type_t pai_module_id);
sai_status_t
pai_db_get_next_persistent_obj_id(pai_module_type_t pai_module_id, sai_object_id_t switch_id, int *obj_id);

#endif
