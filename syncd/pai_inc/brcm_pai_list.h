/*
 *
 * $Id: brcm_pai_list.h $
 *
 * $Copyright: (c) 2021 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 *
 */

#ifndef __PAI_LIST_H_
#define __PAI_LIST_H_

#include "brcm_pai_db.h"

typedef struct _pai_link_s
{
    void *data;
    struct _pai_link_s *next;
    struct _pai_link_s *prev;
} pai_link_t;

typedef struct _pai_list_s
{
    pai_link_t *list_head;
    pai_link_t *list_tail;
    pai_link_t *list_wrk_ptr;
    unsigned int list_element_cnt;
} pai_list_t;

int pai_list_init(pai_module_type_t module_list_id);
int pai_list_insert(pai_module_type_t module_list_id, void *data);
int pai_list_remove(pai_module_type_t module_list_id, sai_object_id_t object_id,
    void **data);
int pai_list_get_root_node(pai_module_type_t module_list_id, pai_list_t
    *pai_list_root_node);
int pai_list_search(pai_module_type_t module_list_id, sai_object_id_t object_id);
int pai_list_get_item(pai_module_type_t module_list_id,
    sai_object_id_t object_id, void **data);
int pai_list_free(pai_module_type_t module_list_id);
int pai_list_get_next_item(pai_module_type_t module_list_id, void *start, void **data);
int pai_list_print(pai_module_type_t module_list_id);
#endif
