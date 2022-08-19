/*
 *
 * $Id: brcm_pai_wb.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 *
 */

#ifndef __BRCM_PAI_WB_H_
#define __BRCM_PAI_WB_H_

#include "brcm_pai_db.h"

sai_status_t brcm_pai_wb_init(pai_init_t *init);
sai_status_t brcm_pai_wb_restore(pai_init_t *init);
int brcm_pai_wb_alloc_module(pai_module_type_t module_id, sai_object_id_t object_id,
    void *data_p, int data_size);
int brcm_pai_wb_write_module(pai_module_type_t module_id, sai_object_id_t object_id,
    void *data_p, int data_size);
int brcm_pai_wb_read_module(pai_module_type_t module_id, sai_object_id_t object_id,
    void *data_p, int data_size);
int brcm_pai_wb_free_module(pai_module_type_t module_id, sai_object_id_t object_id,
    int data_size);
int brcm_pai_wb_print_all_objects(void);
#endif

