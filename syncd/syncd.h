#ifndef __SYNCD_H__
#define __SYNCD_H__

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

#include "string.h"
extern "C" {
#include "sai.h"
}

#include "common/saiserialize.h"
#include "common/saiattributelist.h"
#include "swss/dbconnector.h"
#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/select.h"
#include "swss/scheme.h"
#include "swss/logger.h"
#include "swss/table.h"
#include "swss/counter.h"

#define UNREFERENCED_PARAMETER(X)

#define SYNCD_LOG_ERR(MSG, ...)         swss::Logger::getInstance().write(swss::Logger::SWSS_ERROR, MSG, ##__VA_ARGS__)
#define SYNCD_LOG_WRN(MSG, ...)         swss::Logger::getInstance().write(swss::Logger::SWSS_WARN, MSG, ##__VA_ARGS__)
#define SYNCD_LOG_NTC(MSG, ...)         swss::Logger::getInstance().write(swss::Logger::SWSS_NOTICE, MSG, ##__VA_ARGS__)
#define SYNCD_LOG_INF(MSG, ...)         swss::Logger::getInstance().write(swss::Logger::SWSS_INFO, MSG, ##__VA_ARGS__)
#define SYNCD_LOG_DBG(MSG, ...)         swss::Logger::getInstance().write(swss::Logger::SWSS_DEBUG, MSG, ##__VA_ARGS__)
#define SYNCD_LOG_ENTER()   		SYNCD_LOG_DBG("%s: >", __FUNCTION__)
#define SYNCD_LOG_EXIT()    		SYNCD_LOG_DBG("%s: <", __FUNCTION__)

sai_object_id_t replaceVidToRid(const sai_object_id_t &virtual_object_id);

extern swss::ConsumerTable *getRequest;
extern swss::ProducerTable *getResponse;
extern swss::ProducerTable *notifications;

extern swss::Table *g_vidToRid;
extern swss::Table *g_ridToVid;

extern swss::Counter *g_vidCounter;

sai_object_id_t translate_vid_to_rid(
        _In_ sai_object_id_t vid);

void translate_vid_to_rid(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

template <typename T>
void translate_list_vid_to_rid(
        _In_ T &element)
{
    for (uint32_t i = 0; i < element.count; i++)
    {
        element.list[i] = translate_vid_to_rid(element.list[i]);
    }
}

void translate_rid_to_vid(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

typedef sai_status_t (*create_fn)(
        _Out_ sai_object_id_t *stp_id,
        _In_  uint32_t attr_count,
        _In_  const sai_attribute_t *attr_list);

typedef sai_status_t (*remove_fn)(
        _In_ sai_object_id_t stp_id);

typedef sai_status_t (*set_attribute_fn)(
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr);

typedef sai_status_t (*get_attribute_fn)(
        _In_ sai_object_id_t stp_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list);

extern create_fn           common_create[SAI_OBJECT_TYPE_MAX];
extern remove_fn           common_remove[SAI_OBJECT_TYPE_MAX];
extern set_attribute_fn    common_set_attribute[SAI_OBJECT_TYPE_MAX];
extern get_attribute_fn    common_get_attribute[SAI_OBJECT_TYPE_MAX];

extern sai_acl_api_t                *sai_acl_api;
extern sai_buffer_api_t             *sai_buffer_api;
extern sai_fdb_api_t                *sai_fdb_api;
extern sai_hash_api_t               *sai_hash_api;
extern sai_hostif_api_t             *sai_hostif_api;
extern sai_lag_api_t                *sai_lag_api;
extern sai_mirror_api_t             *sai_mirror_api;
extern sai_neighbor_api_t           *sai_neighbor_api;
extern sai_next_hop_api_t           *sai_next_hop_api;
extern sai_next_hop_group_api_t     *sai_next_hop_group_api;
extern sai_policer_api_t            *sai_policer_api;
extern sai_port_api_t               *sai_port_api;
extern sai_qos_map_api_t            *sai_qos_map_api;
extern sai_queue_api_t              *sai_queue_api;
extern sai_route_api_t              *sai_route_api;
extern sai_router_interface_api_t   *sai_router_interface_api;
extern sai_samplepacket_api_t       *sai_samplepacket_api;
extern sai_scheduler_api_t          *sai_scheduler_api;
extern sai_scheduler_group_api_t    *sai_scheduler_group_api;
extern sai_stp_api_t                *sai_stp_api;
extern sai_switch_api_t             *sai_switch_api;
extern sai_udf_api_t                *sai_udf_api;
extern sai_virtual_router_api_t     *sai_router_api;
extern sai_vlan_api_t               *sai_vlan_api;
extern sai_wred_api_t               *sai_wred_api;

extern sai_switch_notification_t switch_notifications;

extern swss::Table *vidToRid;

void initialize_common_api_pointers();
void populate_sai_apis();

#endif // __SYNCD_H__
